#pragma once

#include <stdint.h>

#define EASYCON_UART_NUM        UART_NUM_0
#define EASYCON_UART_TX_PIN     GPIO_NUM_43
#define EASYCON_UART_RX_PIN     GPIO_NUM_44
#define EASYCON_UART_BAUD_RATE  115200

void uart_handler_init(void);
void uart_handler_send(uint8_t byte);
