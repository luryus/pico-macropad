#include <stdio.h>
#include <string.h>

#include "display_ui.h"

#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "u8g2.h"

#include "constants.h"
#include "log.h"
#include "pico_u8g2_i2c.h"
#include "profiles.h"
#include "usb_hid.h"
#include "utils.h"
#include "version.h"

#define FPS           20
#define FRAME_TIME_US 1000000 / FPS

static bool display_on = true;
static absolute_time_t next_display_off = {0};
static absolute_time_t next_frame = {0};

enum ui_state_t {
    UI_STATE_SCREEN_VERSION,
    UI_STATE_SCREEN_INPUT_DEBUG,
    UI_STATE_SCREEN_MENU,
    UI_STATE_SCREEN_USB_EVENT_SETTING,
    UI_STATE_SCREEN_PROFILE_NAME,
    UI_STATE_SCREEN_KEYMAP,
} current_ui_state = UI_STATE_SCREEN_VERSION;

typedef struct {
    uint16_t key_matrix;
    uint8_t encoder_0;
    bool encoder_0_button;
    uint8_t encoder_1;
    bool encoder_1_button;
} input_state_t;

static volatile input_state_t unsafe_current_input_state;
static input_state_t current_input_state;
static input_state_t previous_input_state;
static volatile bool input_changed;

static uint8_t menu_selected_index = 0;
static uint8_t usb_config_selected_index = 0;

static absolute_time_t profile_name_exit = {0};

static const char *const menu_items[] = {"Debug", "USBConf", "Version"};
#define MENU_ITEMS_TOTAL 3

static u8g2_t u8g2;

static void ui_display_on() {
    u8g2_SetPowerSave(&u8g2, false);
}

static void ui_display_off() {
    u8g2_SetPowerSave(&u8g2, true);
}

static inline bool get_current_key_state(uint8_t y, uint8_t x) {
    return ((current_input_state.key_matrix >> ((2 - y) * 4)) >> x) & 0x1;
}

#pragma region Drawing functions

static void ui_draw_version_screen() {
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_t0_11_mr);
    u8g2_DrawStr(&u8g2, 0, 8, "Macropad");

    char version_str[30] = {0};
    snprintf(version_str, 30, "%s (%.6s)", MACROPAD_VERSION, MACROPAD_VERSION_GIT_SHA);

    u8g2_DrawStr(&u8g2, 0, 20, version_str);
}

static void ui_draw_input_debug_screen() {
    const uint8_t key_side = 6;

    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_t0_11_mr);
    for (uint8_t y = 0; y < MACROPAD_KEY_MATRIX_HEIGHT; y++) {
        for (uint8_t x = 0; x < MACROPAD_KEY_MATRIX_WIDTH; x++) {
            bool key_state = get_current_key_state(y, x);
            uint8_t rect_x = 2 + x * (key_side + 2);
            uint8_t rect_y = 2 + y * (key_side + 2);

            if (key_state) {
                u8g2_DrawBox(&u8g2, rect_x, rect_y, key_side, key_side);
            } else {
                u8g2_DrawFrame(&u8g2, rect_x, rect_y, key_side, key_side);
            }
        }
    }

    u8g2_SetFont(&u8g2, u8g2_font_t0_11_mr);

    char encoder_val[4] = {'\0'};
    snprintf(encoder_val, 4, "%u", current_input_state.encoder_0);
    u8g2_SetDrawColor(&u8g2, current_input_state.encoder_0_button ? 0 : 1);
    u8g2_DrawStr(&u8g2, 60, 8, encoder_val);

    snprintf(encoder_val, 4, "%u", current_input_state.encoder_1);
    u8g2_SetDrawColor(&u8g2, current_input_state.encoder_1_button ? 0 : 1);
    u8g2_DrawStr(&u8g2, 60, 20, encoder_val);
}

static void ui_draw_menu_screen() {
    const uint8_t items_on_row = 3;
    const uint8_t item_w = 40;
    const uint8_t item_h = 8;

    u8g2_SetFont(&u8g2, u8g2_font_t0_11_mr);
    for (uint8_t i = 0; i < MENU_ITEMS_TOTAL; i++) {
        u8g2_SetDrawColor(&u8g2, i == menu_selected_index ? 0 : 1);
        uint8_t row = i / items_on_row;
        uint8_t col = i % items_on_row;

        uint8_t x = col * (item_w + 2);
        uint8_t y = (row + 1) * (item_h + 2);

        u8g2_DrawStr(&u8g2, x, y, menu_items[i]);
    }
}

