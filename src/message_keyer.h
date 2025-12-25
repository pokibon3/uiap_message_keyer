#pragma once
#include <stdint.h>

#define SQZ_FREE 0
#define SQZ_SPC0 1
#define SQZ_SPC  2
#define SQZ_DOT0 3
#define SQZ_DOT  4
#define SQZ_DAH_CONT0 5
#define SQZ_DAH_CONT1 6
#define SQZ_DAH_CONT  7
#define SQZ_DASH 8

#define PDL_DOT  1
#define PDL_DASH 2
#define PDL_FREE 0

#define SQUEEZE_TYPE 0
#define PDL_RATIO 4

#define WPM_MAX 40
#define WPM_MIN 5

const char* morseForChar(char c);
uint8_t job_auto();
void set_message_buffers(uint8_t *msg_a, uint8_t *msg_b);
void set_active_message(uint8_t index);
