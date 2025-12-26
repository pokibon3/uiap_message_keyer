//==================================================================//
// Record message helper
//==================================================================//
#include "rec_message.h"
#include "ch32v003fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "keyer_hal.h"
#include <string.h>
#include "print_ascii.h"
#include "message_keyer.h"
#include "flash_eep.h"

#define RECORD_PAGE_COUNT 8
#define RECORD_MSG_SIZE 256
#define RECORD_PAGES_PER_MSG 4
#define RECORD_MAX_LEN 255

extern volatile bool auto_mode;
extern volatile bool auto_repeat;
extern volatile bool req_start_auto;
extern volatile bool req_stop_auto;
extern volatile bool req_reset_auto;
extern volatile bool auto_finished;
extern void keyup();

FLASH_EEP eep;

static uint8_t msg_buf_storage[RECORD_MSG_SIZE];
static uint8_t *msg_buf = msg_buf_storage;
static uint8_t *record_buf = nullptr;
static uint8_t record_len = 0;
static volatile bool record_mode = false;
static volatile bool record_active = false;
static ui_mode_t ui_mode = UI_MODE_NORMAL;
static rec_header_cb_t header_cb = nullptr;

static void draw_centered(uint8_t y, const char *text, uint8_t color)
{
    size_t len = strlen(text);
    uint16_t width = (uint16_t)(len * 8);
    uint8_t x = 0;
    uint8_t w = oled_width();
    if (width < w) {
        x = (uint8_t)((w - width) / 2);
    }
    oled_drawstr8(x, y, text, color);
}

static void disp_record_select()
{
//    oled_fill(1);
//    draw_centered(16, "RECORD MODE", 0);
//    draw_centered(32, "SELECT A or B", 0);
    oled_fill(0);
    draw_centered(0, "**RECORD MODE**", 0);
    oled_hline(10, 1);
    oled_refresh();
}

static void disp_recorded(uint8_t target)
{
    oled_fill(0);
    if (target == 0) {
        draw_centered(24, "SWA RECORDED", 1);
    } else {
        draw_centered(24, "SWB RECORDED", 1);
    }
    oled_refresh();
}

static void disp_record_canceled(uint8_t target)
{
    oled_fill(0);
    if (target == 0) {
        draw_centered(24, "SWA CANCELED", 1);
    } else {
        draw_centered(24, "SWB CANCELED", 1);
    }
    oled_refresh();
}

static void beep_pattern(uint8_t count)
{
    tim1_int_suspend();
    pwm_set_freq(3000);
    for (uint8_t i = 0; i < count; i++) {
        start_pwm();
        Delay_Ms(80);
        stop_pwm();
        Delay_Ms(80);
    }
    pwm_restore_default();
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

static void record_char_hook(int8_t c)
{
    if (!record_active || record_buf == nullptr) return;
    if (record_len >= RECORD_MAX_LEN) return;
    record_buf[1 + record_len] = (uint8_t)c;
    record_len++;
    record_buf[0] = record_len;
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

void rec_exit_mode()
{
    record_active = false;
    record_mode = false;
    setPrintAsciiHook(nullptr);
    printAsciiReset();
    beep_pattern(4);
    ui_mode = UI_MODE_NORMAL;
    oled_fill(0);
    if (header_cb != nullptr) {
        header_cb();
    }
    oled_hline(10, 1);
    oled_refresh();
}

void rec_record_start(uint8_t target)
{
    (void)target;
    record_buf = msg_buf;
    if (record_buf == nullptr) return;
    record_len = 0;
    memset(record_buf, 0, RECORD_MSG_SIZE);
    record_buf[0] = 0;
    record_active = true;
    setPrintAsciiHook(record_char_hook);
    printAsciiReset();
    oled_fill(0);
    oled_refresh();
    ui_mode = UI_MODE_RECORDING;
}

void rec_record_finish(uint8_t target)
{
    record_active = false;
    setPrintAsciiHook(nullptr);
    uint8_t *save_buf = msg_buf;
    if (save_buf == nullptr) {
        ui_mode = UI_MODE_RECORD_SELECT;
        disp_record_select();
        return;
    }
    save_message(target, save_buf);
    disp_recorded(target);
    Delay_Ms(1000);
    disp_record_select();
    ui_mode = UI_MODE_RECORD_SELECT;
}

void rec_record_cancel(uint8_t target)
{
    record_active = false;
    setPrintAsciiHook(nullptr);
    printAsciiReset();
    disp_record_canceled(target);
    Delay_Ms(1500);
    disp_record_select();
    ui_mode = UI_MODE_RECORD_SELECT;
}

void rec_load_message(uint8_t index)
{
    rec_load_message_into(index, msg_buf);
}

void rec_init()
{
    eep.begin(RECORD_PAGE_COUNT);
    memset(msg_buf, 0, RECORD_MSG_SIZE);
    rec_load_message(0);
    set_message_buffers(msg_buf, msg_buf);
}




