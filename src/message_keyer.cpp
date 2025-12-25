#include "message_keyer.h"
#include "print_ascii.h"
#include "decode.h"

extern int key_spd;
extern volatile bool req_reset_auto;
extern volatile bool auto_repeat;
extern volatile bool auto_finished;

static uint8_t *msg_bufs[2] = {nullptr, nullptr};
static uint8_t active_msg = 0;

const char* morseForChar(char c) {
  return encode_us_char(c);
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
        pos = 0;
        gap_half = 12;
        return 0;
    }

    char c = msg_char(active_msg, pos);

    if (c == ' ') {
        while (pos < msg_len && msg_char(active_msg, pos) == ' ') pos++;
        gap_half = 12;
        return 0;
    }

    if (seq == nullptr) {
        printAscii(c);
        seq = morseForChar(c);
        elem = 0;
        if (seq == nullptr) {
        pos++;
        gap_half = 12;
        return 0;
        }
    }

    char e = seq[elem];
    if (e == '\0') {
        seq = nullptr;
        elem = 0;
        pos++;
        gap_half = 4;
        return 0;
    }

    if (e == '.') auto_squeeze = SQZ_DOT;
    else          auto_squeeze = SQZ_DASH;
    elem++;

    if (seq[elem] == '\0') {
        uint16_t look = pos + 1;
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
            pending_gap_half = 12;
            pending_jump = true;
        } else {
            pending_pos = look;
            pending_gap_half = 4;
            pending_jump = true;
        }
    }

    return out;
}
