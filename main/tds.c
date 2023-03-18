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
#include "tds.h"

#define DEFAULT_VREF 1100

#define TDS_STABILISATION_DELAY 10                                    //(int) How long to wait (in seconds) after enabling sensor before taking a reading
#define TDS_NUM_SAMPLES 10                                            //(int) Number of reading to take for an average
#define TDS_SAMPLE_PERIOD 20                                          //(int) Sample period (delay between samples == sample period / number of readings)
#define TDS_TEMPERATURE 24.0                                          //(float) Temperature of water (we should measure this with a sensor to get an accurate reading)
#define TDS_VREF 1.18                                                 //(float) Voltage reference for ADC. We should measure the actual value of each ESP32
#define TDS_SAMPLE_DELAY (TDS_SAMPLE_PERIOD / TDS_NUM_SAMPLES) * 1000 // seconds

#define TDS_ANALOG_GPIO ADC1_CHANNEL_7 // PIN 35 ; ADC1 is availalbe on pins 15, 34, 35 & 36

// #define PH_LOW 5.5
// #define PH_HIGH 7.0
// #define TDS_PUMP_TIMER 5   // seconds
// #define TDS_DELAY_TIMER 10 // seconds

#define TDS_A_GPIO 16
#define TDS_B_GPIO 17

static const char *TDS = "TDS INFO";
static esp_timer_handle_t ph_pump_timer;
static esp_timer_handle_t ph_delay_timer;

static esp_adc_cal_characteristics_t adc1_chars;

static void tds_config_pins()
{
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, DEFAULT_VREF, &adc1_chars);
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(TDS_ANALOG_GPIO, ADC_ATTEN_DB_11));
}

static float tds_read()
{
    // Take n sensor readings every p millseconds where n is TDS_NUM_SAMPLES, and p is TDS_SAMPLE_DELAY.
    // Return the average sample value.
    uint32_t runningSampleValue = 0;

    for (int i = 0; i < TDS_NUM_SAMPLES; i++) {
        // Read analogue value
        int analogSample = adc1_get_raw(TDS_ANALOG_GPIO);
        ESP_LOGI(TDS, "Read analog value %d then sleep for %d milli seconds.", analogSample, TDS_SAMPLE_DELAY);
        runningSampleValue = runningSampleValue + analogSample;
        vTaskDelay(TDS_SAMPLE_DELAY / portTICK_PERIOD_MS);
    }

    float tdsAverage = runningSampleValue / TDS_NUM_SAMPLES;
    ESP_LOGI(TDS, "Calculated average = %f", tdsAverage);
    return tdsAverage;
}

static float tds_convert_to_ppm(float analogReading)
{
    ESP_LOGI(TDS, "Converting an analog value to a TDS PPM value.");
    float adcCompensation = 1 + (1 / 3.9);                                                                                                                                                 // 1/3.9 (11dB) attenuation.
    float vPerDiv = (TDS_VREF / 4096) * adcCompensation;                                                                                                                                   // Calculate the volts per division using the VREF taking account of the chosen attenuation value.
    float averageVoltage = analogReading * vPerDiv;                                                                                                                                        // Convert the ADC reading into volts
    float compensationCoefficient = 1.0 + 0.02 * (TDS_TEMPERATURE - 25.0);                                                                                                                 // temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
    float compensationVolatge = averageVoltage / compensationCoefficient;                                                                                                                  // temperature compensation
    float tdsValue = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 * compensationVolatge * compensationVolatge + 857.39 * compensationVolatge) * 0.5; // convert voltage value to tds value

    ESP_LOGI(TDS, "Volts per division = %f", vPerDiv);
    ESP_LOGI(TDS, "Average Voltage = %f", averageVoltage);
    ESP_LOGI(TDS, "Temperature (currently fixed, we should measure this) = %f", TDS_TEMPERATURE);
    ESP_LOGI(TDS, "Compensation Coefficient = %f", compensationCoefficient);
    ESP_LOGI(TDS, "Compensation Voltge = %f", compensationVolatge);
    ESP_LOGI(TDS, "tdsValue = %f ppm", tdsValue);
    return tdsValue;
}

static void tds_task(void *pvParameters)
{
    while (true) {
        float sensorReading = tds_read();
        float tdsResult = tds_convert_to_ppm(sensorReading);
        printf("TDS Reading = %f ppm\n", tdsResult);
        vTaskDelay(((1000 / portTICK_PERIOD_MS) * 60) * 1); // delay in minutes between measurements
    }
}

esp_err_t tds_init(context_t *context)
{
    // Init timer
    // tds_timer_init();

    // Init gpio
    // tds_pump_init();

    // Start task
    xTaskCreatePinnedToCore(tds_task, "tds", 4096, context, 5, NULL, tskNO_AFFINITY);
    return ESP_OK;
}
