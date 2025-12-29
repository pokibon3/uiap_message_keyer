#pragma once
#include <stdint.h>

class Oled {
public:
    void init();
    uint8_t width() const;
    uint8_t height() const;
    void fill(uint8_t color);
    void drawStr8(uint8_t x, uint8_t y, const char *text, uint8_t color);
    void drawChar16(uint8_t x, uint8_t y, uint8_t chr, uint8_t color);
    void hline(uint8_t y, uint8_t color);
    void refresh();
};

extern Oled g_oled;
