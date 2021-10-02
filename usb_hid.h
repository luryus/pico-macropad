#if !defined(USB_HID__H)
#define USB_HID__H

#include <stdbool.h>
#include <stdint.h>

#define USB_HID_REPORT_NUM_KEYPAD  1
#define USB_HID_REPORT_NUM_ENCODER 2

void hid_task();

void usb_hid_set_keys(uint16_t key_states);

void usb_hid_set_encoder_rotation(uint8_t rot);

bool usb_hid_is_event_sending_enabled();

void usb_hid_set_event_sending_enabled(bool enabled);

#endif // USB_HID__H
