#if !defined(DISPLAY_UI__H)
#define DISPLAY_UI__H

#include <stdbool.h>
#include <stdint.h>

void ui_init();

void ui_set_input_states(
    uint16_t *key_matrix, uint8_t *encoder_0, bool *encoder_0_button, uint8_t *encoder_1,
    bool *encoder_1_button);

void ui_trigger_profile_change();

void ui_task();

#endif // DISPLAY_UI__H
