//==================================================================//
// Record message helper
//==================================================================//
#include "rec_message.h"
#include "ch32v003fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "keyer_hal.h"
#include "keyer.h"
#include <string.h>
#include "print_ascii.h"
#include "oled.h"
#include "message_keyer.h"
#include "flash_eep.h"

#define RECORD_PAGE_COUNT 8
#define RECORD_MSG_SIZE 256
#define RECORD_PAGES_PER_MSG 4
#define RECORD_MAX_LEN 255


FLASH_EEP eep;

static uint8_t msg_buf_storage[RECORD_MSG_SIZE];
static uint8_t *msg_buf = msg_buf_storage;
static uint8_t *record_buf = nullptr;
static uint8_t record_len = 0;
static volatile bool record_mode = false;
static volatile bool record_active = false;
static ui_mode_t ui_mode = UI_MODE_NORMAL;
static rec_header_cb_t header_cb = nullptr;

void rec_draw_header(void);

static const char default_msg_a[] =
    "VVV DE JA1AOQ BT UIAP MESSAGE KEYER IS A COMPACT HAM RADIO KEYER FOR UIAPDUINO, "
    "STORING AUTO MESSAGES IN FLASH AND SENDING CLEAR CW CODE FOR PORTABLE OPS AND DEMOS. "
    "73 AR TU VA E E";

static uint8_t map_prosign_token(const char *token, size_t len)
{
    if (len == 2) {
        if (token[0] == 'A' && token[1] == 'R') return (uint8_t)'a';
        if (token[0] == 'B' && token[1] == 'T') return (uint8_t)'b';
        if (token[0] == 'K' && token[1] == 'N') return (uint8_t)'k';
        if (token[0] == 'V' && token[1] == 'A') return (uint8_t)'v';
    }
    return 0;
}

static void build_message_from_text(uint8_t *buf, const char *msg)
{
    if (buf == nullptr || msg == nullptr) return;
    memset(buf, 0, RECORD_MSG_SIZE);
    uint16_t len = 0;
    const char *p = msg;

    while (*p != '\0' && len < RECORD_MAX_LEN) {
        if (*p == ' ') {
            buf[1 + len] = (uint8_t)' ';
            len++;
            p++;
            continue;
        }
        const char *start = p;
        while (*p != '\0' && *p != ' ') p++;
        size_t tok_len = (size_t)(p - start);
        uint8_t mapped = map_prosign_token(start, tok_len);
        if (mapped != 0) {
            buf[1 + len] = mapped;
            len++;
        } else {
            for (size_t i = 0; i < tok_len && len < RECORD_MAX_LEN; i++) {
                buf[1 + len] = (uint8_t)start[i];
                len++;
            }
        }
    }
    buf[0] = (uint8_t)len;
}

static bool flash_message_uninitialized(uint8_t index)
{
    uint8_t page[FLASH_PAGE_SIZE];
    if (eep.read((index * RECORD_PAGES_PER_MSG), page) != 0) {
        return false;
    }
    for (uint8_t i = 0; i < 4; i++) {
        if (page[i] != 0xFF) return false;
    }
    return true;
}

static void draw_centered(uint8_t y, const char *text, uint8_t color)
{
    size_t len = strlen(text);
    uint16_t width = (uint16_t)(len * 8);
    uint8_t x = 0;
    uint8_t w = g_oled.width();
    if (width < w) {
        x = (uint8_t)((w - width) / 2);
    }
    g_oled.drawStr8(x, y, text, color);
}

static void disp_record_select()
{
    g_oled.fill(0);
    rec_draw_header();
    g_oled.hline(10, 1);
    g_oled.refresh();
}

static void disp_recorded(uint8_t target)
{
    g_oled.fill(0);
    if (target == 0) {
        draw_centered(24, "SWA RECORDED", 1);
    } else {
        draw_centered(24, "SWB RECORDED", 1);
    }
    g_oled.refresh();
}

static void disp_record_start(uint8_t target)
{
    g_oled.fill(0);
    if (target == 0) {
        draw_centered(24, "SWA RECORDING", 1);
    } else {
        draw_centered(24, "SWB RECORDING", 1);
    }
    g_oled.refresh();
}

static void disp_record_canceled(uint8_t target)
{
    g_oled.fill(0);
    if (target == 0) {
        draw_centered(24, "SWA CANCELED", 1);
    } else {
        draw_centered(24, "SWB CANCELED", 1);
    }
    g_oled.refresh();
}

