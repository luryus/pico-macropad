#include "usb_hid.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "tusb.h"


int main() {

    stdio_init_all();
    tusb_init();

    while (true) {
        tud_task();
        hid_task();
    }

    return 1;
}