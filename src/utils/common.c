#include "distribpro/common.h"
#include <stdarg.h>
#include <time.h>

void dp_log(const char *level, const char *fmt, va_list args) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[26];
    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    printf("[%s] [%s] ", time_str, level);
    vprintf(fmt, args);
    printf("\n");
}

void dp_log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    dp_log("INFO", fmt, args);
    va_end(args);
}

void dp_log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    dp_log("ERROR", fmt, args);
    va_end(args);
}
