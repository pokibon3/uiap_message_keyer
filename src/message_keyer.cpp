#include "message_keyer.h"
#include "print_ascii.h"
#include "decode.h"

extern int key_spd;
extern volatile bool req_reset_auto;
extern volatile bool auto_repeat;
extern volatile bool auto_finished;

static const char *msgs[] = {
  " CQ CQ CQ DE JA1AOQ PSE K   ",
  "JA1AOQ DE JA9OIR K   ",
  "JA9OIR DE JA1AOQ GM TNX FER UR CALL b UR RST 599 ES QTH SAGAMIHARA CITY ES NAME KIMIWO HW? JA9OIR DE JA1AOQ k   ",
  "R JA1AOQ DE JA9OIR GM DR KIMIWO SAN TKS FB REPT b UR RST 599 ES QTH UOZU CITY ES NAME POKI HW? JA1AOQ DE JA9OIR k   ",
  "R DE JA1AOQ TNX FB 1ST QSO, HPE CU AGN DR POKI SAN 73 JA9OIR DE JA1AOQ TU v E E   ",
  "DE JA9OIR TNX FB QSO ES HVE A NICE DAY KIMIWO SAN 73 a JA1AOQ DE JA9OIR TU v E E   ",
};
#define MSG_COUNT (sizeof(msgs) / sizeof(msgs[0]))

const char* morseForChar(char c) {
  return encode_us_char(c);
}

uint8_t job_auto()
{
    static uint32_t left_time = 0;
    static int auto_squeeze = 0;
    static int gap_half = 0;
    static uint8_t msg_i = 0;
    static uint16_t pos = 0;

    static const char* seq = nullptr;
    static uint8_t elem = 0;

    static bool pending_jump = false;
    static uint8_t pending_msg = 0;
    static uint16_t pending_pos = 0;
    static int pending_gap_half = 0;

    if (req_reset_auto) {
        req_reset_auto = false;
        left_time = 0;
        auto_squeeze = 0;
        gap_half = 0;
        msg_i = 0;
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
        msg_i = pending_msg;
        pos   = pending_pos;
        gap_half = pending_gap_half;
        pending_gap_half = 0;
        seq = nullptr;
        elem = 0;
        return 0;
    }

    const char* msg = msgs[msg_i];
    char c = msg[pos];
    if (c == '\0') {
        if (!auto_repeat && msg_i == (MSG_COUNT - 1)) {
            auto_finished = true;
            return 0;
        }
        msg_i = (uint8_t)((msg_i + 1) % MSG_COUNT);
        pos = 0;
        gap_half = 12;
        return 0;
    }

    if (c == ' ') {
        while (msg[pos] == ' ') pos++;
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
        if (msg[look] == ' ') {
            uint8_t nsp = 0;
            while (msg[look] == ' ') { nsp++; look++; }

            printAscii(32);
            if (msg[look] == '\0') {
                if (!auto_repeat && msg_i == (MSG_COUNT - 1)) {
                    auto_finished = true;
                    return 0;
                }
                pending_msg = (uint8_t)((msg_i + 1) % MSG_COUNT);
                pending_pos = 0;
            } else {
                pending_msg = msg_i;
                pending_pos = look;
            }

            pending_gap_half = (int)(14 * nsp - 2);
            pending_jump = true;
        } else if (msg[look] == '\0') {
            if (!auto_repeat && msg_i == (MSG_COUNT - 1)) {
                auto_finished = true;
                return 0;
            }
            pending_msg = (uint8_t)((msg_i + 1) % MSG_COUNT);
            pending_pos = 0;
            pending_gap_half = 12;
            pending_jump = true;
        } else {
            pending_msg = msg_i;
            pending_pos = look;
            pending_gap_half = 4;
            pending_jump = true;
        }
    }

    return out;
}