static void ui_draw_usb_config_screen() {
    bool current_state = usb_hid_is_event_sending_enabled();

    u8g2_SetFont(&u8g2, u8g2_font_t0_11_mr);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_DrawStr(&u8g2, 0, 8, current_state ? "HID events enabled" : "HID events disabled");

    u8g2_SetDrawColor(&u8g2, usb_config_selected_index == 0 ? 0 : 1);
    u8g2_DrawStr(&u8g2, 0, 20, "Enable");

    u8g2_SetDrawColor(&u8g2, usb_config_selected_index == 1 ? 0 : 1);
    u8g2_DrawStr(&u8g2, 64, 20, "Disable");
}

static void ui_draw_profile_name_screen() {
    const char *name = prof_get_current_name();

    u8g2_SetFont(&u8g2, u8g2_font_t0_14_mr);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_DrawStr(&u8g2, 0, 20, name);
}

static void ui_draw_keymap_screen() {
    const uint8_t item_w = 34;
    const uint8_t item_h = 8;

    const char *key_names = prof_get_current_key_names();

    // 4x3 array (width x height)
    // The key_names array contains the names as four-character
    // sequences back to back with no null termination.
    char current_key_name[5] = {0};
    u8g2_SetFont(&u8g2, u8g2_font_t0_11_mr);

    for (uint8_t y = 0; y < MACROPAD_KEY_MATRIX_HEIGHT; y++) {
        for (uint8_t x = 0; x < MACROPAD_KEY_MATRIX_WIDTH; x++) {
            memcpy(
                current_key_name,
                key_names + y * MACROPAD_KEY_MATRIX_WIDTH * MACROPAD_KEY_NAME_LENGTH +
                    x * MACROPAD_KEY_NAME_LENGTH,
                MACROPAD_KEY_NAME_LENGTH);

            bool key_state = get_current_key_state(y, x);

            // If key is down, draw with white background and black text.
            // Otherwise, white text on black.
            u8g2_SetDrawColor(&u8g2, !key_state);

            uint8_t text_x = x * item_w;
            uint8_t text_y = (y + 1) * item_h + (y * MACROPAD_KEY_MATRIX_WIDTH);
            u8g2_DrawStr(&u8g2, text_x, text_y, current_key_name);
        }
    }
}

#pragma endregion

#pragma region Input handling functions

static void ui_handle_input_version_screen(
    __attribute__((unused)) bool button_raising, bool button_falling,
    __attribute__((unused)) int8_t encoder_delta) {
    if (button_falling) {
        // Go back to menu
        current_ui_state = UI_STATE_SCREEN_MENU;
    }
}

static void ui_handle_input_input_debug_screen(
    __attribute__((unused)) bool button_raising, bool button_falling,
    __attribute__((unused)) int8_t encoder_delta) {
    if (button_falling) {
        // Go back to menu
        current_ui_state = UI_STATE_SCREEN_MENU;
    }
}

static void ui_handle_input_usb_config_screen(
    __attribute__((unused)) bool button_raising, bool button_falling, int8_t encoder_delta) {
    while (encoder_delta < 0) {
        encoder_delta += 2;
    }
    if (encoder_delta > 0) {
        usb_config_selected_index = (usb_config_selected_index + encoder_delta) % 2;
    }

    if (button_falling) {
        switch (usb_config_selected_index) {
        case 0:
            usb_hid_set_event_sending_enabled(true);
            break;
        case 1:
            usb_hid_set_event_sending_enabled(false);
            break;
        }
        current_ui_state = UI_STATE_SCREEN_MENU;
    }
}

static void ui_handle_input_menu_screen(
    __attribute__((unused)) bool button_raising, bool button_falling, int8_t encoder_delta) {
    while (encoder_delta < 0) {
        encoder_delta += MENU_ITEMS_TOTAL;
    }
    if (encoder_delta > 0) {
        menu_selected_index = (menu_selected_index + encoder_delta) % MENU_ITEMS_TOTAL;
    }

    if (button_falling) {
        switch (menu_selected_index) {
        case 0:
            current_ui_state = UI_STATE_SCREEN_INPUT_DEBUG;
            break;
        case 1:
            current_ui_state = UI_STATE_SCREEN_USB_EVENT_SETTING;
            break;
        case 2:
            current_ui_state = UI_STATE_SCREEN_VERSION;
            break;
        default:
            LOGW("Unknown menu item selected: %hhu", menu_selected_index);
        }
    }
}

static void ui_handle_input_profile_name_screen(
    __attribute__((unused)) bool button_raising, __attribute__((unused)) bool button_falling,
    __attribute__((unused)) int8_t encoder_delta) {
    // Key events actually not handled for now

    if (time_passed(profile_name_exit)) {
        current_ui_state = UI_STATE_SCREEN_KEYMAP;
    }
}

