#ifndef IR_BLASTER_WIFI_H
#define IR_BLASTER_WIFI_H

#include "esp_err.h"

#include "context.h"

int8_t wifi_get_ap_rssi(void);

esp_err_t wifi_init(context_t *context);

#endif // IR_BLASTER_WIFI_H
