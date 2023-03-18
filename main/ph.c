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

#define DEFAULT_VREF 1100
#define NO_OF_SAMPLES 64

#define NEUTRAL_VOLTAGE 1500
#define ACID_VOLTAGE 2033

#define PH_LOW 5.5
#define PH_HIGH 7.0

#define PH_PUMP_TIMER 5   // seconds
#define PH_DELAY_TIMER 10 // seconds

#define PH_UP_GPIO 18
#define PH_DOWN_GPIO 19
#define PH_GPIO ((1ULL << PH_UP_GPIO) | (1ULL << PH_DOWN_GPIO))
#define PH_ANALOG_GPIO ADC1_CHANNEL_6 //PIN 34 ; ADC1 is availalbe on pins 15, 34, 35 & 36

static const char *TAG = "ph";

static esp_timer_handle_t ph_pump_timer;
static esp_timer_handle_t ph_delay_timer;

static esp_adc_cal_characteristics_t adc1_chars;

static bool ph_pump_ready = true;
static gpio_num_t pump_gpio;

static uint32_t ph_read_raw(adc_channel_t channel)
{
    uint32_t adc_reading = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw(channel);
    }
    return adc_reading / NO_OF_SAMPLES;
}

static uint32_t ph_read_voltage(int channel)
{
    return esp_adc_cal_raw_to_voltage(ph_read_raw(channel), &adc1_chars);
}

static float ph_read(uint32_t voltage)
{
    float slope = (7.0 - 4.0) / ((NEUTRAL_VOLTAGE - 1500.0) / 3.0 - (ACID_VOLTAGE - 1500.0) / 3.0);
    float intercept = 7.0 - slope * (NEUTRAL_VOLTAGE - 1500.0) / 3.0;
    float ph = slope * (voltage - 1500.0) / 3.0 + intercept;

    return ph;
}

static void ph_task(void *arg)
{
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, DEFAULT_VREF, &adc1_chars);
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(PH_ANALOG_GPIO, ADC_ATTEN_DB_11));
    context_t *context = (context_t *)arg;
    uint32_t raw, voltage = 0;
    float ph = 0.0;
    while (true) {
        raw = ph_read_raw(ADC_CHANNEL_6);
        voltage = ph_read_voltage(ADC_CHANNEL_6);
        ph = ph_read(voltage);
        ESP_LOGI(TAG, "Raw: %d, Voltage: %d, pH: %.2f", raw, voltage, ph);

        if (ph < PH_LOW && ph_pump_ready) {
            ph_pump_ready = false;
            // start ph up pump
            pump_gpio = PH_UP_GPIO;
            gpio_set_level(pump_gpio, 1);
            ESP_ERROR_CHECK(esp_timer_start_once(ph_pump_timer, PH_PUMP_TIMER * 1000000));
            ESP_ERROR_CHECK(esp_timer_start_once(ph_delay_timer, PH_DELAY_TIMER * 1000000));
            printf("pH up start\n");
        } else if (ph > PH_HIGH && ph_pump_ready) {
            ph_pump_ready = false;
            // start ph up pump
            pump_gpio = PH_DOWN_GPIO;
            gpio_set_level(pump_gpio, 1);
            ESP_ERROR_CHECK(esp_timer_start_once(ph_pump_timer, PH_PUMP_TIMER * 1000000));
            ESP_ERROR_CHECK(esp_timer_start_once(ph_delay_timer, PH_DELAY_TIMER * 1000000));
            printf("pH down start\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void ph_pump_timer_callback(void *arg)
{
    // stop ph up pump
    gpio_set_level(pump_gpio, 0);
    printf("pH pump stop\n");
}

static void ph_delay_timer_callback(void *arg)
{
    ph_pump_ready = true;
    ESP_LOGW(TAG, "pH pump ready");
}

static void ph_timer_init(void)
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

static void ph_pump_init(void)
{
    gpio_config_t config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = PH_GPIO,
        .pull_up_en = 1,
    };
    gpio_config(&config);
}

esp_err_t ph_init(context_t *context)
{
    // Init timer
    ph_timer_init();

    // Init gpio
    ph_pump_init();

    // Start task
    xTaskCreatePinnedToCore(ph_task, "ph", 4096, context, 6, NULL, tskNO_AFFINITY);
    return ESP_OK;
}
