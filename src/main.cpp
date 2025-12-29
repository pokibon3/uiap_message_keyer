//==================================================================//
// UIAP_keyer_for_ch32fun
// BASE Software from : https://www.gejigeji.com/?page_id=1045
// Modified by Kimio Ohe JA9OIR/JA1AOQ
//	- port to ch32fun library
//==================================================================
#include "ch32v003fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "keyer_hal.h"
#include <stdint.h>
#include <string.h>
#define SSD1306_128X64
#define printf(...) ((void)0)
#include "ssd1306_i2c.h"
#undef printf
#include "ssd1306.h"
#include "print_ascii.h"
#include "message_keyer.h"
#include "cw_decoder.h"
#include "rec_message.h"

#define WAIT_FOR_WORDSPACE 0        // wait for WORD SPACE after 7 dots

// ==== 変数 ====
int  key_spd = 1000;
int  wpm = 20;
bool tone_enabled = false;
static volatile uint8_t tone_div = 0;
static volatile bool tone_state = false;
volatile bool display_request = true;
int squeeze = 0;
int paddle = PDL_FREE;
int paddle_old = PDL_FREE;
static bool header_dirty = false;
static char title1[]   = "  UIAP Message ";
static char title2[]   = "     KEYER     ";
static char title3[]   = "  Version 1.2  ";
void keyup();
static volatile uint8_t req_start_msg = 0;
// ==== 自動送信制御 ====
// 起動時は何もしない
volatile bool auto_mode = false;   // 今、自動送信中
volatile bool auto_repeat = false; // リピート再生中
volatile bool req_start_auto = false;
volatile bool req_stop_auto = false;
volatile bool req_reset_auto = false;
volatile bool auto_finished = false;

// ===================== 画面表示 =====================
static void disp_header()
{
    char buf[17];

    const char prefix[] = "UIAPKEYER ";
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
    buf[pos] = '\0';
    ssd1306_drawstr_sz(4, 0, buf, 1, fontsize_8x8);
}

static void disp_title()
{
    //    disp title;
    ssd1306_fillRect(0, 0, SSD1306_W, SSD1306_H, 0);
    ssd1306_drawstr_sz(0, 8, title1, 1, fontsize_8x8);
    ssd1306_drawstr_sz(0, 24, title2, 1, fontsize_8x8);
    ssd1306_drawstr_sz(0, 40, title3, 1, fontsize_8x8);
    ssd1306_refresh();

	Delay_Ms(1000);
    ssd1306_fillRect(0, 0, SSD1306_W, SSD1306_H, 0);
    disp_header();
    ssd1306_drawFastHLine(0, 10, 128, 1);
    ssd1306_refresh();
}

uint8_t oled_width(void)
{
    return SSD1306_W;
}

uint8_t oled_height(void)
{
    return SSD1306_H;
}

void oled_fill(uint8_t color)
{
    ssd1306_fillRect(0, 0, SSD1306_W, SSD1306_H, color);
}

void oled_drawstr8(uint8_t x, uint8_t y, const char *text, uint8_t color)
{
    ssd1306_drawstr_sz(x, y, (char *)text, color, fontsize_8x8);
}

void oled_drawchar16(uint8_t x, uint8_t y, uint8_t chr, uint8_t color)
{
    ssd1306_drawchar_sz(x, y, chr, color, fontsize_16x16);
}

void oled_hline(uint8_t y, uint8_t color)
{
    ssd1306_drawFastHLine(0, y, SSD1306_W, color);
}

void oled_refresh(void)
{
    ssd1306_refresh();
}

