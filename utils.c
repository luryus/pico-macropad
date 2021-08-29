#include "utils.h"

#include <stdint.h>

bool time_passed(absolute_time_t time) {
    uint64_t now = to_us_since_boot(get_absolute_time());
    uint64_t target = to_us_since_boot(time);
    return now >= target;
}