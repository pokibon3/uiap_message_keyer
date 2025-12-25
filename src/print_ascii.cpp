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

// Simple SPSC queue for ISR-safe character enqueue
#define PRINT_Q_SIZE 64
static volatile uint8_t q_head = 0;
static volatile uint8_t q_tail = 0;
static uint8_t q_buf[PRINT_Q_SIZE];

static void enqueueChar(int8_t c)
{
    uint8_t head = q_head;
    uint8_t next = (uint8_t)((head + 1) % PRINT_Q_SIZE);
    if (next == q_tail) {
        return; // drop on overflow
    }
    q_buf[head] = (uint8_t)c;
    q_head = next;
}

static bool dequeueChar(int8_t *out)
{
    uint8_t tail = q_tail;
    if (tail == q_head) {
        return false;
    }
    *out = (int8_t)q_buf[tail];
    q_tail = (uint8_t)((tail + 1) % PRINT_Q_SIZE);
    return true;
}

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
    display_dirty = true;
    lcdindex += 1;
}

void printAscii(int8_t c)
{
    switch (c) {
        case 'b': // BT
            enqueueChar('B');
            enqueueChar('T');
            break;
        case 'a':   // AR
            enqueueChar('A');
            enqueueChar('R');
            break;
        case 'k':   // KN
            enqueueChar('K');
            enqueueChar('N');
            break;
        case 'v':   // VA
            enqueueChar('V');
            enqueueChar('A');
            break;
        default:
            enqueueChar(c);
            break;
    }
}

void setDisplayEnabled(bool enabled)
{
    display_enabled = enabled;
}

void displayFlushIfNeeded(void)
{
    if (display_enabled && display_dirty) {
        display_dirty = false;
        ssd1306_refresh();
    }
}

void displayProcessQueue(void)
{
    int8_t c;
    while (dequeueChar(&c)) {
        printAsc(c);
    }
}
