#if !defined(PICO_U8G2_I2C__H)
#define PICO_U8G2_I2C__H

#include "u8x8.h"
#include <stdint.h>

uint8_t pico_u8g2_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t pico_u8g2_byte_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#endif // PICO_U8G2_I2C__H
