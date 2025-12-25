#pragma once
#include <stdint.h>
#include <stdbool.h>

void printAscii(int8_t c);
void setDisplayEnabled(bool enabled);
void displayFlushIfNeeded(void);
void displayProcessQueue(void);
void setPrintAsciiHook(void (*hook)(int8_t c));
void printAsciiReset(void);

extern "C" int mini_snprintf(char* buffer, unsigned int buffer_len, const char *fmt, ...);

uint8_t oled_width(void);
uint8_t oled_height(void);
void oled_fill(uint8_t color);
void oled_drawstr8(uint8_t x, uint8_t y, const char *text, uint8_t color);
void oled_drawchar16(uint8_t x, uint8_t y, uint8_t chr, uint8_t color);
void oled_hline(uint8_t y, uint8_t color);
void oled_refresh(void);