// ===================== 手動パドル処理（SWA/SWBは混ぜない）=====================
uint8_t job_paddle()
{
    static uint32_t left_time = 0;
#if WAIT_FOR_WORDSPACE
    static int gap_half = 0;
    static int extra_wait = 0;
    static uint8_t pending_paddle = PDL_FREE;
#endif
    uint8_t key_dot, key_dash;

    key_dot  = (!GPIO_digitalRead(PIN_DOT));
    key_dash = (!GPIO_digitalRead(PIN_DASH));

    if (left_time != 0) {
        left_time--;
    } else {
        left_time = key_spd / 2;
        if (squeeze != SQZ_FREE) squeeze--;
#if WAIT_FOR_WORDSPACE
        if (extra_wait > 0) {
            extra_wait--;
        } else if (squeeze == SQZ_FREE) {
            gap_half++;
        } else {
            gap_half = 0;
        }
#endif
    }

#if WAIT_FOR_WORDSPACE
    if (extra_wait > 0) return 0;

    if (pending_paddle != PDL_FREE) {
        paddle = pending_paddle;
        pending_paddle = PDL_FREE;
    }
#endif

    if (squeeze != SQZ_FREE) {
        if (paddle_old == PDL_DOT  && key_dash) paddle = PDL_DASH;
        else if (paddle_old == PDL_DASH && key_dot) paddle = PDL_DOT;
    }

    if (SQUEEZE_TYPE == 0) {
        if (squeeze > SQZ_DASH) paddle = PDL_FREE;
    } else {
        if (squeeze > SQZ_SPC)  paddle = PDL_FREE;
    }

    if (squeeze > SQZ_SPC) return 1;
    else if (squeeze == SQZ_SPC || squeeze == SQZ_SPC0) return 0;

    if (paddle == PDL_FREE) {
        if (key_dot) paddle = PDL_DOT;
        else if (key_dash) paddle = PDL_DASH;
    }

    if (paddle == PDL_FREE) return 0;

#if WAIT_FOR_WORDSPACE
    if (gap_half > 6) {
        int remaining = 14 - gap_half;
        if (remaining < 0) remaining = 0;
        pending_paddle = paddle;
        paddle = PDL_FREE;
        extra_wait = remaining;
        gap_half = 0;
        return 0;
    }
#endif

    if (paddle == PDL_DOT) squeeze = SQZ_DOT;
    else {
        uint8_t dash_len = (SQZ_SPC * PDL_RATIO + 5) / 2;
        squeeze = SQZ_SPC + dash_len;
  }

#if WAIT_FOR_WORDSPACE
    gap_half = 0;
#endif
    left_time = key_spd / 2;
    paddle_old = paddle;
    paddle = PDL_FREE;
    return 1;
}

// ===================== トーン制御 =====================
void keydown()
{
    if (tone_enabled) return;
    tone_enabled = true;
    tone_div = 0;
    tone_state = false;
    GPIO_digitalWrite(PIN_TONE, low);
    display_request = false;
    if (!rec_is_record_mode()) {
	    GPIO_digitalWrite(PIN_KEYOUT, high);  // ON
    }
    setstate(high);
}

void keyup()
{
    tone_enabled = false;
    tone_div = 0;
    tone_state = false;
    GPIO_digitalWrite(PIN_TONE, low);
    display_request = true;
    if (!rec_is_record_mode()) {
	    GPIO_digitalWrite(PIN_KEYOUT, low);   // OFF
    }
    setstate(low);
}

