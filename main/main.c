#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "easycon.h"
#include "hid_device.h"
#include "uart_handler.h"
#include "storage.h"
#include "tinyusb.h"

static const char *TAG = "EasyMCU";

static TaskHandle_t s_uart_rx_task_handle = NULL;
static TaskHandle_t s_cdc_rx_task_handle = NULL;
static TaskHandle_t s_hid_tx_task_handle = NULL;
static TaskHandle_t s_script_task_handle = NULL;

static void uart_rx_task(void *pvParameters) {
    (void)pvParameters;
    uint8_t byte;
    for (;;) {
        int len = uart_read_bytes(EASYCON_UART_NUM, &byte, 1, pdMS_TO_TICKS(10));
        if (len > 0) {
            EasyCon_serial_task((int16_t)byte);
        }
    }
}

static void cdc_rx_task(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        if (tud_cdc_available()) {
            uint8_t byte = tud_cdc_read_char();
            EasyCon_serial_task((int16_t)byte);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void hid_tx_task(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        if (EasyCon_need_send_report()) {
            hid_device_send();
            EasyCon_report_send_callback();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void script_task(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        EasyCon_script_task();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void tick_task(void *pvParameters) {
    (void)pvParameters;
    TickType_t last_tick = xTaskGetTickCount();
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        if (now != last_tick) {
            EasyCon_tick();
            last_tick = now;
        }
        vTaskDelay(1);
    }
}

// Custom log output function for UART1
static int uart1_log_output(const char* format, va_list args) {
    char stack_buffer[256];
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(stack_buffer, sizeof(stack_buffer), format, args_copy);
    va_end(args_copy);

    if (len <= 0) {
        return len;
    }

    if ((size_t)len < sizeof(stack_buffer)) {
        uart_write_bytes(UART_NUM_1, stack_buffer, len);
        return len;
    }

    size_t buffer_size = (size_t)len + 1;
    char *heap_buffer = (char *)malloc(buffer_size);
    if (heap_buffer == NULL) {
        uart_write_bytes(UART_NUM_1, stack_buffer, sizeof(stack_buffer) - 1);
        return len;
    }

    va_copy(args_copy, args);
    vsnprintf(heap_buffer, buffer_size, format, args_copy);
    va_end(args_copy);

    uart_write_bytes(UART_NUM_1, heap_buffer, len);
    free(heap_buffer);
    return len;
}

static void uart1_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, GPIO_NUM_17, GPIO_NUM_18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // Redirect log output to UART1
    esp_log_set_vprintf(uart1_log_output);
}

void app_main(void) {
    // Initialize UART1 for debug log    
    uart1_init();
    
    ESP_LOGI(TAG, "EasyMCU ESP32-S3 starting");

    ESP_ERROR_CHECK(storage_init());
    ESP_LOGI(TAG, "Storage initialized");

    uart_handler_init();
    ESP_LOGI(TAG, "UART handler initialized");

    hid_device_init();
    ESP_LOGI(TAG, "HID device initialized");

    EasyCon_script_init();
    ESP_LOGI(TAG, "EasyCon script initialized");

    xTaskCreatePinnedToCore(tick_task, "tick", 2048, NULL, 6, NULL, 0);
    xTaskCreatePinnedToCore(uart_rx_task, "uart_rx", 4096, NULL, 5, &s_uart_rx_task_handle, 0);
    xTaskCreatePinnedToCore(cdc_rx_task, "cdc_rx", 4096, NULL, 5, &s_cdc_rx_task_handle, 0);
    xTaskCreatePinnedToCore(hid_tx_task, "hid_tx", 4096, NULL, 4, &s_hid_tx_task_handle, 1);
    xTaskCreatePinnedToCore(script_task, "script", 8192, NULL, 3, &s_script_task_handle, 1);
    ESP_LOGI(TAG, "Tasks created");

    EasyCon_script_auto_start();

    ESP_LOGI(TAG, "EasyMCU initialized and ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
