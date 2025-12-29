#include "message_keyer.h"
#include "print_ascii.h"
#include "decode.h"
#include "keyer.h"

static uint8_t *msg_bufs[2] = {nullptr, nullptr};
static uint8_t active_msg = 0;
static uint8_t enc_mode = MODE_US;
static uint8_t advance_after = 1;

const char* morseForChar(char c) {
  return encode_us_char(c);
}

static int repeat_gap_half(void)
{
    if (key_spd == 0) return 12;
    uint32_t half_dot_us = (uint32_t)key_spd * 128u;
    if (half_dot_us == 0) return 12;
    uint32_t half_dots = (2000000u + (half_dot_us / 2u)) / half_dot_us;
    if (half_dots == 0) half_dots = 1;
    if (half_dots > 0x7FFFu) half_dots = 0x7FFFu;
    return (int)half_dots;
}

void set_message_buffers(uint8_t *msg_a, uint8_t *msg_b)
{
    msg_bufs[0] = msg_a;
    msg_bufs[1] = msg_b;
}

void set_active_message(uint8_t index)
{
    active_msg = (index > 0) ? 1 : 0;
}

static inline uint8_t msg_length(uint8_t index)
{
    if (msg_bufs[index] == nullptr) return 0;
    return msg_bufs[index][0];
}

static inline char msg_char(uint8_t index, uint16_t pos)
{
    uint8_t len = msg_length(index);
    if (pos >= len) return '\0';
    return (char)msg_bufs[index][1 + pos];
}

static bool is_jp_halfwidth(uint8_t c)
{
    return (c >= 0xA1 && c <= 0xDF);
}

static bool match_mode_marker(uint16_t pos, uint8_t *out_data, uint8_t *out_mode)
{
    uint8_t len = msg_length(active_msg);
    if ((pos + 2) >= len) return false;

    uint8_t c0 = (uint8_t)msg_char(active_msg, pos);
    uint8_t c1 = (uint8_t)msg_char(active_msg, pos + 1);
    uint8_t c2 = (uint8_t)msg_char(active_msg, pos + 2);

    if (c0 != '^') return false;
    if (c1 == 0xCE && c2 == 0xDA) { // ^HO-RE
        *out_data = 5;
        *out_mode = MODE_JP;
        return true;
    }
    if (c1 == 0xD7 && c2 == 0xC0) { // ^RA-TA
        *out_data = 6;
        *out_mode = MODE_US;
        return true;
    }
    return false;
}

static bool match_control_pair(uint16_t pos, uint8_t *out_data, uint8_t *out_next_mode)
{
    uint8_t len = msg_length(active_msg);
    if ((pos + 1) >= len) return false;

    uint8_t c1 = (uint8_t)msg_char(active_msg, pos);
    uint8_t c2 = (uint8_t)msg_char(active_msg, pos + 1);

    if (c1 == 0xCE && c2 == 0xDA) { // HO-RE
        *out_data = 5;
        *out_next_mode = MODE_JP;
        return true;
    }
    if (c1 == 0xD7 && c2 == 0xC0) { // RA-TA
        *out_data = 6;
        *out_next_mode = MODE_US;
        return true;
    }
    return false;
}

