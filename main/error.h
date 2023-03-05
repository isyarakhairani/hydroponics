#ifndef IR_BLASTER_ERROR_H
#define IR_BLASTER_ERROR_H

#include <sys/cdefs.h>

#include "esp_log.h"

#define ERR_PARAM_NULL "parameter == null"

#define ARG_UNUSED(x) (void)(x)

#define ARG_CHECK(a, str, ...)                                                                  \
    do {                                                                                        \
        if (unlikely(!(a))) {                                                                   \
            arg_loge(TAG, "%s:%d (%s): " str, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
            return ESP_ERR_INVALID_ARG;                                                         \
        }                                                                                       \
    } while (0)

#define ARG_ERROR_CHECK(a, str, ...)                                                            \
    do {                                                                                        \
        if (unlikely(!(a))) {                                                                   \
            arg_loge(TAG, "%s:%d (%s): " str, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
            ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);                                             \
        }                                                                                       \
    } while (0)

void arg_loge(const char *tag, const char *fmt, ...) __printflike(2, 3);

#endif // IR_BLASTER_ERROR_H
