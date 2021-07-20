#if !defined(USB_HID__H)
#define USB_HID__H

#include <stdint.h>

void hid_task();

void usb_hid_key_down(uint8_t keycode);

void usb_hid_key_up(uint8_t keycode);


#endif // USB_HID__H
