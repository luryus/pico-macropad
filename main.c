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
#include "log.h"
#include "pico_u8g2_i2c.h"
#include "u8g2.h"

#define BUTTON_GPIO 14
#define ENCODER_A_GPIO 12
#define ENCODER_B_GPIO 13

#define KEY_MATRIX_ROW_PIN 2
#define KEY_MATRIX_COL_PIN 5

static uint encoder_sm = 0;
static uint key_matrix_sm = 0;

static u8g2_t u8g2;

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

static inline bool setup_u8g2() {
    u8g2_Setup_ssd1306_i2c_128x32_univision_f(
        &u8g2,
        U8G2_R0,
        pico_u8g2_byte_i2c,
        pico_u8g2_delay_cb
    );
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, false);
    u8g2_SetFont(&u8g2, u8g2_font_t0_11_mr);

    u8g2_DrawStr(&u8g2, 0, 8, "Macropad");
    u8g2_DrawStr(&u8g2, 0, 20, "Firmware 0.1");

    u8g2_SendBuffer(&u8g2);
}

static void display_off() {
    u8g2_SetPowerSave(&u8g2, true);
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
    setup_u8g2();

    bool last_logged_state = false;
    bool display_on = true;
    absolute_time_t next_display_off = make_timeout_time_ms(5000);
    uint8_t previous_encoder_val = 255;

    while (true) {
        if (display_on && absolute_time_diff_us(next_display_off, get_absolute_time()) > 0) {
            display_off();
            display_on = false;
        }

        if (encoder0_rotation != previous_encoder_val) {
            LOGD("Encoder state: 0x%02x", encoder0_rotation);
            previous_encoder_val = encoder0_rotation;

            //u8g2_ClearBuffer(&u8g2);
            
            char buf[4];
            snprintf(buf, 4, "%u", encoder0_rotation);
            buf[3] = '\0';
            //u8g2_DrawStr(&u8g2, 0, 0, buf);
            //u8g2_SendBuffer(&u8g2);
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