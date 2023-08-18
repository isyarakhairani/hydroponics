#ifndef HYDROPONICS_TANK_H
#define HYDROPONICS_TANK_H

#include "esp_err.h"

#include "context.h"

void tank_drain_task(void *arg);

esp_err_t tank_init(context_t *context);

#endif // HYDROPONICS_TANK_H
