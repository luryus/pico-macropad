#include "pico_u8g2_i2c.h"

#include "hardware/i2c.h"
#include "log.h"
#include "pico/stdlib.h"
#include "u8g2.h"
#include <stdint.h>

#define DISPLAY_SDA_PIN 16
#define DISPLAY_SCL_PIN 17
#define I2C_INSTANCE    i2c0

uint8_t pico_u8g2_delay_cb(
    u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, __attribute__((unused)) void *arg_ptr) {
    // Only implements the necessary messages for using Pico's built-in i2c

    switch (msg) {
    case U8X8_MSG_DELAY_MILLI:
        // arg_int * 1 ms delay
        sleep_ms(arg_int);
        break;
    default:
        LOGW("pico_u8g2_delay_cb called with unimplemented msg %u", msg);
        u8x8_SetGPIOResult(u8x8, 1); // default return value
        break;
    }
    return 1;
}

static void init_i2c() {
    i2c_init(I2C_INSTANCE, 400 * 1000);
    gpio_set_function(DISPLAY_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(DISPLAY_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(DISPLAY_SDA_PIN);
    gpio_pull_up(DISPLAY_SCL_PIN);
}

uint8_t pico_u8g2_byte_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    static uint8_t buffer[32]; // At most 32 bytes sent between start/end transfer
    static uint8_t buffer_size = 0;

    switch (msg) {
    case U8X8_MSG_BYTE_INIT:
        init_i2c();
        break;

    case U8X8_MSG_BYTE_SEND:
        // Store the data to the buffer, later sent when transfer ends

        // arg_ptr: pointer to first byte in data array
        // arg_int: size of the data array
        uint8_t *data_ptr = (uint8_t *)arg_ptr;
        while (arg_int-- > 0) {
            assert(buffer_size < 32);
            buffer[buffer_size++] = *(data_ptr++);
        }
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        // Reset buffer
        buffer_size = 0;
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        // Send the data
        assert(buffer_size > 0);
        uint8_t address = u8x8_GetI2CAddress(u8x8) >> 1;
        int written = i2c_write_blocking(I2C_INSTANCE, address, buffer, buffer_size, false);
        if (written != buffer_size) {
            LOGE("Error writing i2c data: %d", written);
            return 0;
        }
        break;

    default:
        LOGW("pico_u8g2_byte_i2c called with unimplemented message: %d", msg);
        return 0;
    }
    return 1;
}