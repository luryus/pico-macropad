#include "usb_hid.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "tusb.h"
#include "encoder.pio.h"

#define BUTTON_GPIO 14
#define ENCODER_A_GPIO 12
#define ENCODER_B_GPIO 13



static uint encoder_sm = 0;

static inline void setup_button() {
    gpio_init(BUTTON_GPIO);
    gpio_set_dir(BUTTON_GPIO, false /* in */);
    gpio_pull_up(BUTTON_GPIO);
}

static inline void setup_encoder() {
    uint offset = pio_add_program(pio0, &encoder_program);
    encoder_sm = pio_claim_unused_sm(pio0, true);
    encoder_pio_init(pio0, encoder_sm ,offset, ENCODER_A_GPIO);
}

static inline bool is_button_pressed() {
    return !gpio_get(BUTTON_GPIO);
}

static inline void setup_display() {

}

static uint8_t encoder_val = 0;

static inline uint8_t get_encoder_state() {
    if (pio_sm_is_rx_fifo_empty(pio0, encoder_sm)) {
        return 0;
    }

    uint32_t val = pio_sm_get(pio0, encoder_sm);
    if (val == 0x02) {
        encoder_val++;
    }
    else if (val == 0x03) {
        encoder_val--;
    }
    printf("Got raw value 0x%02x\n", val);
    return (uint8_t) encoder_val;
}

int main() {

    stdio_init_all();
    //tusb_init();

    setup_button();
    setup_encoder();

    bool last_logged_state = false;

    while (true) {
        //sleep_ms(200);
        uint8_t encoder = get_encoder_state();
        if (encoder) {
            printf("Encoder state: 0x%02x\n", encoder_val);
        }

        if (is_button_pressed()) {
            if (!last_logged_state) {
                printf("Button down\n");
                last_logged_state = true;
            }
            usb_hid_key_down(0x73);
            usb_hid_key_down(0x72);
        } else {
            if (last_logged_state) {
                printf("Button up\n");
                last_logged_state = false;
            }
            usb_hid_key_up(0x73);
            usb_hid_key_up(0x72);
        }

        tud_task();
        //hid_task();
    }

    return 1;
}