void rec_draw_header(void)
{
    char buf[17];
    memset(buf, ' ', sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    const char prefix[] = "RECORD    ";
    const char suffix[] = "wpm";
    unsigned int pos = 0;
    for (unsigned int i = 0; i < sizeof(prefix) - 1 && pos < sizeof(buf) - 1; i++) {
        buf[pos++] = prefix[i];
    }
    int tens = wpm / 10;
    int ones = wpm % 10;
    if (pos < sizeof(buf) - 1) buf[pos++] = (tens > 0) ? (char)('0' + tens) : ' ';
    if (pos < sizeof(buf) - 1) buf[pos++] = (char)('0' + ones);
    for (unsigned int i = 0; i < sizeof(suffix) - 1 && pos < sizeof(buf) - 1; i++) {
        buf[pos++] = suffix[i];
    }
    for (uint8_t y = 0; y < 8; y++) {
        g_oled.hline(y, 1);
    }
    g_oled.drawStr8(4, 0, buf, 0);
}

static void beep_pattern(uint8_t count)
{
    tim1_int_suspend();
    for (uint8_t i = 0; i < count; i++) {
        bool state = false;
        for (uint16_t j = 0; j < 320; j++) {
            state = !state;
            if (state) {
                GPIO_digitalWrite(PIN_TONE, high);
            } else {
                GPIO_digitalWrite(PIN_TONE, low);
            }
            Delay_Us(250);
        }
        GPIO_digitalWrite(PIN_TONE, low);
        Delay_Ms(80);
    }
    tim1_int_resume();
}

static void rec_load_message_into(uint8_t index, uint8_t *buf)
{
    if (buf == nullptr) return;
    for (uint8_t i = 0; i < RECORD_PAGES_PER_MSG; i++) {
        eep.read((index * RECORD_PAGES_PER_MSG) + i, buf + (i * FLASH_PAGE_SIZE));
    }
    if (buf[0] == 0xFF || buf[0] > RECORD_MAX_LEN) {
        buf[0] = 0;
    }
}

static void save_message(uint8_t index, uint8_t *buf)
{
    if (buf == nullptr) return;
    for (uint8_t i = 0; i < RECORD_PAGES_PER_MSG; i++) {
        eep.write((index * RECORD_PAGES_PER_MSG) + i, buf + (i * FLASH_PAGE_SIZE));
    }
}

static bool record_char_hook(int8_t c)
{
    if (!record_active || record_buf == nullptr) return true;
    if (record_len >= RECORD_MAX_LEN) return false;
    if (c == ' ') {
        if (record_len == 0) return false;
        if (record_buf[record_len] == ' ') return false;
    }
    record_buf[1 + record_len] = (uint8_t)c;
    record_len++;
    record_buf[0] = record_len;
    return true;
}

void rec_set_header_cb(rec_header_cb_t cb)
{
    header_cb = cb;
}

bool rec_is_record_mode(void)
{
    return record_mode;
}

ui_mode_t rec_get_ui_mode(void)
{
    return ui_mode;
}

void rec_enter_mode()
{
    auto_mode = false;
    auto_repeat = false;
    req_start_auto = false;
    req_stop_auto = false;
    req_reset_auto = false;
    auto_finished = false;
    keyup();
    record_mode = true;
    record_active = false;
    setPrintAsciiHook(nullptr);
    printAsciiReset();
    beep_pattern(2);
    ui_mode = UI_MODE_RECORD_SELECT;
    disp_record_select();
}

static void exit_record_mode(bool beep)
{
    record_active = false;
    record_mode = false;
    setPrintAsciiHook(nullptr);
    printAsciiReset();
    if (beep) {
        beep_pattern(2);
    }
    ui_mode = UI_MODE_NORMAL;
    g_oled.fill(0);
    if (header_cb != nullptr) {
        header_cb();
    }
    g_oled.hline(10, 1);
    g_oled.refresh();
}

void rec_exit_mode()
{
    exit_record_mode(true);
}

void rec_record_start(uint8_t target)
{
    (void)target;
    beep_pattern(1);
    disp_record_start(target);
    Delay_Ms(1000);
    record_buf = msg_buf;
    if (record_buf == nullptr) return;
    record_len = 0;
    memset(record_buf, 0, RECORD_MSG_SIZE);
    record_buf[0] = 0;
    record_active = true;
    setPrintAsciiHook(record_char_hook);
    printAsciiReset();
    g_oled.fill(0);
    rec_draw_header();
    g_oled.hline(10, 1);
    g_oled.refresh();
    ui_mode = UI_MODE_RECORDING;
}

void rec_record_finish(uint8_t target)
{
    record_active = false;
    setPrintAsciiHook(nullptr);
    beep_pattern(2);
    uint8_t *save_buf = msg_buf;
    if (save_buf == nullptr) {
        ui_mode = UI_MODE_RECORD_SELECT;
        disp_record_select();
        return;
    }
    save_message(target, save_buf);
    disp_recorded(target);
    Delay_Ms(1000);
    exit_record_mode(false);
}

void rec_record_cancel(uint8_t target)
{
    record_active = false;
    setPrintAsciiHook(nullptr);
    printAsciiReset();
    beep_pattern(3);
    disp_record_canceled(target);
    Delay_Ms(1500);
    exit_record_mode(false);
}

void rec_handle_correction(void)
{
    if (!record_active || record_buf == nullptr) {
        return;
    }
    if (record_len == 0) {
        return;
    }
    while (record_len > 0 && record_buf[record_len] == ' ') {
        record_len--;
        record_buf[0] = record_len;
        record_buf[1 + record_len] = 0;
        printAsciiBackspace();
    }
    if (record_len == 0) {
        return;
    }
    record_len--;
    record_buf[0] = record_len;
    record_buf[1 + record_len] = 0;
    printAsciiBackspace();
}

void rec_load_message(uint8_t index)
{
    rec_load_message_into(index, msg_buf);
}

void rec_init()
{
    eep.begin(RECORD_PAGE_COUNT);
    memset(msg_buf, 0, RECORD_MSG_SIZE);
    if (flash_message_uninitialized(0)) {
        build_message_from_text(msg_buf, default_msg_a);
        save_message(0, msg_buf);
    }
    rec_load_message(0);
    set_message_buffers(msg_buf, msg_buf);
}




