#include "usb_hid.h"

#include "display_ui.h"
#include "encoder.pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "key_matrix.pio.h"
#include "log.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "utils.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define ENCODER_0_A_GPIO      10
#define ENCODER_0_B_GPIO      11
#define ENCODER_0_BUTTON_GPIO 12

#define ENCODER_1_A_GPIO      13
#define ENCODER_1_B_GPIO      14
#define ENCODER_1_BUTTON_GPIO 15

#define KEY_MATRIX_ROW_PIN 2
#define KEY_MATRIX_COL_PIN 5

static uint encoder0_sm = 0;
static uint encoder1_sm = 0;
static uint key_matrix_sm = 0;

static volatile uint8_t encoder0_rotation = 0;
static volatile uint8_t encoder1_rotation = 0;

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

    uint8_t rot = encoder0_rotation;
    ui_set_input_states(NULL, &rot, NULL, NULL, NULL);
}

static void encoder1_isr() {
    int8_t change = 0;
    if (pio_interrupt_get(pio0, 2)) {
        // CW
        change--;
    }
    if (pio_interrupt_get(pio0, 3)) {
        // CCW
        change++;
    }

    encoder1_rotation += change;

    // Reset interrupt flags
    pio_interrupt_clear(pio0, 2);
    pio_interrupt_clear(pio0, 3);

    uint8_t rot = encoder1_rotation;
    ui_set_input_states(NULL, NULL, NULL, &rot, NULL);
    usb_hid_set_encoder_rotation(rot);
}

static inline void setup_encoders() {
    encoder0_sm = 0;
    encoder1_sm = 2;

    pio_claim_sm_mask(pio0, 5); // Claims SMs 0 and 2 (0b101)

    uint offset = pio_add_program(pio0, &encoder_program);
    encoder_pio_init(pio0, 0, encoder0_sm, offset, ENCODER_0_A_GPIO);
    irq_set_exclusive_handler(PIO0_IRQ_0, encoder0_isr);

    offset = pio_add_program(pio0, &encoder_program);
    encoder_pio_init(pio0, 1, encoder1_sm, offset, ENCODER_1_A_GPIO);
    irq_set_exclusive_handler(PIO0_IRQ_1, encoder1_isr);

    // Setup the encoder buttons
    gpio_init(ENCODER_0_BUTTON_GPIO);
    gpio_pull_up(ENCODER_0_BUTTON_GPIO);
    gpio_init(ENCODER_1_BUTTON_GPIO);
    gpio_pull_up(ENCODER_1_BUTTON_GPIO);
}

static void key_matrix_isr() {
    if (!pio_sm_is_rx_fifo_empty(pio1, key_matrix_sm)) {
        uint16_t data = (uint16_t)pio_sm_get_blocking(pio1, key_matrix_sm);
        usb_hid_set_keys(data);
        ui_set_input_states(&data, NULL, NULL, NULL, NULL);
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

    float effective_freq = (float)sys_freq / clk_div;

    LOGI(
        "Using clock divider %u (wanted %u):\n"
        "  clk_sys frequency is %u Hz, with divider effective frequency is %f Hz\n"
        "  debounce takes approximately %d instructions\n"
        "  resulting in debounce time %.2f ms (wanted %lu)",
        clk_div, target_clk_div, sys_freq, effective_freq, total_debounce_instructions,
        (float)total_debounce_instructions * 1000 / effective_freq, target_debounce_time_ms);

    uint offset = pio_add_program(pio1, &key_matrix_program);
    key_matrix_sm = pio_claim_unused_sm(pio1, true);
    key_matrix_pio_init(
        pio1, key_matrix_sm, offset, KEY_MATRIX_ROW_PIN, KEY_MATRIX_COL_PIN, clk_div);

    irq_set_enabled(PIO1_IRQ_0, true);
    pio_set_irq0_source_enabled(pio1, pis_sm0_rx_fifo_not_empty + key_matrix_sm, true);
    irq_set_exclusive_handler(PIO1_IRQ_0, key_matrix_isr);
}

typedef struct {
    bool stable_state;
    bool debouncing;
    absolute_time_t debounce_end;
} debounce_state_t;

// Returns true if the button state was changed
static bool check_button_debounced(uint8_t gpio, debounce_state_t *state) {
    bool s = !gpio_get(gpio);
    if (s != state->stable_state) {
        if (state->debouncing) {
            if (time_passed(state->debounce_end)) {
                state->debouncing = false;
                state->stable_state = s;
                return true;
            }
        } else {
            state->debounce_end = make_timeout_time_ms(5);
            state->debouncing = true;
        }
    } else {
        if (state->debouncing) {
            state->debouncing = false;
        }
    }

    return false;
}

static void check_encoder_buttons() {
    static debounce_state_t encoder0_debounce_state = {0};
    static debounce_state_t encoder1_debounce_state = {0};

    if (check_button_debounced(ENCODER_0_BUTTON_GPIO, &encoder0_debounce_state)) {
        ui_set_input_states(NULL, NULL, &encoder0_debounce_state.stable_state, NULL, NULL);
    }
    if (check_button_debounced(ENCODER_1_BUTTON_GPIO, &encoder1_debounce_state)) {
        ui_set_input_states(NULL, NULL, NULL, NULL, &encoder1_debounce_state.stable_state);
    }
}

int main() {

    stdio_init_all();
    tusb_init();

    setup_encoders();
    setup_key_matrix();

    ui_init();

    bool last_logged_state = false;
    bool display_on = true;
    absolute_time_t next_display_off = make_timeout_time_ms(5000);
    uint8_t previous_encoder_val = 255;

    while (true) {
        check_encoder_buttons();
        tud_task();
        hid_task();
        ui_task();
    }

    return 1;
}