#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "ultrasonic.h"

#include "context.h"
#include "error.h"
#include "tank.h"

#define TANK_HEIGHT_CM 30
#define MAX_DISTANCE_CM 500 // 5m max

#define TRIGGER_GPIO 17
#define ECHO_GPIO 16

static const char *TAG = "tank_sensor";

static void tank_task(void *arg)
{
    context_t *context = (context_t *)arg;

    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO,
    };
    ultrasonic_init(&sensor);

    float distance, level;
    while (true) {
        esp_err_t err = ultrasonic_measure_cm(&sensor, MAX_DISTANCE_CM, &distance);
        if (err == ESP_OK) {
            level = TANK_HEIGHT_CM - distance;
            ESP_LOGI(TAG, "Tank level: %.2f cm", level);
        } else {
            ESP_LOGE(TAG, "Cannot measure water level, error 0x%X", err);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t tank_init(context_t *context)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);

    xTaskCreate(tank_task, "tank", 2048, context, 5, NULL);
    return ESP_OK;
}
