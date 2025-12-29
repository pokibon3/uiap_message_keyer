#pragma once
#include <stdint.h>
#include <stdbool.h>

extern int key_spd;
extern int wpm;
extern bool tone_enabled;
extern volatile bool display_request;
extern volatile bool auto_mode;
extern volatile bool auto_repeat;
extern volatile bool req_start_auto;
extern volatile bool req_stop_auto;
extern volatile bool req_reset_auto;
extern volatile bool auto_finished;
extern volatile uint8_t req_start_msg;

uint8_t job_paddle(void);
void keydown(void);
void keyup(void);
