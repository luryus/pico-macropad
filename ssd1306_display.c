#include <stdint.h>
#include <stdio.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "ssd1306_display.h"


#define SSD1306_CONTROL_COMMAND 0x00
#define SSD1306_CONTROL_DATA 0x40

#define SSD1306_COMMAND_ENTIRE_DISPLAY_ON 0xa5
#define SSD1306_COMMAND_ENTIRE_DISPLAY_ON_RESUME_RAM 0xa4

#define SSD1306_COMMAND_DISPLAY_ON 0xAF
#define SSD1306_COMMAND_DISPLAY_OFF 0xAE

#define SSD1306_COMMAND_SET_ADDRESSING_MODE 0x20
#define SSD1306_ADDRESSING_MODE_HORIZONTAL 0x0
#define SSD1306_ADDRESSING_MODE_VERTICAL 0x1
#define SSD1306_ADDRESSING_MODE_PAGE 0x2

#define SSD1306_COMMAND_SET_SEGMENT_REMAP 0xA0
#define SSD1306_COMMAND_SET_COM_SCAN_DESC 0xc8

#define SSD1306_COMMAND_SET_COLUMN_ADDRESS 0x21
#define SSD1306_COMMAND_SET_PAGE_ADDRESS 0x22
#define SSD1306_COMMAND_SET_COM_PINS 0xda

#define SSD1306_COMMAND_SET_CHARGE_PUMP_CONFIGURATION 0x8d
#define SSD1306_CHARGE_PUMP_ENABLED 0x14



static inline void ssd1306_send_command(uint8_t command) {
    uint8_t data[] = { SSD1306_CONTROL_COMMAND, command };
    int ret = i2c_write_blocking(i2c0, DISPLAY_I2C_ADDRESS, data, 2, false);

    #ifndef NDEBUG
    if (ret != 2) {
        printf("Writing I2C data failed: %d\n", ret);
    }
    #endif
}

void ssd1306_send_data(uint8_t data) {
    uint8_t d[] = { SSD1306_CONTROL_DATA, data };
    int ret = i2c_write_blocking(i2c0, DISPLAY_I2C_ADDRESS, d, 2, false);

    #ifndef NDEBUG
    if (ret != 2) {
        printf("Writing I2C data failed: %d\n", ret);
    }
    #endif
}

static inline void ssd1306_setup_i2c() {
    i2c_init(i2c0, 400 * 1000);

    gpio_set_function(DISPLAY_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(DISPLAY_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(DISPLAY_SDA_PIN);
    gpio_pull_up(DISPLAY_SCL_PIN);
}

void ssd1306_init() {

    ssd1306_setup_i2c();

    ssd1306_send_command(SSD1306_COMMAND_SET_CHARGE_PUMP_CONFIGURATION);
    ssd1306_send_command(SSD1306_CHARGE_PUMP_ENABLED);

    ssd1306_send_command(SSD1306_COMMAND_DISPLAY_ON);

    ssd1306_send_command(SSD1306_COMMAND_SET_ADDRESSING_MODE);
    ssd1306_send_command(SSD1306_ADDRESSING_MODE_HORIZONTAL);

    ssd1306_send_command(SSD1306_COMMAND_SET_SEGMENT_REMAP | 0x01);
    ssd1306_send_command(SSD1306_COMMAND_SET_COM_SCAN_DESC);

    ssd1306_send_command(SSD1306_COMMAND_SET_COM_PINS);
    ssd1306_send_command(0x02);


    ssd1306_send_command(SSD1306_COMMAND_SET_PAGE_ADDRESS);
    ssd1306_send_command(0);
    ssd1306_send_command(7);

    // Clear memory
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 128; j++) {
            ssd1306_send_data(0x00);
        }
    }

    ssd1306_reset_position();

    ssd1306_send_command(SSD1306_COMMAND_ENTIRE_DISPLAY_ON_RESUME_RAM);
}

void ssd1306_reset_position() {
    ssd1306_send_command(SSD1306_COMMAND_SET_PAGE_ADDRESS);
    ssd1306_send_command(4);
    ssd1306_send_command(7);

    ssd1306_send_command(SSD1306_COMMAND_SET_COLUMN_ADDRESS);
    ssd1306_send_command(0);
    ssd1306_send_command(127);
}