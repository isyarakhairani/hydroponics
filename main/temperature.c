#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "dht.h"

#include "context.h"
#include "error.h"
#include "temperature.h"

#define DHT22_DATA_GPIO 26

static const char *TAG = "temperature";

static void temperature_task(void *arg)
{
    context_t *context = (context_t *)arg;
    ARG_ERROR_CHECK(context != NULL, ERR_PARAM_NULL);

    esp_err_t err;
    float temperature, humidity;
    while (true) {
        err = dht_read_float_data(DHT_TYPE_AM2301, DHT22_DATA_GPIO, &humidity, &temperature);
        if (err == ESP_OK) {
            context_set_temp_humidity(context, temperature, humidity);
            ESP_LOGI(TAG, "Temperature: %.1fC Humidity: %.1f%%", temperature, humidity);
        } else {
            ESP_LOGE(TAG, "Temperature humidity measure failed, error 0x%X", err);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t temperature_init(context_t *context)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);

    xTaskCreatePinnedToCore(temperature_task, "temperature", 2048, context, 11, NULL, tskNO_AFFINITY);
    return ESP_OK;
}
