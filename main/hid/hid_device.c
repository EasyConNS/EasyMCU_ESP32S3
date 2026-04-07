#include "hid_device.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include "driver/gpio.h"

static const uint8_t switch_pro_hid_report_descriptor[] = {
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     ),
    HID_USAGE      ( HID_USAGE_DESKTOP_GAMEPAD  ),
    HID_COLLECTION ( HID_COLLECTION_APPLICATION ),
        HID_USAGE_PAGE   ( HID_USAGE_PAGE_BUTTON ),
        HID_USAGE_MIN    ( 0x01 ),
        HID_USAGE_MAX    ( 0x10 ),
        HID_LOGICAL_MIN  ( 0 ),
        HID_LOGICAL_MAX  ( 1 ),
        HID_REPORT_COUNT ( 16 ),
        HID_REPORT_SIZE  ( 1 ),
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
        HID_USAGE_PAGE   ( HID_USAGE_PAGE_DESKTOP ),
        HID_LOGICAL_MAX  ( 7 ),
        HID_PHYSICAL_MAX_N ( 315, 2 ),
        HID_REPORT_SIZE  ( 4 ),
        HID_REPORT_COUNT ( 1 ),
        HID_USAGE        ( 0x39 ),
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
        HID_REPORT_SIZE  ( 4 ),
        HID_REPORT_COUNT ( 1 ),
        HID_INPUT        ( HID_CONSTANT ),
        HID_LOGICAL_MAX  ( 255 ),
        HID_PHYSICAL_MAX ( 255 ),
        HID_REPORT_SIZE  ( 8 ),
        HID_REPORT_COUNT ( 4 ),
        HID_USAGE        ( 0x30 ),
        HID_USAGE        ( 0x31 ),
        HID_USAGE        ( 0x32 ),
        HID_USAGE        ( 0x35 ),
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
        HID_USAGE_PAGE_N ( HID_USAGE_PAGE_VENDOR, 2 ),
        HID_USAGE        ( 0x20 ),
        HID_REPORT_COUNT ( 1 ),
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
        HID_USAGE_PAGE_N ( HID_USAGE_PAGE_VENDOR, 2 ),
        HID_USAGE_N      ( 0x2621, 2 ),
        HID_REPORT_COUNT ( 8 ),
        HID_OUTPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
    HID_COLLECTION_END
};

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 70),
    TUD_HID_DESCRIPTOR(0, 0, false, sizeof(switch_pro_hid_report_descriptor), 0x81, 64, 1),
};

static const tusb_desc_device_t hid_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x0F0D,
    .idProduct = 0x0092,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static const char* hid_string_descriptor[] = {
    (char[]){0x09, 0x04},
    "Easycon",
    "Easycon Pro Controller",
    "0123456789",
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return switch_pro_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

switch_hid_report_t hid_next_report;

static void hid_reset_report(void) {
    memset(&hid_next_report, 0, sizeof(hid_next_report));
    hid_next_report.LX = SWITCH_STICK_CENTER;
    hid_next_report.LY = SWITCH_STICK_CENTER;
    hid_next_report.RX = SWITCH_STICK_CENTER;
    hid_next_report.RY = SWITCH_STICK_CENTER;
    hid_next_report.HAT = SWITCH_HAT_CENTERED;
}

void hid_device_init(void) {
    hid_reset_report();
    
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = &hid_device_descriptor;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);
    
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    
    vTaskDelay(pdMS_TO_TICKS(100));
    if (tud_mounted()) {
        tud_hid_report(0, &hid_next_report, sizeof(hid_next_report));
    }
}

void hid_device_set_report(const switch_hid_report_t *report) {
    hid_next_report = *report;
}

void hid_device_get_report(switch_hid_report_t *report) {
    *report = hid_next_report;
}

void hid_device_send(void) {
    if (!tud_mounted()) return;
    if (!tud_hid_ready()) return;
    
    bool ok = tud_hid_report(0, &hid_next_report, sizeof(hid_next_report));
    if (ok) {
        gpio_set_level(GPIO_NUM_10, 0);
    } else {
        gpio_set_level(GPIO_NUM_10, 1);
    }
}

int hid_device_is_ready(void) {
    return tud_mounted() && tud_hid_ready();
}

int hid_device_is_mounted(void) {
    return tud_mounted();
}

uint32_t hid_device_get_send_count(void) { return 0; }
uint32_t hid_device_get_fail_count(void) { return 0; }
