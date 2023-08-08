#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "ultrasonic.h"

#include "context.h"
#include "error.h"
#include "tank.h"

#define TRIGGER_GPIO 2
#define ECHO_GPIO 15

#define SOURCE_VALVE_GPIO 22
#define DRAIN_VALVE_GPIO 23
#define TANK_PUMP_GPIO 5
#define TANK_GPIO_MASK ((1ULL << TANK_PUMP_GPIO) | (1ULL << SOURCE_VALVE_GPIO) | (1ULL << DRAIN_VALVE_GPIO))

#define TANK_HEIGHT_CM 28
#define MAX_DISTANCE 5
#define NO_OF_SAMPLES 20

static const char *TAG = "tank";

static void tank_task(void *arg)
{
    context_t *context = (context_t *)arg;

    gpio_config_t config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = TANK_GPIO_MASK,
        .pull_up_en = 1,
    };
    gpio_config(&config);

    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO,
    };
    ultrasonic_init(&sensor);

    /* Waiting for cycle to be started */
    xEventGroupWaitBits(context->event_group, CONTEXT_EVENT_CYCLE, pdFALSE, pdTRUE, portMAX_DELAY);

    while (true) {
        float distance, scratch = 0;
        esp_err_t err;
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            err = ultrasonic_measure(&sensor, MAX_DISTANCE, &distance);
            if (err != ESP_OK) {
                break;
            }
            scratch += distance;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (err == ESP_OK) {
            float average = TANK_HEIGHT_CM - (scratch * 100 / NO_OF_SAMPLES);
            // ESP_LOGI(TAG, "Tank level: %.02f cm", average);
            ESP_ERROR_CHECK(context_set_tank(context, average));
            if (average < context->sensors.tank.target_min) {
                ESP_LOGW(TAG, "Tank level = %.02f cm (low). Filling tank...", average);
                ESP_ERROR_CHECK(gpio_set_level(TANK_PUMP_GPIO, 0));
                ESP_ERROR_CHECK(gpio_set_level(SOURCE_VALVE_GPIO, 1));
                ESP_ERROR_CHECK(gpio_set_level(DRAIN_VALVE_GPIO, 0));
            } else if (average > context->sensors.tank.target_max) {
                ESP_LOGW(TAG, "Tank level = %.02f cm (high). Draining tank...", average);
                ESP_ERROR_CHECK(gpio_set_level(TANK_PUMP_GPIO, 0));
                ESP_ERROR_CHECK(gpio_set_level(SOURCE_VALVE_GPIO, 0));
                ESP_ERROR_CHECK(gpio_set_level(DRAIN_VALVE_GPIO, 1));
            } else {
                ESP_LOGI(TAG, "Tank level = %.02f cm (normal)", average);
                ESP_ERROR_CHECK(gpio_set_level(TANK_PUMP_GPIO, 1));
                ESP_ERROR_CHECK(gpio_set_level(SOURCE_VALVE_GPIO, 0));
                ESP_ERROR_CHECK(gpio_set_level(DRAIN_VALVE_GPIO, 0));
            }
        } else {
            ESP_LOGE(TAG, "Tank level measure failed, error 0x%X", err);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t tank_init(context_t *context)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);

    xTaskCreate(tank_task, "tank", 4096, context, 5, NULL);
    return ESP_OK;
}
