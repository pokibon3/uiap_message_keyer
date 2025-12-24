//
//    CW Decoder header
//
#pragma once

int16_t decode(char *code, char *mode);
const char* encode_us_char(char c);

#define  MODE_US    0
#define  MODE_JP    1
