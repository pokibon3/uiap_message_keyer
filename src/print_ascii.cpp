#include "print_ascii.h"
#include <string.h>
#define FONT_WIDTH 12
#define FONT_COLOR 1
#define LINE_HEIGHT 16
const int colums = 10; /// have to be 16 or 20

static int lcdindex = 0;
static uint8_t line0[colums];
static uint8_t line1[colums];
static uint8_t line2[colums];
static bool display_enabled = true;
static bool display_dirty = false;
static bool (*ascii_hook)(int8_t c) = nullptr;

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
    if (ascii_hook != nullptr && !ascii_hook(asciinumber)) {
        return;
    }
    if (lcdindex > colums - 1){
        lcdindex = 0;
        for (int i = 0; i <= colums - 1 ; i++){
            line2[i] = line1[i];
            line1[i] = line0[i];
            line0[i] = 32;
        }
        for (int i = 0; i <= colums - 1 ; i++){
            oled_drawchar16(i * FONT_WIDTH , LINE_HEIGHT, line2[i], FONT_COLOR);
            oled_drawchar16(i * FONT_WIDTH , LINE_HEIGHT * 2, line1[i], FONT_COLOR);
            oled_drawchar16(i * FONT_WIDTH , LINE_HEIGHT * 3, line0[i], FONT_COLOR);
        }
    }
    line0[lcdindex] = asciinumber;
    oled_drawchar16(lcdindex * FONT_WIDTH , LINE_HEIGHT * 3, asciinumber, FONT_COLOR);
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
        oled_refresh();
    }
}

void displayProcessQueue(void)
{
    int8_t c;
    while (dequeueChar(&c)) {
        printAsc(c);
    }
}

void setPrintAsciiHook(bool (*hook)(int8_t c))
{
    ascii_hook = hook;
}

void printAsciiReset(void)
{
    lcdindex = 0;
    for (int i = 0; i < colums; i++) {
        line0[i] = 32;
        line1[i] = 32;
        line2[i] = 32;
    }
    display_dirty = true;
}

void printAsciiBackspace(void)
{
    if (lcdindex > 0) {
        lcdindex -= 1;
        line0[lcdindex] = 32;
        oled_drawchar16(lcdindex * FONT_WIDTH, LINE_HEIGHT * 3, 32, FONT_COLOR);
        display_dirty = true;
        return;
    }

    int last = colums - 1;
    while (last >= 0 && line1[last] == 32) {
        last--;
    }
    if (last >= 0) {
        memcpy(line0, line1, sizeof(line0));
        memcpy(line1, line2, sizeof(line1));
        for (int i = 0; i < colums; i++) {
            line2[i] = 32;
        }
    } else {
        int last2 = colums - 1;
        while (last2 >= 0 && line2[last2] == 32) {
            last2--;
        }
        if (last2 < 0) {
            return;
        }
        memcpy(line0, line2, sizeof(line0));
        for (int i = 0; i < colums; i++) {
            line1[i] = 32;
            line2[i] = 32;
        }
        last = last2;
    }

    lcdindex = last;
    line0[lcdindex] = 32;
    for (int i = 0; i < colums; i++) {
        oled_drawchar16(i * FONT_WIDTH, LINE_HEIGHT, line2[i], FONT_COLOR);
        oled_drawchar16(i * FONT_WIDTH, LINE_HEIGHT * 2, line1[i], FONT_COLOR);
        oled_drawchar16(i * FONT_WIDTH, LINE_HEIGHT * 3, line0[i], FONT_COLOR);
    }
    display_dirty = true;
}

void printAsciiNewline(void)
{
    lcdindex = 0;
    for (int i = 0; i <= colums - 1 ; i++){
        line2[i] = line1[i];
        line1[i] = line0[i];
        line0[i] = 32;
    }
    for (int i = 0; i <= colums - 1 ; i++){
        oled_drawchar16(i * FONT_WIDTH , LINE_HEIGHT, line2[i], FONT_COLOR);
        oled_drawchar16(i * FONT_WIDTH , LINE_HEIGHT * 2, line1[i], FONT_COLOR);
        oled_drawchar16(i * FONT_WIDTH , LINE_HEIGHT * 3, line0[i], FONT_COLOR);
    }
    display_dirty = true;
}

bool printAsciiAtLineStart(void)
{
    return (lcdindex == 0);
}







