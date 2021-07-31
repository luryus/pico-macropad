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
#include "key_matrix.pio.h"
#include "ssd1306_display.h"
#include "log.h"

#define BUTTON_GPIO 14
#define ENCODER_A_GPIO 12
#define ENCODER_B_GPIO 13

#define KEY_MATRIX_ROW_PIN 2
#define KEY_MATRIX_COL_PIN 5

static uint encoder_sm = 0;
static uint key_matrix_sm = 0;

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
    encoder_pio_init(pio0, 0, encoder_sm, offset, ENCODER_A_GPIO);

    irq_set_exclusive_handler(PIO0_IRQ_0, encoder0_isr);
}

static void key_matrix_isr() {
    if (!pio_sm_is_rx_fifo_empty(pio1, key_matrix_sm)) {
        uint32_t data = pio_sm_get_blocking(pio1, key_matrix_sm);
        LOGD("Key matrix data: 0x%x", data);
    }
}

static inline void setup_key_matrix() {

    uint32_t sys_freq = clock_get_hz(clk_sys);
    uint32_t total_debounce_instructions = 
        key_matrix_DEBOUNCE_CYCLE_INSTRUCTIONS * key_matrix_DEBOUNCE_CYCLES;
    
    const uint32_t target_debounce_time_ms = 5;

    uint32_t target_clk_div =
        (target_debounce_time_ms * (sys_freq / 1000)) /* instructions during the target time */
        / total_debounce_instructions;
    uint16_t clk_div = target_clk_div > UINT16_MAX ? UINT16_MAX : target_clk_div;

    float effective_freq = (float) sys_freq / clk_div;

    LOGI("Using clock divider %u (wanted %u):\n"
        "  clk_sys frequency is %u Hz, with divider effective frequency is %f Hz\n"
        "  debounce takes approximately %d instructions\n"
        "  resulting in debounce time %.2f ms (wanted %lu)",
        clk_div, target_clk_div, sys_freq, effective_freq,
        total_debounce_instructions,
        (float) total_debounce_instructions*1000 / effective_freq,
        target_debounce_time_ms);


    uint offset = pio_add_program(pio1, &key_matrix_program);
    key_matrix_sm = pio_claim_unused_sm(pio1, true);
    key_matrix_pio_init(pio1, key_matrix_sm, offset, KEY_MATRIX_ROW_PIN, KEY_MATRIX_COL_PIN, clk_div);

    irq_set_enabled(PIO1_IRQ_0, true);
    pio_set_irq0_source_enabled(pio1, pis_sm0_rx_fifo_not_empty + key_matrix_sm, true);
    irq_set_exclusive_handler(PIO1_IRQ_0, key_matrix_isr);
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

    LOGI("Info");
    LOGD("Debug");
    LOGW("Warning");
    LOGE("Error");

    setup_button();
    setup_encoder();
    setup_key_matrix();
    ssd1306_init();

    bool last_logged_state = false;
    uint8_t display_state = 0;
    uint8_t previous_encoder_val = 0;

    while (true) {
        if (encoder0_rotation != previous_encoder_val) {
            LOGD("Encoder state: 0x%02x", encoder0_rotation);
            previous_encoder_val = encoder0_rotation;

            ssd1306_reset_position();
            for (int i = 0; i < 128; i++) {
                ssd1306_send_data(previous_encoder_val);
            }
        }

        if (is_button_pressed()) {
            if (!last_logged_state) {
                LOGD("Button down");
                last_logged_state = true;
            }
            usb_hid_key_down(0x73);
            usb_hid_key_down(0x72);
        } else {
            if (last_logged_state) {
                LOGD("Button up");
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