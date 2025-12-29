#include "keyer.h"
#include "ch32v003fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "keyer_hal.h"
#include "message_keyer.h"
#include "cw_decoder.h"
#include "rec_message.h"

#define WAIT_FOR_WORDSPACE 0        // wait for WORD SPACE after 7 dots

// ==== 変数 ====
int  key_spd = 1000;
int  wpm = 20;
bool tone_enabled = false;
volatile bool display_request = true;
int squeeze = 0;
int paddle = PDL_FREE;
int paddle_old = PDL_FREE;
volatile uint8_t req_start_msg = 0;
// ==== 自動送信制御 ====
// 起動時は何もしない
volatile bool auto_mode = false;   // 今、自動送信中
volatile bool auto_repeat = false; // リピート再生中
volatile bool req_start_auto = false;
volatile bool req_stop_auto = false;
volatile bool req_reset_auto = false;
volatile bool auto_finished = false;

static volatile uint8_t tone_div = 0;
static volatile bool tone_state = false;

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
    static uint8_t last_on = 0;
    if (on != last_on) {
        if (on) {
            keydown();
        } else {
            keyup();
        }
        last_on = on;
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
