#pragma once
#include <stdint.h>
#include <stdbool.h>

void printAscii(int8_t c);
void setDisplayEnabled(bool enabled);
void displayFlushIfNeeded(void);
void displayProcessQueue(void);
void setPrintAsciiHook(bool (*hook)(int8_t c));
void printAsciiReset(void);
void printAsciiBackspace(void);
void printAsciiNewline(void);
bool printAsciiAtLineStart(void);