static inline long map(long x,
                              long in_min, long in_max,
                              long out_min, long out_max)
{
    // Arduino本家と同じくゼロ除算チェックはしない。in_max == in_min だと未定義。
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
// ==== ADCからスピード読み込み ====
void update_speed_from_adc()
{
    static int old_wpm;
    static int new_wpm;

    int adc = GPIO_analogRead(GPIO_Ain0_A2);        // 0 .. 1023
    new_wpm = map(adc, 0, 1023, WPM_MIN, WPM_MAX);
    if (new_wpm != old_wpm) {
        old_wpm = new_wpm;
        wpm = new_wpm;
        key_spd = 4687 / wpm;  // = (1200/wpm) /0.256  4687.5 -> 4687
        if (rec_is_record_mode()) {
            rec_draw_header();
        } else {
            disp_header();
        }
        if (display_request) {
            ssd1306_refresh();
        } else {
            header_dirty = true;
        }
    }
    //printf("WPM=%d, SPD=%d\r\n", wpm, key_spd);
    Delay_Ms(10);
}


// ===================== タイマー割り込み =====================
void TIM1_UP_IRQHandler(void)
{

    bool dotPressed  = !GPIO_digitalRead(PIN_DOT);
    bool dashPressed = !GPIO_digitalRead(PIN_DASH);

    // フラグのクリア
	  TIM1->INTFR &= (uint16_t)~TIM_IT_Update;

    if (!rec_is_record_mode()) {
        if (req_start_auto) {
            req_start_auto = false;
            auto_mode = true;
            auto_finished = false;
            req_reset_auto = true;
            set_active_message(req_start_msg);
        }

        if (req_stop_auto) {
            req_stop_auto = false;
            auto_mode = false;
            auto_finished = false;
            keyup();
        }

        if (auto_mode && (dotPressed || dashPressed)) {
            auto_mode = false;
            auto_finished = false;
            keyup();
        }
    } else {
        if (auto_mode) {
            auto_mode = false;
            auto_finished = false;
            keyup();
        }
    }

    // 出力（停止中でも手動パドルは有効）
    uint8_t on = (auto_mode && !rec_is_record_mode()) ? job_auto() : job_paddle();
    if (!rec_is_record_mode() && auto_mode && auto_finished && on == 0) {
        auto_mode = false;
        auto_finished = false;
        keyup();
        return;
    }
    if (on) {
        keydown();
    } else {
        keyup();
    }
    if (tone_enabled) {
        tone_div++;
        if (tone_div >= 2) {
            tone_div = 0;
            tone_state = !tone_state;
            if (tone_state) {
                GPIO_digitalWrite(PIN_TONE, high);
            } else {
                GPIO_digitalWrite(PIN_TONE, low);
            }
        }
    } else if (tone_state) {
        tone_state = false;
        GPIO_digitalWrite(PIN_TONE, low);
    }
}

//
//  main loop
//
int main()
{
    SystemInit();
  	ssd1306_i2c_init();
	ssd1306_init();
	GPIO_setup();				// gpio Setup;
	GPIO_ADCinit();
	tim1_int_init();			//
    rec_set_header_cb(disp_header);
    rec_init();
    disp_title();
    while (1)
    {
        // SWA/SWB デバウンスとモード制御は離した時に確定！
        static uint32_t tA = 0;
        static uint32_t tB = 0;
        static int lastA = high;
        static int lastB = high;
        static bool swa_pressed = false;
        static bool swb_pressed = false;
        static bool swa_long_fired = false;
        static bool swb_long_fired = false;
        static bool swa_ignore_release = false;
        static bool swb_ignore_release = false;
        static bool combo_release_suppress = false;
        static uint32_t swa_press_time = 0;
        static uint32_t swb_press_time = 0;
        static uint32_t combo_start = 0;
        static bool combo_armed = false;
        static bool combo_fired = false;
        static int rec_target = -1;
        const uint32_t DEB = 30;
        const uint32_t LONG_PRESS_MS = 1500;
        const uint32_t RECORD_MIN_PRESS_MS = 80;
        static bool display_enabled = true;

		static uint32_t last_tick = 0;
        static uint32_t last_display_flush = 0;
		uint32_t now = millis();

		if ((now - last_tick) >= 10) {
		    last_tick = now;
		    update_speed_from_adc();
		    int a = GPIO_digitalRead(PIN_SWA);
            int b = GPIO_digitalRead(PIN_SWB);

		    if (a != lastA && (now - tA) > DEB) {
		        tA = now;
		        lastA = a;
		        if (a == low) {
                    swa_pressed = true;
                    swa_press_time = now;
                    swa_long_fired = false;
                    swa_ignore_release = false;
                } else {
                    if (swa_ignore_release) {
                        swa_ignore_release = false;
                        swa_pressed = false;
                        if (combo_armed) {
                            combo_armed = false;
                            combo_fired = false;
                            combo_start = 0;
                        }
                    } else {
                        ui_mode_t mode = rec_get_ui_mode();
                        uint32_t held = now - swa_press_time;
                        if (mode != UI_MODE_NORMAL && held < RECORD_MIN_PRESS_MS) {
                            swa_pressed = false;
                            if (combo_armed) {
                                combo_armed = false;
                                combo_fired = false;
                                combo_start = 0;
                            }
                            goto swa_release_done;
                        }
                        if (mode == UI_MODE_RECORD_SELECT) {
                            rec_record_start(0);
                            rec_target = 0;
                        } else if (mode == UI_MODE_RECORDING) {
                            if (rec_target == 0) {
                                rec_record_finish(0);
                                rec_target = -1;
                            } else if (rec_target == 1) {
                                rec_record_cancel(1);
                                rec_target = -1;
                            }
                        } else if (mode == UI_MODE_NORMAL) {
                            if (auto_mode) {
                                req_stop_auto = true;
                            } else {
                                auto_repeat = (held >= LONG_PRESS_MS);
                                req_start_msg = 0;
                                rec_load_message(0);
                                req_start_auto = true;
                            }
                        }
                    }
                swa_release_done:
                    if (combo_armed) {
                        combo_armed = false;
                        combo_fired = false;
                        combo_start = 0;
                    }
                    swa_pressed = false;
                }
		    }

            if (b != lastB && (now - tB) > DEB) {
		        tB = now;
		        lastB = b;
		        if (b == low) {
                    swb_pressed = true;
                    swb_press_time = now;
                    swb_long_fired = false;
                    swb_ignore_release = false;
                } else {
                    if (swb_ignore_release) {
                        swb_ignore_release = false;
                        swb_pressed = false;
                        if (combo_armed) {
                            combo_armed = false;
                            combo_fired = false;
                            combo_start = 0;
                        }
                    } else {
                        ui_mode_t mode = rec_get_ui_mode();
                        uint32_t held = now - swb_press_time;
                        if (mode != UI_MODE_NORMAL && held < RECORD_MIN_PRESS_MS) {
                            swb_pressed = false;
                            if (combo_armed) {
                                combo_armed = false;
                                combo_fired = false;
                                combo_start = 0;
                            }
                            goto swb_release_done;
                        }
                        if (mode == UI_MODE_RECORD_SELECT) {
                            rec_record_start(1);
                            rec_target = 1;
                        } else if (mode == UI_MODE_RECORDING) {
                            if (rec_target == 1) {
                                rec_record_finish(1);
                                rec_target = -1;
                            } else if (rec_target == 0) {
                                rec_record_cancel(0);
                                rec_target = -1;
                            }
                        } else if (mode == UI_MODE_NORMAL) {
                            if (auto_mode) {
                                req_stop_auto = true;
                            } else {
                                auto_repeat = (held >= LONG_PRESS_MS);
                                req_start_msg = 1;
                                rec_load_message(1);
                                req_start_auto = true;
                            }
                        }
                    }
                swb_release_done:
                    if (combo_armed) {
                        combo_armed = false;
                        combo_fired = false;
                        combo_start = 0;
                    }
                    swb_pressed = false;
                }
		    }

            if (swa_pressed && swb_pressed) {
                if (!combo_armed) {
                    combo_armed = true;
                    combo_start = now;
                    combo_fired = false;
                }
                if (combo_armed && !combo_fired && (now - combo_start) >= LONG_PRESS_MS) {
                    combo_fired = true;
                    combo_armed = false;
                    combo_start = 0;
                    swa_ignore_release = true;
                    swb_ignore_release = true;
                    swa_long_fired = true;
                    swb_long_fired = true;
                    combo_release_suppress = true;
                    if (rec_get_ui_mode() == UI_MODE_NORMAL) {
                        rec_enter_mode();
                    } else {
                        rec_exit_mode();
                        rec_target = -1;
                    }
                }
            } else if (rec_get_ui_mode() == UI_MODE_NORMAL && !auto_mode && !combo_release_suppress) {
                if (swa_pressed && !swb_pressed && !swa_long_fired && (now - swa_press_time) >= LONG_PRESS_MS) {
                    swa_long_fired = true;
                    swa_ignore_release = true;
                    auto_repeat = true;
                    req_start_msg = 0;
                    rec_load_message(0);
                    req_start_auto = true;
                } else if (swb_pressed && !swa_pressed && !swb_long_fired && (now - swb_press_time) >= LONG_PRESS_MS) {
                    swb_long_fired = true;
                    swb_ignore_release = true;
                    auto_repeat = true;
                    req_start_msg = 1;
                    rec_load_message(1);
                    req_start_auto = true;
                }
            }

            if (!swa_pressed && !swb_pressed) {
                combo_armed = false;
                combo_fired = false;
                combo_start = 0;
                combo_release_suppress = false;
            }
		}
        if (display_enabled != display_request) {
            display_enabled = display_request;
            setDisplayEnabled(display_enabled);
            if (display_enabled && header_dirty) {
                header_dirty = false;
                ssd1306_refresh();
            }
        }
        displayProcessQueue();
        if (display_request && (now - last_display_flush) >= 50) {
            last_display_flush = now;
            displayFlushIfNeeded();
        }
        if (!auto_mode || rec_is_record_mode()) {
            cwDecoder();
        }
  		Delay_Ms(1);
    }
}




