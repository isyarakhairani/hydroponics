#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
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

#define TANK_HEIGHT_CM 27.5
#define MAX_DISTANCE 5
#define NO_OF_SAMPLES 10

static const char *TAG = "tank";

static ultrasonic_sensor_t hcsr04;

void tank_drain_task(void *arg)
{
    context_t *context = (context_t *)arg;

    ESP_ERROR_CHECK(gpio_set_level(TANK_PUMP_GPIO, 0));
    ESP_ERROR_CHECK(gpio_set_level(SOURCE_VALVE_GPIO, 0));

    while (true) {
        esp_err_t err;
        float distance;
        float running_sample = 0;
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            err = ultrasonic_measure(&hcsr04, MAX_DISTANCE, &distance);
            if (err != ESP_OK) {
                continue;
            }
            running_sample += distance;
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (err == ESP_OK) {
            float average = TANK_HEIGHT_CM - (running_sample * 100 / NO_OF_SAMPLES);
            ESP_ERROR_CHECK(context_set_tank(context, average));
            ESP_LOGI(TAG, "Tank level = %.02f cm", average);
            if (average >= 18) {
                ESP_ERROR_CHECK(gpio_set_level(DRAIN_VALVE_GPIO, 1));
            } else {
                ESP_LOGI(TAG, "Drain complete");
                ESP_ERROR_CHECK(gpio_set_level(DRAIN_VALVE_GPIO, 0));
                ESP_LOGI(TAG, "Restarting in 5 seconds...");
                vTaskDelay(pdMS_TO_TICKS(5000));
                esp_restart();
            }
        } else {
            ESP_LOGE(TAG, "Tank level measure failed, error 0x%X", err);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void tank_task(void *arg)
{
    context_t *context = (context_t *)arg;

    /* Waiting for cycle to be started */
    // xEventGroupWaitBits(context->event_group, CONTEXT_EVENT_CYCLE, pdFALSE, pdTRUE, portMAX_DELAY);

    while (true) {
        esp_err_t err;
        float distance;
        float running_sample = 0;
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            err = ultrasonic_measure(&hcsr04, MAX_DISTANCE, &distance);
            if (err != ESP_OK) {
                continue;
            }
            running_sample += distance;
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (err == ESP_OK) {
            float average = TANK_HEIGHT_CM - (running_sample * 100 / NO_OF_SAMPLES);
            ESP_ERROR_CHECK(context_set_tank(context, average));
            ESP_LOGI(TAG, "Tank level = %.02f cm", average);
            if (context->cycle.initialized) {
                if (average < context->sensors.tank.target_min) {
                    ESP_ERROR_CHECK(gpio_set_level(TANK_PUMP_GPIO, 0));
                    ESP_ERROR_CHECK(gpio_set_level(SOURCE_VALVE_GPIO, 1));
                    ESP_ERROR_CHECK(gpio_set_level(DRAIN_VALVE_GPIO, 0));
                } else if (average > context->sensors.tank.target_max) {
                    ESP_ERROR_CHECK(gpio_set_level(TANK_PUMP_GPIO, 0));
                    ESP_ERROR_CHECK(gpio_set_level(SOURCE_VALVE_GPIO, 0));
                    ESP_ERROR_CHECK(gpio_set_level(DRAIN_VALVE_GPIO, 1));
                } else {
                    ESP_ERROR_CHECK(gpio_set_level(TANK_PUMP_GPIO, 1));
                    ESP_ERROR_CHECK(gpio_set_level(SOURCE_VALVE_GPIO, 0));
                    ESP_ERROR_CHECK(gpio_set_level(DRAIN_VALVE_GPIO, 0));
                }
            }
        } else {
            ESP_LOGE(TAG, "Tank level measure failed, error 0x%X", err);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void driver_init(void)
{
    hcsr04.trigger_pin = TRIGGER_GPIO;
    hcsr04.echo_pin = ECHO_GPIO;
    ultrasonic_init(&hcsr04);

    gpio_config_t config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = TANK_GPIO_MASK,
        .pull_up_en = 1,
    };
    gpio_config(&config);
}

esp_err_t tank_init(context_t *context)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);

    driver_init();

    xTaskCreatePinnedToCore(tank_task, "tank", 4096, context, 5, &context->sensors.tank.task_handle, tskNO_AFFINITY);
    return ESP_OK;
}
