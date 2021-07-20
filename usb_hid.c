#include "usb_hid.h"

#include "tusb.h"

static uint8_t keycodes[6] = { 0 };
static uint8_t key_count = 0;

void usb_hid_key_down(uint8_t keycode) {
    if (key_count >= 6) {
        return;
    }

    uint8_t place = UINT8_MAX;
    for (uint8_t i = 0; i < 6; i++) {
        if (keycodes[i] == keycode) {
            return;
        }

        if (keycodes[i] == 0) {
            place = i;
        }
    }

    assert(place != UINT8_MAX);
    keycodes[place] = keycode;
    key_count++;
}

void usb_hid_key_up(uint8_t keycode) {
    if (key_count == 0) {
        return;
    }

    for (uint8_t i = 0; i < 6; i++) {
        if (keycodes[i] == keycode) {
            keycodes[i] = 0;
            key_count--;
            return;
        }
    }
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

static void send_keyboard_hid_report()
{
    if (!tud_hid_ready()) {
        return;
    }

    // itf and report_id can be ignored, as there's
    // only one interface and report

    static bool previous_report_empty = false;

    if (key_count == 0 && previous_report_empty) {
        return;
    }
    else if (key_count == 0) {
        previous_report_empty = true;
    }
    else {
        previous_report_empty = false;
    }

    printf("Sending keycodes: %x, %x, %x, %x, %x, %x \n",
            keycodes[0], keycodes[1], keycodes[2], keycodes[3], keycodes[4], keycodes[5]);
    tud_hid_keyboard_report(0, 0, keycodes);
}

void hid_task()
{
#define REPORT_SEND_INTERVAL_MS 10

    static uint32_t next_send_start_ms = 0;

    if (next_send_start_ms > to_ms_since_boot(get_absolute_time())) {
        return;
    }
    next_send_start_ms += REPORT_SEND_INTERVAL_MS;

    send_keyboard_hid_report();
}

void tud_hid_report_complete_cb(uint8_t interface, uint8_t const* report, uint8_t len)
{

}

