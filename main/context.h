#ifndef IR_BLASTER_CONTEXT_H
#define IR_BLASTER_CONTEXT_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"

#include "esp_bit_defs.h"

#define CONTEXT_UNKNOWN_VALUE INT16_MIN
#define CONTEXT_VALUE_IS_VALID(x) ((x) != CONTEXT_UNKNOWN_VALUE)

typedef enum {
    CONTEXT_EVENT_TEMPERATURE = BIT0,
    CONTEXT_EVENT_HUMIDITY = BIT1,
    CONTEXT_EVENT_WIFI = BIT2,
    CONTEXT_EVENT_NETWORK = BIT3,
    CONTEXT_EVENT_TIME = BIT4,
    CONTEXT_EVENT_IOT = BIT5,
    CONTEXT_EVENT_NETWORK_ERROR = BIT6,
} context_event_t;

typedef struct {
    portMUX_TYPE spinlock;
    EventGroupHandle_t event_group;

    struct {
        uint8_t ssid[32];
        uint8_t password[64];
    } config;

    struct {
        volatile float temp;
        volatile float humidity;
    } sensors;
} context_t;

context_t *context_create(void);

void context_lock(context_t *context);

void context_unlock(context_t *context);

esp_err_t context_set_temp_humidity(context_t *context, float temp, float humidity);

esp_err_t context_set_wifi_provisioned(context_t *context);

esp_err_t context_set_network_connected(context_t *context, bool connected);

esp_err_t context_set_network_error(context_t *context, bool error);

esp_err_t context_set_time_updated(context_t *context);

esp_err_t context_set_iot_connected(context_t *context, bool connected);

#endif // IR_BLASTER_CONTEXT_H
