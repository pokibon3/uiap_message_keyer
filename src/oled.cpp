#define SSD1306_128X64
#define printf(...) ((void)0)
#include "oled.h"
#include "ch32fun.h"
#include "ssd1306_i2c.h"
#include "ssd1306.h"
#undef printf

void Oled::init()
{
    ssd1306_i2c_init();
    ssd1306_init();
}

uint8_t Oled::width() const
{
    return SSD1306_W;
}

uint8_t Oled::height() const
{
    return SSD1306_H;
}

void Oled::fill(uint8_t color)
{
    ssd1306_fillRect(0, 0, SSD1306_W, SSD1306_H, color);
}

void Oled::drawStr8(uint8_t x, uint8_t y, const char *text, uint8_t color)
{
    ssd1306_drawstr_sz(x, y, (char *)text, color, fontsize_8x8);
}

void Oled::drawChar16(uint8_t x, uint8_t y, uint8_t chr, uint8_t color)
{
    ssd1306_drawchar_sz(x, y, chr, color, fontsize_16x16);
}

void Oled::hline(uint8_t y, uint8_t color)
{
    ssd1306_drawFastHLine(0, y, SSD1306_W, color);
}

void Oled::refresh()
{
    ssd1306_refresh();
}
