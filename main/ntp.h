#ifndef HYDROPONICS_NTP_H
#define HYDROPONICS_NTP_H

#include "esp_err.h"

#include "context.h"

esp_err_t ntp_init(context_t *context);

#endif // HYDROPONICS_NTP_H
