#include "print_ascii.h"

#include <stdint.h>

typedef enum {
    fontsize_8x8 = 1,
    fontsize_16x16 = 2,
    fontsize_32x32 = 4,
    fontsize_64x64 = 8,
} font_size_t;

void ssd1306_drawchar_sz(uint8_t x, uint8_t y, uint8_t chr, uint8_t color, font_size_t font_size);
void ssd1306_refresh(void);

#define FONT_WIDTH 12
#define FONT_COLOR 1
#define LINE_HEIGHT 16
#define FONT_SCALE_16X16 fontsize_16x16
const int colums = 10; /// have to be 16 or 20

static int lcdindex = 0;
static uint8_t line1[colums];
static uint8_t line2[colums];
static bool display_enabled = true;
static bool display_dirty = false;

static void printAsc(int8_t asciinumber)
{
    if (lcdindex > colums - 1){
        lcdindex = 0;
        for (int i = 0; i <= colums - 1 ; i++){
            ssd1306_drawchar_sz(i * FONT_WIDTH , LINE_HEIGHT, line2[i], FONT_COLOR, FONT_SCALE_16X16);
            line2[i] = line1[i];
        }
        for (int i = 0; i <= colums - 1 ; i++){
            ssd1306_drawchar_sz(i * FONT_WIDTH , LINE_HEIGHT * 2, line1[i], FONT_COLOR, FONT_SCALE_16X16);
            ssd1306_drawchar_sz(i * FONT_WIDTH , LINE_HEIGHT * 3, 32, FONT_COLOR, FONT_SCALE_16X16);
        }
    }
    line1[lcdindex] = asciinumber;
    ssd1306_drawchar_sz(lcdindex * FONT_WIDTH , LINE_HEIGHT * 3, asciinumber, FONT_COLOR, FONT_SCALE_16X16);
    if (display_enabled) {
        ssd1306_refresh();
    } else {
        display_dirty = true;
    }
    lcdindex += 1;
}

void printAscii(int8_t c)
{
    switch (c) {
        case 'b': // BT
            printAsc('B');
            printAsc('T');
            break;
        case 'a':   // AR
            printAsc('A');
            printAsc('R');
            break;
        case 'k':   // KN
            printAsc('K');
            printAsc('N');
            break;
        case 'v':   // VA
            printAsc('V');
            printAsc('A');
            break;
        default:
            printAsc(c);
            break;
    }
}

void setDisplayEnabled(bool enabled)
{
    display_enabled = enabled;
    if (display_enabled && display_dirty) {
        display_dirty = false;
        ssd1306_refresh();
    }
}
