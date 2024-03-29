#ifndef HYDROPONICS_CONTEXT_H
#define HYDROPONICS_CONTEXT_H

#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"

#include "esp_bit_defs.h"

#define CONTEXT_UNKNOWN_VALUE INT16_MIN
#define CONTEXT_VALUE_IS_VALID(x) ((x) != CONTEXT_UNKNOWN_VALUE)

typedef enum {
    CONTEXT_EVENT_WIFI = BIT0,
    CONTEXT_EVENT_NETWORK = BIT1,
    CONTEXT_EVENT_TIME = BIT2,
    CONTEXT_EVENT_IOT = BIT3,
    CONTEXT_EVENT_NETWORK_ERROR = BIT4,
    CONTEXT_EVENT_TEMPERATURE = BIT5,
    CONTEXT_EVENT_HUMIDITY = BIT6,
    CONTEXT_EVENT_TDS = BIT7,
    CONTEXT_EVENT_PH = BIT8,
    CONTEXT_EVENT_PUMP_PH_UP = BIT9,
    CONTEXT_EVENT_PUMP_PH_DOWN = BIT10,
    CONTEXT_EVENT_PUMP_TDS_A = BIT11,
    CONTEXT_EVENT_PUMP_TDS_B = BIT12,
    CONTEXT_EVENT_PUMP_MAIN = BIT13,
    CONTEXT_EVENT_TANK = BIT14,
    CONTEXT_EVENT_TANK_VALVE = BIT15,
    CONTEXT_EVENT_CYCLE = BIT16,
} context_event_t;

typedef struct {
    portMUX_TYPE spinlock;
    EventGroupHandle_t event_group;

    struct {
        uint8_t ssid[32];
        uint8_t password[64];
    } config;

    struct {
        bool initialized;
        int64_t start_time;
        int elapsed_days;
        TaskHandle_t task_handle;
    } cycle;

    struct {
        volatile float temp;
        volatile float humidity;
        struct {
            volatile float value;
            volatile float target_min;
            volatile float target_max;
            volatile int constant;
            TaskHandle_t task_handle;
        } tds;
        struct {
            volatile float value;
            volatile float target_min;
            volatile float target_max;
            volatile int constant;
            TaskHandle_t task_handle;
        } ph;
        struct {
            volatile float value;
            volatile float target_min;
            volatile float target_max;
            TaskHandle_t task_handle;
        } tank;
    } sensors;
} context_t;

context_t *context_create(void);

void context_lock(context_t *context);

void context_unlock(context_t *context);

esp_err_t context_set_tds(context_t *context, float value);

esp_err_t context_set_target_tds(context_t *context, float min, float max);

esp_err_t context_set_ph(context_t *context, float value);

esp_err_t context_set_tank(context_t *context, float value);

esp_err_t context_set_temp_humidity(context_t *context, float temp, float humidity);

esp_err_t context_set_wifi_provisioned(context_t *context);

esp_err_t context_set_network_connected(context_t *context, bool connected);

esp_err_t context_set_network_error(context_t *context, bool error);

esp_err_t context_set_time_updated(context_t *context);

esp_err_t context_set_iot_connected(context_t *context, bool connected);

esp_err_t context_set_cycle(context_t *context, int64_t start_time);

#endif // HYDROPONICS_CONTEXT_H
