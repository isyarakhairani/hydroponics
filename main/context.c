#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_err.h"

#include "context.h"
#include "error.h"

#define CONTEXT_PH_MIN_VALUE 5.5
#define CONTEXT_PH_MAX_VALUE 6.5
#define CONTEXT_TANK_MIN_VALUE 20
#define CONTEXT_TANK_MAX_VALUE 24

#define context_set(p, v, f)  \
    do {                      \
        if ((p) != (v)) {     \
            (p) = (v);        \
            bitsToSet |= (f); \
        }                     \
    } while (0)

#define context_set_single(c, p, v, f)                 \
    do {                                               \
        if ((p) != (v)) {                              \
            (p) = (v);                                 \
            xEventGroupSetBits((c)->event_group, (f)); \
        }                                              \
    } while (0)

#define context_set_flags(c, v, f)                       \
    do {                                                 \
        if (v) {                                         \
            xEventGroupSetBits((c)->event_group, (f));   \
        } else {                                         \
            xEventGroupClearBits((c)->event_group, (f)); \
        }                                                \
    } while (0)

static const char *TAG = "context";

context_t *context_create(void)
{
    context_t *context = calloc(1, sizeof(context_t));

    portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
    context->spinlock = spinlock;
    context->event_group = xEventGroupCreate();

    context->cycle.initialized = false;
    context->cycle.task_handle = NULL;

    context->sensors.temp = 0;
    context->sensors.humidity = 0;

    context->sensors.tds.value = 0;
    context->sensors.tds.target_min = 0;
    context->sensors.tds.target_min = 0;
    context->sensors.tds.constant = 0;
    context->sensors.tds.task_handle = NULL;

    context->sensors.ph.value = 0;
    context->sensors.ph.target_min = CONTEXT_PH_MIN_VALUE;
    context->sensors.ph.target_max = CONTEXT_PH_MAX_VALUE;
    context->sensors.ph.constant = 0;
    context->sensors.ph.task_handle = NULL;

    context->sensors.tank.value = 0;
    context->sensors.tank.target_min = CONTEXT_TANK_MIN_VALUE;
    context->sensors.tank.target_max = CONTEXT_TANK_MAX_VALUE;
    context->sensors.tank.task_handle = NULL;

    return context;
}

inline void context_lock(context_t *context)
{
    portENTER_CRITICAL(&context->spinlock);
}

inline void context_unlock(context_t *context)
{
    portEXIT_CRITICAL(&context->spinlock);
}

esp_err_t context_set_tds(context_t *context, float value)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);
    context_set_single(context, context->sensors.tds.value, value, CONTEXT_EVENT_TDS);
    return ESP_OK;
}

esp_err_t context_set_target_tds(context_t *context, float min, float max)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);
    context_set_single(context, context->sensors.tds.target_min, min, CONTEXT_EVENT_TDS);
    context_set_single(context, context->sensors.tds.target_max, max, CONTEXT_EVENT_TDS);
    return ESP_OK;
}

esp_err_t context_set_ph(context_t *context, float value)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);
    context_set_single(context, context->sensors.ph.value, value, CONTEXT_EVENT_PH);
    return ESP_OK;
}

esp_err_t context_set_tank(context_t *context, float value)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);
    context_set_single(context, context->sensors.tank.value, value, CONTEXT_EVENT_TANK);
    return ESP_OK;
}

esp_err_t context_set_temp_humidity(context_t *context, float temp, float humidity)
{
    EventBits_t bitsToSet = 0U;
    context_lock(context);
    context_set(context->sensors.temp, temp, CONTEXT_EVENT_TEMPERATURE);
    context_set(context->sensors.humidity, humidity, CONTEXT_EVENT_HUMIDITY);
    context_unlock(context);

    if (bitsToSet) xEventGroupSetBits(context->event_group, bitsToSet);
    return ESP_OK;
}

esp_err_t context_set_wifi_provisioned(context_t *context)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);
    xEventGroupSetBits(context->event_group, CONTEXT_EVENT_WIFI);
    return ESP_OK;
}

esp_err_t context_set_network_connected(context_t *context, bool connected)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);
    context_set_flags(context, connected, CONTEXT_EVENT_NETWORK);
    return ESP_OK;
}

esp_err_t context_set_network_error(context_t *context, bool error)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);
    context_set_flags(context, error, CONTEXT_EVENT_NETWORK_ERROR);
    return ESP_OK;
}

esp_err_t context_set_time_updated(context_t *context)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);
    xEventGroupSetBits(context->event_group, CONTEXT_EVENT_TIME);
    return ESP_OK;
}

esp_err_t context_set_iot_connected(context_t *context, bool connected)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);
    context_set_flags(context, connected, CONTEXT_EVENT_IOT);
    return ESP_OK;
}

esp_err_t context_set_cycle(context_t *context, int64_t start_time)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);
    context_set_single(context, context->cycle.start_time, start_time, CONTEXT_EVENT_CYCLE);
    context_set_single(context, context->cycle.initialized, true, CONTEXT_EVENT_CYCLE);
    return ESP_OK;
}
