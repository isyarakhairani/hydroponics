#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "context.h"
#include "error.h"
#include "smartconfig.h"

#define CONNECTED_BIT BIT(0)

static const char *TAG = "wifi";

static EventGroupHandle_t wifi_event_group;
static context_t *context;

int8_t wifi_get_ap_rssi(void)
{
    wifi_ap_record_t ap_info;
    ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
    return ap_info.rssi;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ARG_UNUSED(arg);
    ESP_LOGD(TAG, "Event base: %s, id: %d", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Starting wifi...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Connected to %s...", context->config.ssid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Disconnected from %*s, reason: %d", event->ssid_len, event->ssid, event->reason);
        ESP_ERROR_CHECK(context_set_network_connected(context, false));
        ESP_ERROR_CHECK(esp_wifi_connect());
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        ESP_ERROR_CHECK(context_set_network_error(context, true));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Got ip address...");
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        ESP_LOGI(TAG, "Lost ip address...");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    }
}

static void wifi_dev_init(void)
{
    static bool initialized = false;
    if (initialized) {
        return;
    }

    esp_log_level_set("wifi", ESP_LOG_VERBOSE);
    ESP_ERROR_CHECK(context_set_network_connected(context, false));
    ESP_LOGI(TAG, "Initializing wifi...");

    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
    initialized = true;
}

static esp_err_t wifi_is_provisioned(bool *provisioned)
{
    ARG_CHECK(provisioned != NULL, ERR_PARAM_NULL);

    *provisioned = false;

    wifi_config_t wifi_config;
    if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config) != ESP_OK) {
        return ESP_FAIL;
    }

    if (strlen((const char *)wifi_config.sta.ssid)) {
        *provisioned = true;
        memcpy(context->config.ssid, wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid));
        memcpy(context->config.password, wifi_config.sta.password, sizeof(wifi_config.sta.password));
        ESP_LOGI(TAG, "Saved ssid: %s", (const char *)context->config.ssid);
        ESP_LOGI(TAG, "Saved password: %s", (const char *)context->config.password);
        ESP_ERROR_CHECK(context_set_wifi_provisioned(context));
    }
    return ESP_OK;
}

static void wifi_task(void *arg)
{
    context = (context_t *)arg;
    ARG_ERROR_CHECK(context != NULL, ERR_PARAM_NULL);

    ESP_ERROR_CHECK(context_set_network_connected(context, false));
    ESP_ERROR_CHECK(context_set_network_error(context, false));
    while (true) {
        wifi_dev_init();
        bool provisioned = false;
        ESP_ERROR_CHECK(wifi_is_provisioned(&provisioned));
        if (!provisioned) {
            ESP_LOGI(TAG, "Starting provisioning...");
            ESP_ERROR_CHECK(smartconfig_init(context));
        }
        xEventGroupWaitBits(context->event_group, CONTEXT_EVENT_WIFI, pdTRUE, pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Connecting to %s...", (const char *)context->config.ssid);
        ESP_ERROR_CHECK(esp_wifi_connect());
        while (true) {
            ESP_ERROR_CHECK(context_set_network_connected(context, false));
            xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
            ESP_ERROR_CHECK(context_set_network_connected(context, true));
            ESP_ERROR_CHECK(context_set_network_error(context, false));

            // Wait until network error is dispatched.
            xEventGroupWaitBits(context->event_group, CONTEXT_EVENT_NETWORK_ERROR, pdTRUE, pdTRUE, portMAX_DELAY);
            ESP_LOGW(TAG, "Network error, reconnecting...");
            ESP_ERROR_CHECK(esp_wifi_disconnect());
            ESP_ERROR_CHECK(esp_wifi_connect());
        }
    }
}

esp_err_t wifi_init(context_t *context)
{
    ARG_CHECK(context != NULL, ERR_PARAM_NULL);

    xTaskCreatePinnedToCore(wifi_task, "wifi", 4096, context, configMAX_PRIORITIES - 7, NULL, tskNO_AFFINITY);
    return ESP_OK;
}