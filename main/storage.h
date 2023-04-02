#ifndef HYDROPONICS_STORAGE_H
#define HYDROPONICS_STORAGE_H

#include "esp_err.h"

#include "context.h"

esp_err_t storage_init(context_t *context);

esp_err_t storage_set_i64(const char *key, int64_t value);

esp_err_t storage_get_i64(const char *key, int64_t *out_value);

#endif // HYDROPONICS_STORAGE_H
