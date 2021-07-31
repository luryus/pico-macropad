#if !defined(LOG__H)
#define LOG__H

#include <stdio.h>
#include "hardware/clocks.h"

#define RESET "\033[0m"
#define CYAN "\033[36m"
#define YELLOW "\033[33m"
#define RED "\033[31m"
#define BOLD "\033[1m"
#define GRAY "\033[90m"

#define __LOG(level, format, ...) \
    printf(level " [%lu] " format "\n", time_us_32() / 1000, ##__VA_ARGS__)

#define LOGE(format, ...) __LOG(RED BOLD "ERR", format RESET, ##__VA_ARGS__)
#define LOGW(format, ...) __LOG(YELLOW "WRN", format RESET, ##__VA_ARGS__)
#define LOGI(format, ...) __LOG(CYAN "INF" RESET, format, ##__VA_ARGS__)
#define LOGD(format, ...) __LOG(GRAY "DBG", format RESET, ##__VA_ARGS__)

#endif // LOG__H
