#ifndef HYDROPONICS_MQTT_H
#define HYDROPONICS_MQTT_H

#include "esp_err.h"

#include "context.h"

esp_err_t mqtt_init(context_t *context);

#endif // HYDROPONICS_MQTT_H
