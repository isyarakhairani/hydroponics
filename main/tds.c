#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"

#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_adc_cal.h"
#include "esp_timer.h"

#include "context.h"
#include "mqtt.h"
#include "tds.h"

#define DEFAULT_VREF 1100

#define TDS_NUM_SAMPLES 32   // (int) Number of reading to take for an average
#define TDS_SAMPLE_DELAY 50  // (int) Sample period (delay between samples == sample period / number of readings)
#define TDS_TEMPERATURE 25.0 // (float) Temperature of water (we should measure this with a sensor to get an accurate reading)
#define TDS_VREF 2.28        // (float) Voltage reference for ADC. We should measure the actual value of each ESP32

#define TDS_ANALOG_GPIO ADC1_CHANNEL_0 // GPIO 36

#define TDS_A_PUMP_GPIO 16
#define TDS_B_PUMP_GPIO 17
#define PUMP_GPIO_MASK ((1ULL << TDS_A_PUMP_GPIO) | (1ULL << TDS_B_PUMP_GPIO))

static const char *TAG = "tds";

static esp_adc_cal_characteristics_t adc1_chars;

static esp_timer_handle_t tds_pump_duration_timer;
static esp_timer_handle_t tds_pump_delay_timer;

static bool is_pump_ready = true;

static void tds_config_pin()
{
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, DEFAULT_VREF, &adc1_chars);
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(TDS_ANALOG_GPIO, ADC_ATTEN_DB_11));

    gpio_config_t config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = PUMP_GPIO_MASK,
        .pull_up_en = 1,
    };
    gpio_config(&config);
}

static float tds_read()
{
    uint32_t runningSampleValue = 0;
    for (int i = 0; i < TDS_NUM_SAMPLES; i++) {
        int analogSample = adc1_get_raw(TDS_ANALOG_GPIO);
        runningSampleValue = runningSampleValue + analogSample;
        vTaskDelay(pdMS_TO_TICKS(TDS_SAMPLE_DELAY));
    }

    float tdsAverage = runningSampleValue / TDS_NUM_SAMPLES;
    uint32_t adcVoltage = esp_adc_cal_raw_to_voltage(tdsAverage, &adc1_chars);
    ESP_LOGI(TAG, "raw = %.f, voltage = %d", tdsAverage, adcVoltage);
    return tdsAverage;
}

static float tds_convert_to_ppm(float analogReading)
{
    float adcCompensation = 1 + (1 / 3.9);                                 // 1/3.9 (11dB) attenuation.
    float vPerDiv = (TDS_VREF / 4096) * adcCompensation;                   // Calculate the volts per division using the VREF taking account of the chosen attenuation value.
    float averageVoltage = analogReading * vPerDiv;                        // Convert the ADC reading into volts
    float compensationCoefficient = 1.0 + 0.02 * (TDS_TEMPERATURE - 25.0); // temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
    float compensationVolatge = averageVoltage / compensationCoefficient;  // temperature compensation
    float tdsValue = ((133.42 * compensationVolatge * compensationVolatge * compensationVolatge) -
                      (255.86 * compensationVolatge * compensationVolatge) +
                      (857.39 * compensationVolatge)) *
                     0.5; // convert voltage value to tds value

    ESP_LOGI(TAG, "averageVoltage = %f", averageVoltage);
    return tdsValue;
}

static void tds_task(void *arg)
{
    context_t *context = (context_t *)arg;

    /* Waiting for tank level measurement */
    xEventGroupWaitBits(context->event_group, CONTEXT_EVENT_TANK, pdFALSE, pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(10000));

    while (true) {
        float sensorReading = tds_read();
        float tdsResult = tds_convert_to_ppm(sensorReading);
        ESP_ERROR_CHECK(context_set_tds(context, tdsResult));
        ESP_LOGI(TAG, "value: %.02f ppm", tdsResult);

        if (context->sensors.tank.value >= context->sensors.tank.target_min &&
            context->sensors.tank.value <= context->sensors.tank.target_max) {
            if (tdsResult < context->sensors.tds.target_min) {
                if (is_pump_ready) {
                    ESP_LOGW(TAG, "TDS < %.02f, starting TDS A and B pump...", context->sensors.tds.target_min);
                    gpio_set_level(TDS_A_PUMP_GPIO, 1);
                    gpio_set_level(TDS_B_PUMP_GPIO, 1);
                    ESP_ERROR_CHECK(esp_timer_start_once(tds_pump_duration_timer, 1000000 * 5));
                    ESP_ERROR_CHECK(esp_timer_start_once(tds_pump_delay_timer, 1000000 * 30));
                    mqtt_publish_state("PUMP_TDS_A_B");
                    is_pump_ready = false;
                }
            }
        } else {
            ESP_LOGW(TAG, "Waiting for tank level to be set");
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

static void tds_pump_duration_cb(void *arg)
{
    gpio_set_level(TDS_A_PUMP_GPIO, 0);
    gpio_set_level(TDS_B_PUMP_GPIO, 0);
    ESP_LOGI(TAG, "pump stop");
}

static void tds_pump_delay_cb(void *arg)
{
    is_pump_ready = true;
    ESP_LOGI(TAG, "pump ready");
}

static void tds_init_timer(void)
{
    const esp_timer_create_args_t duration_timer_args = {
        .callback = &tds_pump_duration_cb,
        .name = "tds_pump_duration_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&duration_timer_args, &tds_pump_duration_timer));

    const esp_timer_create_args_t delay_timer_args = {
        .callback = &tds_pump_delay_cb,
        .name = "tds_pump_delay_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&delay_timer_args, &tds_pump_delay_timer));
}

esp_err_t tds_init(context_t *context)
{
    tds_config_pin();
    tds_init_timer();
    xTaskCreatePinnedToCore(tds_task, "tds", 4096, context, 5, NULL, tskNO_AFFINITY);
    return ESP_OK;
}
