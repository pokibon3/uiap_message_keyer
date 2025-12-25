#pragma once
#include <stdint.h>
#include <stdbool.h>

void printAscii(int8_t c);
void setDisplayEnabled(bool enabled);
void displayFlushIfNeeded(void);
void displayProcessQueue(void);

extern "C" int mini_snprintf(char* buffer, unsigned int buffer_len, const char *fmt, ...);
