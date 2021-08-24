#include "tusb.h"
#include "pico/unique_id.h"

#include "usb_hid.h"

/// Device descriptor
/// ==================

static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    // Use Raspberry Pi Ltd. vendor id
    .idVendor = 0x2e8a,
    .idProduct = 0xffee,
    .bcdDevice = 0x0100,

    // Indices to string descriptions

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01
};

// This callback is invoked when a GET DEVICE DESCRIPTOR
// request is received. Return a pointer to the descriptor
// structure.
uint8_t const* tud_descriptor_device_cb() {
    return (uint8_t const*) &desc_device;
}


/// String descriptor
/// =================

static char const* strings[] = {
    // The first string descriptor defines the supoprted languages
    // Just support english (0x0409)
    (const char[]) { 0x09, 0x04 },   // 0: supported lang (en)
    "Luryus",                        // 1: Manufacturer,
    "Pico Macropad",                 // 2: Product,
    NULL                             // 3: Serial number    
};

static const uint8_t STRING_INDEX_LANG = 0;
static const uint8_t STRING_INDEX_SERIAL_NUMBER = 3;

#define SERIAL_NUMBER_STRING_LEN (PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1)

static const char* get_serial_string() {
    static char serial_buffer[SERIAL_NUMBER_STRING_LEN];
    static bool initialized = false;

    if (!initialized) {
        pico_get_unique_board_id_string(serial_buffer, SERIAL_NUMBER_STRING_LEN);
        initialized = true;
    }

    return (const char*) serial_buffer;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t lang_id) {
    // lang_id is ignored, we just support English

    assert(index >= 0 && index <= 3);

    // The description to return.
    // It must be stay valid during transmission; thus static.
    // Cap length at 31 + (2-byte header + 31 byte string).
    static uint16_t descriptor_data[32];
    uint8_t data_len = 0;

    // Temporary pointer to string that will be converted
    // to utf-16 later
    const char* ascii_data = NULL;

    switch (index) {
        case STRING_INDEX_LANG:
            // No need to do any UTF-16 conversion, just
            // return the data
            data_len = 1;
            // Copy the two-byte language descriptor
            memcpy(descriptor_data + 1, strings[0], 2);
            break;
        case STRING_INDEX_SERIAL_NUMBER:
            ascii_data = get_serial_string();
            data_len = SERIAL_NUMBER_STRING_LEN;
            break;
        default:
            ascii_data = strings[index];
            data_len = strlen(ascii_data);
            break;
    }

    if (ascii_data != NULL) {
        if (data_len > 31) {
            data_len = 31;
        }

        for (uint8_t i = 0; i < data_len; ++i) {
            // ASCII -> UTF-16: just set the ascii value to lower byte
            // and higher byte to 0
            descriptor_data[i+1] = (uint16_t) ascii_data[i];
        }
    }

    // Set the descriptor header:
    // Offset 0: Length (in bytes)
    // Offset 1: Descriptor type == string descriptor == 0x03
    
    descriptor_data[0] = (TUSB_DESC_STRING << 8)
                         | 2 + 2*data_len;   // Two-byte header + utf-16 data

    return descriptor_data;
}

/// HID Report Descriptor
/// =====================

uint8_t const hid_report_descriptor[] = {

    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
    HID_USAGE(HID_USAGE_DESKTOP_KEYPAD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
        // 12 bits, one for each key
        HID_USAGE_PAGE(HID_USAGE_PAGE_KEYBOARD),
            HID_USAGE_MIN(0x68),
            HID_USAGE_MAX(0x73),
            HID_LOGICAL_MIN(0),
            HID_LOGICAL_MAX(1),
            HID_REPORT_COUNT(12),
            HID_REPORT_SIZE(1),
            HID_INPUT(HID_DATA|HID_VARIABLE|HID_ABSOLUTE),
            
            // 4 bit padding
            HID_REPORT_COUNT(1),
            HID_REPORT_SIZE(4),
            HID_INPUT(HID_CONSTANT),

    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
            HID_USAGE(HID_USAGE_DESKTOP_DIAL),
            HID_LOGICAL_MIN(0x81),
            HID_LOGICAL_MAX(0x7f),
            HID_REPORT_COUNT(1),
            HID_REPORT_SIZE(8),
            HID_INPUT(HID_DATA|HID_VARIABLE|HID_RELATIVE),
    HID_COLLECTION_END
};

uint8_t const* tud_hid_descriptor_report_cb(uint8_t interface) {
    return hid_report_descriptor;
}


/// Configuration Descriptor
/// ========================

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

// MSB: direction IN
// bits 0-2: device number (0b001)
#define EPNUM 0x81

uint8_t const configuration_descriptor[] = {

    TUD_CONFIG_DESCRIPTOR(
        1,   // Configuration number. Only one configuration ==> 1
        1,   // Interface count. Just one for now
        0,   // String index. Zero, as no description required
        CONFIG_TOTAL_LEN,
        0,   // Attributes. None required for this
        100  // Pull 100mA max
    ),

    TUD_HID_DESCRIPTOR(
        0,   // Only one interface, so number is 0
        0,   // String index. Again, none required
        HID_ITF_PROTOCOL_NONE,  // do not try to conform to a keyboard protocol
        sizeof(hid_report_descriptor),
        EPNUM,
        CFG_TUD_HID_EP_BUFSIZE,
        5
    )

};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return configuration_descriptor;
}