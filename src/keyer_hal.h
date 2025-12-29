/*
  ==== ?????? ====
  - PD1 ? SWIO(????/????) ??????????
*/
#include <stdint.h>
#define micros() (SysTick->CNT / DELAY_US_TIME)
#define millis() (SysTick->CNT / DELAY_MS_TIME)

#define PIN_DOT    PC5     // ??? DOT
#define PIN_DASH   PC6     // ??? DASH
#define PIN_TONE   PC7     // ??? TONE output
#define PIN_KEYOUT PC0     // KEYOUT ??
#define PIN_SPEED  PA2     // ???? ADC (PA2/IN0)
#define PIN_SWA    PD0     // SW A
#define PIN_SWB    PC3     // SW B

extern int GPIO_setup(void);
extern void tim1_int_init(void);
void tim1_int_suspend(void);
void tim1_int_resume(void);

extern "C" void TIM1_UP_IRQHandler(void) __attribute__((interrupt));
