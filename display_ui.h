#if !defined(DISPLAY_UI__H)
#define DISPLAY_UI__H

#include <stdint.h>
#include <stdbool.h>

void ui_init();

void ui_set_input_states(
    uint16_t* key_matrix,
    uint8_t* encoder_0,
    bool* encoder_0_button,
    uint8_t* encoder_1,
    bool* encoder_1_button
);

void ui_task();


#endif // DISPLAY_UI__H
