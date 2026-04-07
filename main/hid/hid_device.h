#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SWITCH_BUTTON_Y       (1 << 0)
#define SWITCH_BUTTON_B       (1 << 1)
#define SWITCH_BUTTON_A       (1 << 2)
#define SWITCH_BUTTON_X       (1 << 3)
#define SWITCH_BUTTON_L       (1 << 4)
#define SWITCH_BUTTON_R       (1 << 5)
#define SWITCH_BUTTON_ZL      (1 << 6)
#define SWITCH_BUTTON_ZR      (1 << 7)
#define SWITCH_BUTTON_MINUS   (1 << 8)
#define SWITCH_BUTTON_PLUS    (1 << 9)
#define SWITCH_BUTTON_LCLICK  (1 << 10)
#define SWITCH_BUTTON_RCLICK  (1 << 11)
#define SWITCH_BUTTON_HOME    (1 << 12)
#define SWITCH_BUTTON_CAPTURE (1 << 13)

#define SWITCH_HAT_TOP         0
#define SWITCH_HAT_TOP_RIGHT   1
#define SWITCH_HAT_RIGHT       2
#define SWITCH_HAT_BOTTOM_RIGHT 3
#define SWITCH_HAT_BOTTOM      4
#define SWITCH_HAT_BOTTOM_LEFT 5
#define SWITCH_HAT_LEFT        6
#define SWITCH_HAT_TOP_LEFT    7
#define SWITCH_HAT_CENTERED    8

#define SWITCH_STICK_MIN    0
#define SWITCH_STICK_CENTER 128
#define SWITCH_STICK_MAX    255

typedef struct {
    uint16_t Button;
    uint8_t  HAT;
    uint8_t  LX;
    uint8_t  LY;
    uint8_t  RX;
    uint8_t  RY;
    uint8_t  VendorSpec;
} __attribute__((packed)) switch_hid_report_t;

void hid_device_init(void);
void hid_device_set_report(const switch_hid_report_t *report);
void hid_device_get_report(switch_hid_report_t *report);
void hid_device_send(void);
int  hid_device_is_ready(void);
int  hid_device_is_mounted(void);
uint32_t hid_device_get_send_count(void);
uint32_t hid_device_get_fail_count(void);
