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
#include "ph.h"

#define DEFAULT_VREF 1100  // (int) Default reference voltage
#define PH_NUM_SAMPLES 64  // (int) Number of reading to take for an average
#define PH_SAMPLE_DELAY 15 // (int) Sample delay in millis

#define PH_NEUTRAL_VOLTAGE 1500
#define PH_ACID_VOLTAGE 2033

#define PH_PUMP_TIMER 5    // (int) Pump on duration in second
#define PH_DELAY_TIMER 120 // (int) Delay duration between pump dosing in second

#define PH_UP_GPIO 18
#define PH_DOWN_GPIO 19
#define PH_GPIO_MASK ((1ULL << PH_UP_GPIO) | (1ULL << PH_DOWN_GPIO))
#define PH_ANALOG_GPIO ADC1_CHANNEL_6 // PIN 34 ; ADC1 is availalbe on pins 15, 34, 35 & 36

static const char *TAG = "ph_sensor";

static esp_timer_handle_t ph_pump_timer;
static esp_timer_handle_t ph_delay_timer;

static esp_adc_cal_characteristics_t adc1_chars;

static bool ph_pump_ready = true;
static gpio_num_t pump_gpio;

static void ph_config_pins(void)
{
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, DEFAULT_VREF, &adc1_chars);
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(PH_ANALOG_GPIO, ADC_ATTEN_DB_11));

    gpio_config_t config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = PH_GPIO_MASK,
        .pull_up_en = 1,
    };
    gpio_config(&config);
}

uint32_t ph_read_voltage(void)
{
    uint32_t adc_reading = 0;
    for (int i = 0; i < PH_NUM_SAMPLES; i++) {
        int adc_raw = adc1_get_raw(PH_ANALOG_GPIO);
        adc_reading = adc_reading + adc_raw;
        vTaskDelay(pdMS_TO_TICKS(PH_SAMPLE_DELAY));
    }
    adc_reading /= PH_NUM_SAMPLES;
    uint32_t adc_voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc1_chars);
    ESP_LOGI(TAG, "raw: %d voltage: %d", adc_reading, adc_voltage);
    return adc_voltage;
}

float ph_get_value(uint32_t voltage)
{
    float slope = (7.0 - 4.0) / ((PH_NEUTRAL_VOLTAGE - 1500.0) / 3.0 - (PH_ACID_VOLTAGE - 1500.0) / 3.0);
    float intercept = 7.0 - slope * (PH_NEUTRAL_VOLTAGE - 1500.0) / 3.0;
    float ph = slope * (voltage - 1500.0) / 3.0 + intercept;
    return ph;
}

static void ph_task(void *arg)
{
    context_t *context = (context_t *)arg;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        uint32_t voltage = ph_read_voltage();
        float value = ph_get_value(voltage);
        context_set_ph(context, value);
        ESP_LOGI(TAG, "value: %.2f", value);

        if (ph_pump_ready) {
            if (value < context->sensors.ph.target_min) {
                ESP_LOGW(TAG, "pH < %.1f, starting pH up pump...", context->sensors.ph.target_min);
                pump_gpio = PH_UP_GPIO;
            } else if (value > context->sensors.ph.target_max) {
                ESP_LOGW(TAG, "pH > %.1f, starting pH down pump...", context->sensors.ph.target_max);
                pump_gpio = PH_DOWN_GPIO;
            } else {
                ESP_LOGI(TAG, "pH is normal.");
                continue;
            }
            gpio_set_level(pump_gpio, 1);
            ESP_ERROR_CHECK(esp_timer_start_once(ph_pump_timer, PH_PUMP_TIMER * 1000000));
            ESP_ERROR_CHECK(esp_timer_start_once(ph_delay_timer, PH_DELAY_TIMER * 1000000));
            ph_pump_ready = false;
            printf("pH pump start.\n");
        } else {
            ESP_LOGI(TAG, "pH pump is not available.");
        }
    }
}

static void ph_pump_timer_callback(void *arg)
{
    gpio_set_level(pump_gpio, 0);
    printf("pH pump stop.\n");
}

static void ph_delay_timer_callback(void *arg)
{
    ph_pump_ready = true;
    ESP_LOGI(TAG, "pH pump ready.");
}

static void ph_create_timer(void)
{
    const esp_timer_create_args_t ph_pump_timer_args = {
        .callback = &ph_pump_timer_callback,
        .name = "ph_pump_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&ph_pump_timer_args, &ph_pump_timer));

    const esp_timer_create_args_t ph_delay_timer_args = {
        .callback = &ph_delay_timer_callback,
        .name = "ph_delay_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&ph_delay_timer_args, &ph_delay_timer));
}

esp_err_t ph_init(context_t *context)
{
    ph_config_pins();
    ph_create_timer();
    xTaskCreatePinnedToCore(ph_task, "ph", 4096, context, 6, NULL, tskNO_AFFINITY);
    return ESP_OK;
}
