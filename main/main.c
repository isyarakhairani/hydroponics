#include "esp_event.h"

#include "context.h"
#include "mqtt.h"
#include "ntp.h"
#include "storage.h"
#include "wifi.h"
#include "ph.h"
#include "tds.h"

static context_t *context;

void app_main(void)
{
    context = context_create();
    ESP_ERROR_CHECK(ph_init(context));
    ESP_ERROR_CHECK(tds_init(context));
    ESP_ERROR_CHECK(storage_init(context));
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(wifi_init(context));
    ESP_ERROR_CHECK(ntp_init(context));
    ESP_ERROR_CHECK(mqtt_init(context));
}