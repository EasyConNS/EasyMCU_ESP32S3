#pragma once

#include <stdint.h>
#include <stdbool.h>

#define VERSION 0x46
#define MEM_SIZE 512
#define SERIAL_BUFFER_SIZE 20
#define ECHO_TIMES 2
#define ECHO_INTERVAL 2

#define CMD_READY       0xA5
#define CMD_DEBUG       0x80
#define CMD_HELLO       0x81
#define CMD_FLASH       0x82
#define CMD_SCRIPTSTART 0x83
#define CMD_SCRIPTSTOP  0x84
#define CMD_VERSION     0x85
#define CMD_LED         0x86

#define REPLY_ERROR     0x00
#define REPLY_ACK       0xFF
#define REPLY_BUSY      0xFE
#define REPLY_HELLO     0x80
#define REPLY_FLASHSTART 0x81
#define REPLY_FLASHEND  0x82
#define REPLY_SCRIPTACK 0x83

#define STICK_MIN    0
#define STICK_CENTER 128
#define STICK_MAX    255

#define BUTTON_Y       (1 << 0)
#define BUTTON_B       (1 << 1)
#define BUTTON_A       (1 << 2)
#define BUTTON_X       (1 << 3)
#define BUTTON_L       (1 << 4)
#define BUTTON_R       (1 << 5)
#define BUTTON_ZL      (1 << 6)
#define BUTTON_ZR      (1 << 7)
#define BUTTON_MINUS   (1 << 8)
#define BUTTON_PLUS    (1 << 9)
#define BUTTON_LCLICK  (1 << 10)
#define BUTTON_RCLICK  (1 << 11)
#define BUTTON_HOME    (1 << 12)
#define BUTTON_CAPTURE (1 << 13)

#define HAT_TOP          0
#define HAT_TOP_RIGHT    1
#define HAT_RIGHT        2
#define HAT_BOTTOM_RIGHT 3
#define HAT_BOTTOM       4
#define HAT_BOTTOM_LEFT  5
#define HAT_LEFT         6
#define HAT_TOP_LEFT     7
#define HAT_CENTERED     8

#define SERIAL_BUFFER(idx) (serial_buffer[(uint8_t)(idx)])

void EasyCon_serial_task(int16_t byte);
void EasyCon_serial_send(uint8_t byte);
void EasyCon_tick(void);
bool EasyCon_need_send_report(void);
void EasyCon_report_send_callback(void);

void EasyCon_script_init(void);
void EasyCon_script_task(void);
void EasyCon_script_start(void);
void EasyCon_script_stop(void);
void EasyCon_script_auto_start(void);

void set_buttons(uint16_t buttons);
void press_buttons(uint16_t buttons);
void release_buttons(uint16_t buttons);
void set_HAT_switch(uint8_t hat);
void set_left_stick(uint8_t lx, uint8_t ly);
void set_right_stick(uint8_t rx, uint8_t ry);
void reset_hid_report(void);

uint8_t EasyCon_read_byte(uint16_t addr);
void EasyCon_write_start(uint8_t mode);
void EasyCon_write_data(uint16_t addr, uint8_t *data, uint16_t len);
void EasyCon_write_end(uint8_t mode);
