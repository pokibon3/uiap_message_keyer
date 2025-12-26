#include <stdio.h>
#include <stdlib.h>
#include "decode.h"
#include "ch32fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "keyer_hal.h"
#include "print_ascii.h"

//#define SERIAL_OUT

static uint16_t realstate = low;
static uint16_t realstatebefore = low;
static uint16_t filteredstate = low;
static uint16_t filteredstatebefore = low;
static uint32_t starttimehigh;
static uint32_t highduration;
static uint32_t lasthighduration;
static uint32_t hightimesavg = 60; // start at ~20WPM (dot=60ms)
static uint32_t startttimelow;
static uint32_t lowduration;
static uint32_t laststarttime = 0;
#define  nbtime     6  /// ms noise blanker
#define  MIN_TRAIN_MS 10  /// ignore shorter pulses on startup/noise

static char code[20];
static uint16_t stop = low;
static uint16_t wpm;

static char        sw = MODE_US;

const int colums = 13; /// have to be 16 or 20

int lcdindex = 0;
uint8_t line1[colums];
uint8_t line2[colums];
uint8_t lastChar = 0;

//==================================================================
// gap 
//==================================================================
typedef enum {
    GAP_INTRA = 0,
    GAP_CHAR  = 1,
    GAP_WORD  = 2 
} gap_type_t;

static gap_type_t classify_gap(uint32_t gap, uint32_t unit)
{
    if (unit == 0) return GAP_INTRA; // まだ学習前の保護

    // gap < 1.5 * unit 
    if (gap < (unit * 3) / 2) {
        return GAP_INTRA;
    }
    // 1.5?4.5 * unit 
    if (gap < (unit * 9) / 2) {
        return GAP_CHAR;
    }
    // 5 * unit 
    if (gap >= unit * 5) {
        return GAP_WORD;
    }

    // 4.5 * unit 
    return GAP_CHAR;
}

//==================================================================
//    cwDecoder : deocder main
//==================================================================
static int decodeAscii(int16_t asciinumber)
{
    if (asciinumber == 0) return 0;
    if (lastChar == 32 && asciinumber == 32) return 0;

    if        (asciinumber == 1) {            // AR
        printAscii('A');
        printAscii('R');
    } else if (asciinumber == 2) {            // KN
        printAscii('K');
        printAscii('N');
    } else if (asciinumber == 3) {            // BT
        printAscii('B');
        printAscii('T');
    } else if (asciinumber == 4) {            // VA
        printAscii('V');
        printAscii('A');
    } else if (asciinumber == 5) {            // ホレ
        printAscii(0xce);
        printAscii(0xda);
    } else if (asciinumber == 6) {            // ラタ
        printAscii(0xd7);
        printAscii(0xc0);
    } else {
        printAscii(asciinumber);
    }
    lastChar = asciinumber;

    return 0;
}

void setstate(uint16_t state)
{
    realstate = state;
}

//==================================================================
//    cwDecoder : deocder main
//==================================================================
int cwDecoder()
{
        /////////////////////////////////////////////////////
        // here we clean up the state with a noise blanker //
        /////////////////////////////////////////////////////
        if (realstate != realstatebefore){
            laststarttime = millis();
        }
        if ((millis()-laststarttime)> nbtime) {
            if (realstate != filteredstate) {
                filteredstate = realstate;
            }
        }

        ////////////////////////////////////////////////////////////
        // Then we do want to have some durations on high and low //
        ////////////////////////////////////////////////////////////
        if (filteredstate != filteredstatebefore) {
            if (filteredstate == high) {
                starttimehigh = millis();
                lowduration = (millis() - startttimelow);
            }
            if (filteredstate == low) {
                startttimelow = millis();
                highduration = (millis() - starttimehigh);
                if (highduration >= MIN_TRAIN_MS) {
                    if (highduration < (2*hightimesavg) || hightimesavg == 0) {
                        hightimesavg = (highduration+hightimesavg+hightimesavg) / 3;     // now we know avg dit time ( rolling 3 avg)
                    }
                    if (highduration > (5*hightimesavg) ) {
                        hightimesavg = highduration+hightimesavg;     // if speed decrease fast ..
                    }
                }
            }
        }

        ///////////////////////////////////////////////////////////////
        // now we will check which kind of baud we have - dit or dah //
        // and what kind of pause we do have 1 - 3 or 5 pause        //
        // we think that hightimeavg = 1 bit                         //
        ///////////////////////////////////////////////////////////////
        if (filteredstate != filteredstatebefore){
            stop = low;
            if (filteredstate == low){  //// we did end a HIGH
                if (highduration < (hightimesavg*2) && highduration > (hightimesavg*0.6)){ /// 0.6 filter out false dits
                    strcat(code,".");
//                    printf(".");
                }
                if (highduration > (hightimesavg*2) && highduration < (hightimesavg*6)){
                    strcat(code,"-");
//                    printf("-");
                    wpm = (wpm + (1200/((highduration)/3)))/2;  //// the most precise we can do ;o)
                }
            }
        }
        if (filteredstate == high) {  //// we did end a LOW

            if (hightimesavg >= MIN_TRAIN_MS) {
                gap_type_t g = classify_gap(lowduration, hightimesavg);

                if (g == GAP_CHAR) {          // ???
                    if (strlen(code) > 0) {
                        decodeAscii(decode(code, &sw));
                        code[0] = '\0';
                    }
                } else if (g == GAP_WORD) {   // 単語間
                    if (strlen(code) > 0) {
                        decodeAscii(decode(code, &sw));
                        code[0] = '\0';
                    }
                    decodeAscii(32);           // ??????
                }
            }
        }

        //////////////////////////////
        // write if no more letters //
        //////////////////////////////
        uint32_t unit = (hightimesavg >= MIN_TRAIN_MS) ? hightimesavg : 0;
        if (unit > 0 && (millis() - startttimelow) > unit * 5 && stop == low) {
            decodeAscii(decode(code, &sw));
            code[0] = '\0';
            stop = high;
        }

        //////////////////////////////////
        // the end of main loop clean up//
        /////////////////////////////////
        realstatebefore = realstate;
        lasthighduration = highduration;
        filteredstatebefore = filteredstate;
    return 0;
}
