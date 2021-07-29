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
#include "ssd1306_display.h"

#define BUTTON_GPIO 14
#define ENCODER_A_GPIO 12
#define ENCODER_B_GPIO 13


static uint encoder_sm = 0;

static inline void setup_button() {
    gpio_init(BUTTON_GPIO);
    gpio_set_dir(BUTTON_GPIO, false /* in */);
    gpio_pull_up(BUTTON_GPIO);
}

static volatile uint8_t encoder0_rotation = 0;

static void encoder0_isr() {
    int8_t change = 0;
    if (pio_interrupt_get(pio0, 0)) {
        // CCW
        change--;
    }
    if (pio_interrupt_get(pio0, 1)) {
        // CW
        change++;
    }

    encoder0_rotation += change;

    // Reset interrupt flags
    pio_interrupt_clear(pio0, 0);
    pio_interrupt_clear(pio0, 1);
}

static inline void setup_encoder() {
    uint offset = pio_add_program(pio0, &encoder_program);
    encoder_sm = pio_claim_unused_sm(pio0, true);
    encoder_pio_init(pio0, 0, encoder_sm ,offset, ENCODER_A_GPIO);

    irq_set_exclusive_handler(PIO0_IRQ_0, encoder0_isr);
}

static inline bool is_button_pressed() {
    return !gpio_get(BUTTON_GPIO);
}

static inline bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

int main() {

    stdio_init_all();
    //tusb_init();

    setup_button();
    setup_encoder();
    ssd1306_init();

    bool last_logged_state = false;
    uint8_t display_state = 0;
    uint8_t previous_encoder_val = 0;

    while (true) {
        if (encoder0_rotation != previous_encoder_val) {
            printf("Encoder state: 0x%02x\n", encoder0_rotation);
            previous_encoder_val = encoder0_rotation;

            ssd1306_reset_position();
            for (int i = 0; i < 128; i++) {
                ssd1306_send_data(previous_encoder_val);
            }
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