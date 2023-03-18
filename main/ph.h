#ifndef HYDROPONICS_PH_H
#define HYDROPONICS_PH_H

#include "esp_err.h"

#include "context.h"

uint32_t ph_read_voltage(void);

float ph_get_value(uint32_t voltage);

esp_err_t ph_init(context_t *context);

#endif // HYDROPONICS_PH_H