uint8_t job_auto()
{
    static uint32_t left_time = 0;
    static int auto_squeeze = 0;
    static int gap_half = 0;
    static uint16_t pos = 0;

    static const char* seq = nullptr;
    static uint8_t elem = 0;

    static bool pending_jump = false;
    static uint16_t pending_pos = 0;
    static int pending_gap_half = 0;
    static int8_t pending_mode = -1;

    if (req_reset_auto) {
        req_reset_auto = false;
        left_time = 0;
        auto_squeeze = 0;
        gap_half = 0;
        pos = 0;
        seq = nullptr;
        elem = 0;
        pending_jump = false;
        pending_gap_half = 0;
        auto_finished = false;
        enc_mode = MODE_US;
        advance_after = 1;
        pending_mode = -1;
        if (!printAsciiAtLineStart()) {
            printAsciiNewline();
        }
    }

    if (left_time != 0) {
        left_time--;
    } else {
        left_time = key_spd / 2;
        if (auto_squeeze != 0) auto_squeeze--;
        else if (gap_half > 0) gap_half--;
    }

    uint8_t out = (auto_squeeze > SQZ_SPC) ? 1 : 0;
    if (auto_squeeze != 0 || gap_half > 0) return out;

    if (pending_jump) {
        pending_jump = false;
        pos   = pending_pos;
        gap_half = pending_gap_half;
        pending_gap_half = 0;
        seq = nullptr;
        elem = 0;
        return 0;
    }

    uint8_t msg_len = msg_length(active_msg);
    if (msg_len == 0) {
        auto_finished = true;
        return 0;
    }
    if (pos >= msg_len) {
        if (!auto_repeat) {
            auto_finished = true;
            return 0;
        }
        printAsciiNewline();
        pos = 0;
        gap_half = repeat_gap_half();
        return 0;
    }

    char c = msg_char(active_msg, pos);

    if (c == ' ') {
        while (pos < msg_len && msg_char(active_msg, pos) == ' ') pos++;
        gap_half = 12;
        return 0;
    }

    if (seq == nullptr) {
        uint8_t marker_data = 0;
        uint8_t marker_mode = MODE_US;
        uint8_t ctrl_data = 0;
        uint8_t ctrl_next_mode = MODE_US;
        advance_after = 1;
        pending_mode = -1;

        if (match_mode_marker(pos, &marker_data, &marker_mode)) {
            enc_mode = marker_mode;
            printAscii('^');
            printAscii((int8_t)((marker_mode == MODE_JP) ? 0xCE : 0xD7));
            printAscii((int8_t)((marker_mode == MODE_JP) ? 0xDA : 0xC0));
            seq = (marker_mode == MODE_JP) ? encode_us_data(marker_data)
                                           : encode_jp_data(marker_data);
            elem = 0;
            advance_after = 3;
        } else if (match_control_pair(pos, &ctrl_data, &ctrl_next_mode)) {
            printAscii((int8_t)msg_char(active_msg, pos));
            printAscii((int8_t)msg_char(active_msg, pos + 1));
            if (ctrl_data == 5) {
                seq = encode_us_data(ctrl_data);
            } else {
                seq = encode_jp_data(ctrl_data);
            }
            elem = 0;
            advance_after = 2;
            pending_mode = (int8_t)ctrl_next_mode;
        } else {
            uint8_t uc = (uint8_t)c;
            bool is_jp = is_jp_halfwidth(uc);
            if (is_jp && enc_mode != MODE_JP) {
                enc_mode = MODE_JP;
                seq = encode_us_data(5); // HO-RE
                elem = 0;
                advance_after = 0;
            } else if (!is_jp && enc_mode != MODE_US) {
                enc_mode = MODE_US;
                seq = encode_jp_data(6); // RA-TA
                elem = 0;
                advance_after = 0;
            } else {
                printAscii(c);
                seq = (enc_mode == MODE_JP) ? encode_jp_char(uc) : morseForChar(c);
                elem = 0;
            }
        }

        if (seq == nullptr) {
            pos += advance_after;
            gap_half = 12;
            return 0;
        }
    }

    char e = seq[elem];
    if (e == '\0') {
        seq = nullptr;
        elem = 0;
        pos = (uint16_t)(pos + advance_after);
        gap_half = 4;
        if (pending_mode >= 0) {
            enc_mode = (uint8_t)pending_mode;
            pending_mode = -1;
        }
        return 0;
    }

    if (e == '.') auto_squeeze = SQZ_DOT;
    else          auto_squeeze = SQZ_DASH;
    elem++;

    if (seq[elem] == '\0') {
        if (pending_mode >= 0) {
            enc_mode = (uint8_t)pending_mode;
            pending_mode = -1;
        }
        if (advance_after == 0) {
            pending_pos = pos;
            pending_gap_half = 4;
            pending_jump = true;
        } else {
            uint16_t look = (uint16_t)(pos + advance_after);
            if (look < msg_len && msg_char(active_msg, look) == ' ') {
                uint8_t nsp = 0;
                while (look < msg_len && msg_char(active_msg, look) == ' ') { nsp++; look++; }

                printAscii(32);
                if (look >= msg_len) {
                    if (!auto_repeat) {
                        pending_pos = msg_len;
                        pending_gap_half = (int)(14 * nsp - 2);
                        pending_jump = true;
                        return 0;
                    }
                    pending_pos = 0;
                } else {
                    pending_pos = look;
                }

                pending_gap_half = (int)(14 * nsp - 2);
                pending_jump = true;
            } else if (look >= msg_len) {
                if (!auto_repeat) {
                    pending_pos = msg_len;
                    pending_gap_half = 4;
                    pending_jump = true;
                    return 0;
                }
                pending_pos = 0;
                printAsciiNewline();
                pending_gap_half = repeat_gap_half();
                pending_jump = true;
            } else {
                pending_pos = look;
                pending_gap_half = 4;
                pending_jump = true;
            }
        }
    }

    return out;
}
