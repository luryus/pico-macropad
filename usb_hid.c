#include "usb_hid.h"

#include "tusb.h"
#include "log.h"

static volatile uint16_t curr_key_states = 0x0;
static volatile uint8_t curr_encoder_rot = 0x0;
static volatile bool dirty = true;

void usb_hid_set_keys(uint16_t key_states) {
    dirty = true;
    curr_key_states = key_states & 0x0fff;
}

void usb_hid_set_encoder_rotation(uint8_t rot) {
    dirty = true;
    curr_encoder_rot = rot;
}

uint16_t tud_hid_get_report_cb(
    uint8_t itf, 
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t* buffer,
    uint16_t reqlen)
{
    return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{

}

typedef struct __attribute__((packed)) {
    uint16_t keys;
    uint8_t encoder_rot;
} hid_report_t;

static void send_keyboard_hid_report()
{
    if (!tud_hid_ready()) {
        return;
    }

    if (!dirty) {
        return;
    }

    hid_report_t rep = {
        .keys = curr_key_states,
        .encoder_rot = curr_encoder_rot
    };

    dirty = false;
    LOGD("Sending keys: 0x%03x, encoder rot: 0x%02x", rep.keys, rep.encoder_rot);
    return;
    bool send_report_res = tud_hid_n_report(0, 0, &rep, sizeof(rep));
    if (!send_report_res) {
        LOGW("Failed to send keyboard report");
    }
}

void hid_task()
{
    const uint8_t REPORT_SEND_INTERVAL_MS = 10;

    static absolute_time_t next_send_time = {0};

    if (absolute_time_diff_us(get_absolute_time(), next_send_time) > 0) {
        return;
    }
    next_send_time = delayed_by_ms(next_send_time, REPORT_SEND_INTERVAL_MS);

    send_keyboard_hid_report();
}

void tud_hid_report_complete_cb(uint8_t interface, uint8_t const* report, uint8_t len)
{

}

