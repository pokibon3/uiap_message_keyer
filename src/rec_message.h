#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    UI_MODE_NORMAL = 0,
    UI_MODE_RECORD_SELECT = 1,
    UI_MODE_RECORDING = 2
} ui_mode_t;

typedef void (*rec_header_cb_t)(void);

void rec_set_header_cb(rec_header_cb_t cb);
void rec_draw_header(void);
void rec_init(void);

bool rec_is_record_mode(void);
ui_mode_t rec_get_ui_mode(void);

void rec_enter_mode(void);
void rec_exit_mode(void);
void rec_record_start(uint8_t target);
void rec_record_finish(uint8_t target);
void rec_record_cancel(uint8_t target);
void rec_handle_correction(void);

void rec_load_message(uint8_t index);
