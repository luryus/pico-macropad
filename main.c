#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "tusb.h"

int main() {

    tusb_init();

    while (true) {
        tud_task();
        //hid_task();
    }

    return 1;
}