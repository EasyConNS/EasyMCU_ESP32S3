#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t storage_init(void);
uint8_t EasyCon_read_byte(uint16_t addr);
void EasyCon_write_start(uint8_t mode);
void EasyCon_write_data(uint16_t addr, uint8_t *data, uint16_t len);
void EasyCon_write_end(uint8_t mode);
