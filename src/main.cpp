//==================================================================//
// UIAP_keyer_for_ch32fun
// BASE Software from : https://www.gejigeji.com/?page_id=1045
// Modified by Kimio Ohe JA9OIR/JA1AOQ
//	- port to ch32fun library
//==================================================================
#include "ch32v003fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "keyer_hal.h"
#include <stdio.h>
#include <stdint.h>
#define SSD1306_128X64
#include "ssd1306_i2c.h"
#include "ssd1306.h"
#include "print_ascii.h"
#include "message_keyer.h"
#include "cw_decoder.h"
#include "flash_eep.h"

#define WAIT_FOR_WORDSPACE 0        // wait for WORD SPACE after 7 dots

// グローバルオブジェクト。初期化は main で begin() を呼ぶ。
FLASH_EEP eep;

// ==== 変数 ====
int  key_spd = 1000;
int  wpm = 20;
bool tone_enabled = false;
volatile bool display_request = true;
int squeeze = 0;
int paddle = PDL_FREE;
int paddle_old = PDL_FREE;
static bool header_dirty = false;
static char title1[]   = "  UIAP Message ";
static char title2[]   = "     KEYER     ";
static char title3[]   = "  Version 1.0  ";
// ==== 自動送信制御 ====
// 起動時は何もしない
volatile bool auto_mode = false;   // 今、自動送信中か
volatile bool auto_repeat = false; // リピート再生中か
volatile bool req_start_auto = false;
volatile bool req_stop_auto = false;
volatile bool req_reset_auto = false;
volatile bool auto_finished = false;

// ===================== 画面表示 =====================
static void disp_header()
{
    char buf[17];

    mini_snprintf(buf, sizeof(buf), "UIAPKEYER %2dwpm", wpm);
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


static void eep_init()
{
    uint8_t buf[64];
    int r = eep.read(0, buf);
    buf[0] = 0x00; 
    r = eep.write(0, buf);
    // dummy write to avoid "unused variable" warning
}

// ===================== 手動パドル処理（SWA/SWBは混ぜない） =====================
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
	start_pwm();
    display_request = false;
	GPIO_digitalWrite(PIN_KEYOUT, high);  // ON
    setstate(high);
}

void keyup()
{
	stop_pwm();
    tone_enabled = false;
    display_request = true;
	GPIO_digitalWrite(PIN_KEYOUT, low);   // OFF
    setstate(low);
}

static inline long map(long x,
                              long in_min, long in_max,
                              long out_min, long out_max)
{
    // Arduino本家と同じくゼロ除算チェックはしない（in_max == in_min だと未定義）
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
        disp_header();
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
    bool swbPressed  = !GPIO_digitalRead(PIN_SWB);

    // フラグのクリア
	  TIM1->INTFR &= (uint16_t)~TIM_IT_Update;

    // --- SWB 押下エッジ検出（停止） ---
    static bool prevSWB = false;
    if (!prevSWB && swbPressed) {
        auto_mode = false;
        auto_finished = false;
        keyup(); // 自動送信途中でも即ミュート
    }
    prevSWB = swbPressed;

    // --- SWA 押下→開始 ---
    if (req_start_auto) {
        req_start_auto = false;
        auto_mode = true;
        auto_finished = false;
        req_reset_auto = true; // msgs[0]へ
    }

    // --- SWA 押下中の停止要求 ---
    if (req_stop_auto) {
        req_stop_auto = false;
        auto_mode = false;
        auto_finished = false;
        keyup();
    }

    // --- DOT/DASH：自動送信を停止 ---
    if (auto_mode && (dotPressed || dashPressed)) {
        auto_mode = false;
        auto_finished = false;
        keyup(); // 自動送信途中でも即ミュート
    }

    // 出力（停止中でも手動パドルは有効）
    uint8_t on = auto_mode ? job_auto() : job_paddle();
    if (auto_mode && auto_finished && on == 0) {
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
	tim2_pwm_init();            // TIM2 PWM Setup
    eep_init();
    disp_title();
    while (1)
    {
        // SWAだけデバウンスして「押した瞬間」を拾う
        static uint32_t tA = 0;
        static int lastA = high;
        static bool swa_active = false;
        static uint32_t swa_press_time = 0;
        const uint32_t DEB = 30;
        static bool display_enabled = true;

  		static uint32_t last_tick = 0;
        static uint32_t last_display_flush = 0;
  		uint32_t now = millis();

  		if ((now - last_tick) >= 10) {
  		    last_tick = now;
  		    update_speed_from_adc(); // ???E????
  		    int a = GPIO_digitalRead(PIN_SWA);

		    if (a != lastA && (now - tA) > DEB) {
		        tA = now;
		        lastA = a;
		        if (a == low) {
                    if (auto_mode) {
                        req_stop_auto = true;
                        swa_active = false;
                    } else {
                        swa_active = true;
                        swa_press_time = now;
                    }
		        } else {
                    if (swa_active && !auto_mode) {
                        uint32_t held = now - swa_press_time;
                        auto_repeat = (held >= 2000);
                        req_start_auto = true;
                    }
                    swa_active = false;
                }
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
        if (!auto_mode) {
            cwDecoder();
        }
  		Delay_Ms(1);
    }
}
