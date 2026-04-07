#include "uart_handler.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "easycon.h"
#include "tinyusb.h"

void uart_handler_init(void) {
    uart_config_t uart_config = {
        .baud_rate = EASYCON_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(EASYCON_UART_NUM, &uart_config);
    uart_set_pin(EASYCON_UART_NUM, EASYCON_UART_TX_PIN, EASYCON_UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(EASYCON_UART_NUM, 1024, 0, 0, NULL, 0);
}

void uart_handler_send(uint8_t byte) {
    uart_write_bytes(EASYCON_UART_NUM, &byte, 1);
    // Also send through CDC if available
    if (tud_cdc_connected()) {
        tud_cdc_write_char(byte);
        tud_cdc_write_flush();
    }
}
