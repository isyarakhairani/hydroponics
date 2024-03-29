#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "context.h"
#include "mqtt.h"
#include "ph.h"

#define DEFAULT_VREF 1100  // (int) Default reference voltage
#define PH_NUM_SAMPLES 32  // (int) Number of reading to take for an average
#define PH_SAMPLE_DELAY 50 // (int) Sample delay in millis

#define PH_NEUTRAL_VOLTAGE 1555
#define PH_ACID_VOLTAGE 2010

#define PUMP_ON_DURATION 5     // (int) Pump on duration in second
#define PUMP_DELAY_DURATION 60 // (int) Delay duration between pump dosing in second

#define PH_UP_PUMP_GPIO 18
#define PH_DOWN_PUMP_GPIO 19
#define PUMP_GPIO_MASK ((1ULL << PH_UP_PUMP_GPIO) | (1ULL << PH_DOWN_PUMP_GPIO))
#define PH_ANALOG_GPIO ADC1_CHANNEL_6 // GPIO 34

static const char *TAG = "ph";

static esp_adc_cal_characteristics_t adc1_chars;

static esp_timer_handle_t ph_pump_on_timer;
static esp_timer_handle_t ph_pump_delay_timer;

static bool is_pump_ready = true;

static void ph_config_pin(void)
{
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, DEFAULT_VREF, &adc1_chars);
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(PH_ANALOG_GPIO, ADC_ATTEN_DB_11));

    gpio_config_t config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = PUMP_GPIO_MASK,
        .pull_up_en = 1,
    };
    gpio_config(&config);
}

uint32_t ph_read_voltage(void)
{
    uint32_t running_sample = 0;
    for (int i = 0; i < PH_NUM_SAMPLES; i++) {
        int adc_sample = adc1_get_raw(PH_ANALOG_GPIO);
        running_sample = running_sample + adc_sample;
        vTaskDelay(pdMS_TO_TICKS(PH_SAMPLE_DELAY));
    }
    uint32_t avg_raw = running_sample / PH_NUM_SAMPLES;
    uint32_t avg_voltage = esp_adc_cal_raw_to_voltage(avg_raw, &adc1_chars);
    ESP_LOGI(TAG, "raw = %d, voltage = %d", avg_raw, avg_voltage);
    return avg_voltage;
}

float ph_get_value(uint32_t voltage)
{
    float slope = (7 - 4.0) / ((PH_NEUTRAL_VOLTAGE - 1500.0) / 3.0 - (PH_ACID_VOLTAGE - 1500.0) / 3.0);
    float intercept = 7 - slope * (PH_NEUTRAL_VOLTAGE - 1500.0) / 3.0;
    float ph = slope * (voltage - 1500.0) / 3.0 + intercept;
    return ph;
}

static void ph_task(void *arg)
{
    context_t *context = (context_t *)arg;

    /* Waiting for tank level measurement */
    // xEventGroupWaitBits(context->event_group, CONTEXT_EVENT_TANK, pdFALSE, pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(10000));

    while (true) {
        uint32_t voltage = ph_read_voltage();
        float value = ph_get_value(voltage);
        value += (float)(context->sensors.ph.constant / 100.0);
        ESP_ERROR_CHECK(context_set_ph(context, value));
        ESP_LOGI(TAG, "value: %.02f", value);

        if (context->cycle.initialized) {
            if (context->sensors.tank.value >= context->sensors.tank.target_min &&
                context->sensors.tank.value <= context->sensors.tank.target_max) {
                if (value < context->sensors.ph.target_min) {
                    if (is_pump_ready) {
                        ESP_LOGW(TAG, "ph < %.01f, starting ph up pump...", context->sensors.ph.target_min);
                        gpio_set_level(PH_UP_PUMP_GPIO, 1);
                        ESP_ERROR_CHECK(esp_timer_start_once(ph_pump_on_timer, 1000000 * PUMP_ON_DURATION));
                        ESP_ERROR_CHECK(esp_timer_start_once(ph_pump_delay_timer, 1000000 * PUMP_DELAY_DURATION));
                        mqtt_publish_state("PUMP_PH_UP");
                        is_pump_ready = false;
                    }
                } else if (value > context->sensors.ph.target_max) {
                    if (is_pump_ready) {
                        ESP_LOGW(TAG, "ph > %.01f, starting ph down pump...", context->sensors.ph.target_max);
                        gpio_set_level(PH_DOWN_PUMP_GPIO, 1);
                        ESP_ERROR_CHECK(esp_timer_start_once(ph_pump_on_timer, 1000000 * PUMP_ON_DURATION));
                        ESP_ERROR_CHECK(esp_timer_start_once(ph_pump_delay_timer, 1000000 * PUMP_DELAY_DURATION));
                        mqtt_publish_state("PUMP_PH_DOWN");
                        is_pump_ready = false;
                    }
                }
            } else {
                ESP_LOGW(TAG, "Waiting for tank level to be set");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void pump_on_timer_cb(void *arg)
{
    gpio_set_level(PH_UP_PUMP_GPIO, 0);
    gpio_set_level(PH_DOWN_PUMP_GPIO, 0);
    ESP_LOGI(TAG, "pump stop");
}

static void pump_delay_timer_cb(void *arg)
{
    is_pump_ready = true;
    ESP_LOGI(TAG, "pump ready");
}

static void ph_create_timer(void)
{
    const esp_timer_create_args_t pump_on_timer_args = {
        .callback = &pump_on_timer_cb,
        .name = "ph_pump_on_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&pump_on_timer_args, &ph_pump_on_timer));

    const esp_timer_create_args_t pump_delay_timer_args = {
        .callback = &pump_delay_timer_cb,
        .name = "ph_pump_delay_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&pump_delay_timer_args, &ph_pump_delay_timer));
}

esp_err_t ph_init(context_t *context)
{
    ph_config_pin();
    ph_create_timer();
    xTaskCreatePinnedToCore(ph_task, "ph", 4096, context, 6, &context->sensors.ph.task_handle, tskNO_AFFINITY);
    return ESP_OK;
}
