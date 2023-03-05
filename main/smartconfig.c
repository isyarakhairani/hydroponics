#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"

#include "context.h"
#include "error.h"
#include "smartconfig.h"

static const int GOT_SSID_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

static const char *TAG = "smartconfig";

static EventGroupHandle_t sc_event_group;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event base: %s, id: %d", event_base, event_id);
    if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        context_t *context = (context_t *)arg;
        smartconfig_event_got_ssid_pswd_t *event = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, event->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, event->password, sizeof(wifi_config.sta.password));

        memcpy(context->config.ssid, event->ssid, sizeof(event->ssid));
        memcpy(context->config.password, event->password, sizeof(event->password));
        ESP_LOGI(TAG, "Got ssid: %s", (const char *)wifi_config.sta.ssid);
        ESP_LOGI(TAG, "Got password: %s", (const char *)wifi_config.sta.password);

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        xEventGroupSetBits(sc_event_group, GOT_SSID_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(sc_event_group, ESPTOUCH_DONE_BIT);
    }
}

static void smartconfig_task(void *arg)
{
    context_t *context = (context_t *)arg;
    ARG_ERROR_CHECK(context != NULL, ERR_PARAM_NULL);

    ESP_LOGI(TAG, "Starting smartconfig...");

    sc_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, context));

    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t config = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&config));
    while (1) {
        uxBits = xEventGroupWaitBits(sc_event_group, GOT_SSID_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if (uxBits & GOT_SSID_BIT) {
            ESP_ERROR_CHECK(context_set_wifi_provisioned(context));
        }
        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "Smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

esp_err_t smartconfig_init(context_t *context)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);

    xTaskCreatePinnedToCore(smartconfig_task, "smartconfig", 4096, context, 3, NULL, tskNO_AFFINITY);
    return ESP_OK;
}