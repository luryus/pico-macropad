#if !defined(SSD1306_DISPLAY__H)
#define SSD1306_DISPLAY__H

#define DISPLAY_SDA_PIN 16
#define DISPLAY_SCL_PIN 17
#define DISPLAY_I2C_ADDRESS 0x3c


void ssd1306_init();

void ssd1306_reset_position();

void ssd1306_send_data(uint8_t data);

#endif // SSD1306_DISPLAY__H