static void ui_handle_input_keymap_screen(
    __attribute__((unused)) bool button_raising, bool button_falling,
    __attribute__((unused)) int8_t encoder_delta) {
    if (button_falling) {
        // Go back to menu
        current_ui_state = UI_STATE_SCREEN_MENU;
    }
}

#pragma endregion

void ui_init() {
    u8g2_Setup_ssd1306_i2c_128x32_univision_f(
        &u8g2, U8G2_R0, pico_u8g2_byte_i2c, pico_u8g2_delay_cb);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, false);

    next_display_off = make_timeout_time_ms(5000);
}

void ui_task() {
    // Limit FPS to save resources and cycles and stuff
    if (!time_passed(next_frame)) {
        return;
    }
    next_frame = delayed_by_us(next_frame, FRAME_TIME_US);

    current_input_state = unsafe_current_input_state;

    if (input_changed) {
        input_changed = false;
        next_display_off = make_timeout_time_ms(5000);
        if (!display_on) {
            LOGD("Waking display");
            display_on = true;
            ui_display_on();
        }
    } else {
        if (display_on && time_passed(next_display_off)) {
            LOGD("Shutting down display");
            display_on = false;
            ui_display_off();
        }
    }

    bool button_raising =
        current_input_state.encoder_0_button && !previous_input_state.encoder_0_button;
    bool button_falling =
        !current_input_state.encoder_0_button && previous_input_state.encoder_0_button;
    int8_t encoder_delta = (int8_t)(current_input_state.encoder_0 - previous_input_state.encoder_0);

    switch (current_ui_state) {
    case UI_STATE_SCREEN_VERSION:
        ui_handle_input_version_screen(button_raising, button_falling, encoder_delta);
        break;
    case UI_STATE_SCREEN_INPUT_DEBUG:
        ui_handle_input_input_debug_screen(button_raising, button_falling, encoder_delta);
        break;
    case UI_STATE_SCREEN_MENU:
        ui_handle_input_menu_screen(button_raising, button_falling, encoder_delta);
        break;
    case UI_STATE_SCREEN_USB_EVENT_SETTING:
        ui_handle_input_usb_config_screen(button_raising, button_falling, encoder_delta);
        break;
    case UI_STATE_SCREEN_PROFILE_NAME:
        ui_handle_input_profile_name_screen(button_raising, button_falling, encoder_delta);
        break;
    case UI_STATE_SCREEN_KEYMAP:
        ui_handle_input_keymap_screen(button_raising, button_falling, encoder_delta);
        break;
    }

    // Draw
    u8g2_ClearBuffer(&u8g2);
    switch (current_ui_state) {
    case UI_STATE_SCREEN_VERSION:
        ui_draw_version_screen();
        break;
    case UI_STATE_SCREEN_INPUT_DEBUG:
        ui_draw_input_debug_screen();
        break;
    case UI_STATE_SCREEN_MENU:
        ui_draw_menu_screen();
        break;
    case UI_STATE_SCREEN_USB_EVENT_SETTING:
        ui_draw_usb_config_screen();
        break;
    case UI_STATE_SCREEN_PROFILE_NAME:
        ui_draw_profile_name_screen();
        break;
    case UI_STATE_SCREEN_KEYMAP:
        ui_draw_keymap_screen();
        break;
    }
    u8g2_SendBuffer(&u8g2);

    previous_input_state = current_input_state;
}

void ui_set_input_states(
    const uint16_t *key_matrix, const uint8_t *encoder_0, const bool *encoder_0_button,
    const uint8_t *encoder_1, const bool *encoder_1_button) {
    bool c = false;
    if (key_matrix) {
        unsafe_current_input_state.key_matrix = *key_matrix;
        c = true;
    }
    if (encoder_0) {
        unsafe_current_input_state.encoder_0 = *encoder_0;
        c = true;
    }
    if (encoder_0_button) {
        unsafe_current_input_state.encoder_0_button = *encoder_0_button;
        c = true;
    }
    if (encoder_1) {
        unsafe_current_input_state.encoder_1 = *encoder_1;
        c = true;
    }
    if (encoder_1_button) {
        unsafe_current_input_state.encoder_1_button = *encoder_1_button;
        c = true;
    }

    input_changed = c;
}

void ui_trigger_profile_change() {
    // This is called when an USB event is received for a profile change
    if (current_ui_state == UI_STATE_SCREEN_PROFILE_NAME ||
        current_ui_state == UI_STATE_SCREEN_MENU || current_ui_state == UI_STATE_SCREEN_VERSION ||
        current_ui_state == UI_STATE_SCREEN_KEYMAP) {
        LOGD("Showing profile name screen");
        profile_name_exit = make_timeout_time_ms(700);
        current_ui_state = UI_STATE_SCREEN_PROFILE_NAME;
        input_changed = true;
    }
}