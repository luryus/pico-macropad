#include "usb_hid.h"

#include "display_ui.h"
#include "log.h"
#include "profiles.h"
#include "tusb.h"
#include "utils.h"

#include <stdlib.h>

static volatile uint16_t curr_key_states = 0x0;
static volatile uint8_t curr_encoder_rot = 0x0;
static volatile bool curr_encoder_btn = false;
static volatile bool keypad_dirty = true;
static volatile bool encoder_dirty = true;

static bool event_sending_enabled = false;

void usb_hid_set_keys(uint16_t key_states) {
    keypad_dirty = true;
    curr_key_states = key_states & 0x0fff;
}

void usb_hid_set_encoder_rotation(uint8_t rot) {
    encoder_dirty = true;
    curr_encoder_rot = rot;
}

void usb_hid_set_encoder_button(bool state) {
    encoder_dirty = true;
    curr_encoder_btn = state;
    LOGD("set_encoder_button %d", state);
}

uint16_t tud_hid_get_report_cb(
    uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer,
    uint16_t reqlen) {
    return 0;
}

void tud_hid_set_report_cb(
    uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer,
    uint16_t bufsize) {

    LOGD(
        "tud_hid_set_report_cb: report_id %hhu, report_type %u, bufsize %hu", report_id,
        (uint8_t)report_type, bufsize);
    char *buf = (char *)calloc(2 * bufsize + 1, sizeof(char));
    for (int i = 0; i < bufsize; i++) {
        snprintf(buf + i * 2, 3, "%02X", buffer[i]);
    }
    LOGD("%s", buf);
    free(buf);

    if (report_id == 3) {
        if (bufsize != 9) {
            LOGW("Invalid report 3 (profile name) message, len %d", bufsize);
            return;
        }
        prof_set_current_profile_name(buffer + 1);
        ui_trigger_profile_change();
    } else if (report_id == 4) {
        if (bufsize != 48 + 1) {
            LOGW("Invalid report 4 (key names) message, len %d", bufsize);
            return;
        }
        prof_set_current_key_names(buffer + 1);
    }
}

typedef struct __attribute__((packed)) {
    uint16_t keys;
} hid_report_keypad_t;

typedef struct __attribute__((packed)) {
    uint8_t encoder_rot;
    uint8_t button;
} hid_report_encoder_t;

static void send_keyboard_hid_report() {
    if (!tud_hid_ready()) {
        return;
    }

    if (!keypad_dirty) {
        return;
    }

    hid_report_keypad_t rep = {.keys = curr_key_states};

    keypad_dirty = false;
    LOGD("Sending keys: 0x%03x", rep.keys);

    if (!event_sending_enabled) {
        return;
    }
    bool send_report_res = tud_hid_n_report(0, 1, &rep, sizeof(rep));
    if (!send_report_res) {
        LOGW("Failed to send keyboard report");
    }
}

static void send_encoder_hid_report() {
    if (!tud_hid_ready()) {
        return;
    }

    if (!encoder_dirty) {
        return;
    }

    LOGD("send_encoder_hid_report: curr_encoder_btn: %d", curr_encoder_btn);

    hid_report_encoder_t rep = {
        .encoder_rot = curr_encoder_rot, .button = curr_encoder_btn ? 0x01 : 0x00};

    encoder_dirty = false;
    LOGD("Sending encoder: 0x%02x, button: 0x%02x", rep.encoder_rot, rep.button);

    if (!event_sending_enabled) {
        return;
    }
    bool send_report_res = tud_hid_n_report(0, 2, &rep, sizeof(rep));
    if (!send_report_res) {
        LOGW("Failed to send encoder report");
    }
}

void hid_task() {
    const uint8_t REPORT_SEND_INTERVAL_MS = 10;

    static absolute_time_t next_send_time = {0};

    if (!time_passed(next_send_time)) {
        return;
    }
    next_send_time = delayed_by_ms(next_send_time, REPORT_SEND_INTERVAL_MS);

    send_keyboard_hid_report();
    send_encoder_hid_report();
}

void tud_hid_report_complete_cb(uint8_t interface, uint8_t const *report, uint8_t len) {
}

bool usb_hid_is_event_sending_enabled() {
    return event_sending_enabled;
}

void usb_hid_set_event_sending_enabled(bool enabled) {
    event_sending_enabled = enabled;
}