#include "storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static nvs_handle_t s_nvs_handle = 0;

esp_err_t storage_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret == ESP_OK) {
        ret = nvs_open("easycon", NVS_READWRITE, &s_nvs_handle);
    }
    return ret;
}

uint8_t EasyCon_read_byte(uint16_t addr) {
    uint8_t value = 0xFF;
    char key[16];
    snprintf(key, sizeof(key), "b%u", (unsigned)addr);
    nvs_get_u8(s_nvs_handle, key, &value);
    return value;
}

void EasyCon_write_start(uint8_t mode) {
    (void)mode;
}

void EasyCon_write_data(uint16_t addr, uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        char key[16];
        snprintf(key, sizeof(key), "b%u", (unsigned)(addr + i));
        nvs_set_u8(s_nvs_handle, key, data[i]);
    }
}

void EasyCon_write_end(uint8_t mode) {
    (void)mode;
    nvs_commit(s_nvs_handle);
}
