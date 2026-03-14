#ifndef DISTRIBPRO_COMMON_H
#define DISTRIBPRO_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Standard Response Object
typedef struct {
    bool success;
    char *json_data;
    char *error_code;
    char *error_message;
} ApiResponse;

// Logger
void dp_log_info(const char *fmt, ...);
void dp_log_error(const char *fmt, ...);

#endif // DISTRIBPRO_COMMON_H
