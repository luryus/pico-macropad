#if !defined(DISPLAY_UI__H)
#define DISPLAY_UI__H

#include <stdbool.h>
#include <stdint.h>

void ui_init();

void ui_set_input_states(
    const uint16_t *key_matrix, const uint8_t *encoder_0, const bool *encoder_0_button,
    const uint8_t *encoder_1, const bool *encoder_1_button);

void ui_trigger_profile_change();

void ui_task();

#endif // DISPLAY_UI__H
