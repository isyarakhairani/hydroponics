#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "context.h"
#include "error.h"
#include "storage.h"

static const char *TAG = "storage";

static nvs_handle_t handle;

esp_err_t storage_init(context_t *context)
{
    ARG_UNUSED(context);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "NVS partition was truncated and needs to be erased. err: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &handle));
    return ESP_OK;
}

esp_err_t storage_set_i64(const char *key, int64_t value)
{
    esp_err_t err = nvs_set_i64(handle, key, value);
    return err = ESP_OK ? nvs_commit(handle) : err;
}

esp_err_t storage_get_i64(const char *key, int64_t *out_value)
{
    esp_err_t err = nvs_get_i64(handle, key, out_value);
    return err = ESP_ERR_NOT_FOUND ? ESP_OK : err;
}
