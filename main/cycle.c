#include <stdio.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "context.h"
#include "error.h"
#include "storage.h"

#define GROW_LIGHT_GPIO 21

static const char *TAG = "cycle";

static void cycle_task(void *arg)
{
    context_t *context = (context_t *)arg;
    ARG_ERROR_CHECK(context != NULL, ERR_PARAM_NULL);

    ESP_LOGI(TAG, "Waiting for cycle time to be initialized.");
    xEventGroupWaitBits(context->event_group, CONTEXT_EVENT_TIME | CONTEXT_EVENT_CYCLE,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    gpio_config_t config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = GROW_LIGHT_GPIO,
        .pull_up_en = 1,
    };
    gpio_config(&config);

    struct tm timeinfo = {0};
    char strftime_buf[64] = {0};
    localtime_r((time_t *)&context->cycle.start_time, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%F %R", &timeinfo);
    ESP_LOGI(TAG, "Cycle started on %s", strftime_buf);

    while (true) {
        time_t current_time = time(NULL);
        localtime_r(&current_time, &timeinfo);
        ESP_LOGI(TAG, "Current time: %d:%d", timeinfo.tm_hour, timeinfo.tm_min);
        if (timeinfo.tm_hour >= 6 && timeinfo.tm_hour <= 18) {
            gpio_set_level(GROW_LIGHT_GPIO, 1);
        } else {
            gpio_set_level(GROW_LIGHT_GPIO, 0);
        }
        double elapsed_time = difftime(current_time, (time_t)context->cycle.start_time);
        int elapsed_days = (int)(elapsed_time / (24 * 3600));
        ESP_LOGI(TAG, "Days elapsed since cycle start: %d", elapsed_days);
        context->cycle.elapsed_days = elapsed_days;
        if (elapsed_days >= 21) {
            context->sensors.tds.target_min = 875;
            context->sensors.tds.target_max = 925;
        } else if (elapsed_days >= 14) {
            context->sensors.tds.target_min = 775;
            context->sensors.tds.target_max = 825;
        } else if (elapsed_days >= 7) {
            context->sensors.tds.target_min = 675;
            context->sensors.tds.target_max = 725;
        } else if (elapsed_days >= 0) {
            context->sensors.tds.target_min = 575;
            context->sensors.tds.target_max = 625;
        }
        vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000));
    }
}

esp_err_t cycle_init(context_t *context)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);

    int64_t start_time = 0;
    ESP_ERROR_CHECK(storage_get_i64("cycle_start_tm", &start_time));
    if (start_time > 0) {
        ESP_ERROR_CHECK(context_set_cycle(context, start_time));
    }

    xTaskCreatePinnedToCore(cycle_task, "cycle", 2048, context, 2, NULL, tskNO_AFFINITY);
    return ESP_OK;
}