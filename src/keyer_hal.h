/*
  ==== ?????? ====
  - PD1 ? SWIO(????/????) ??????????
*/
#include <stdint.h>
#define micros() (SysTick->CNT / DELAY_US_TIME)
#define millis() (SysTick->CNT / DELAY_MS_TIME)

#define PIN_DOT    PC5     // ??? DOT
#define PIN_DASH   PC6     // ??? DASH
//#define PIN_TONE   PC7     // ????? (TIM2_CH1, PWM 600Hz)
#define PIN_KEYOUT PC0     // KEYOUT ??
#define PIN_SPEED  PA2     // ???? ADC (PA2/IN0)
#define PIN_SWA    PD0     // SW A
#define PIN_SWB    PC3     // SW B
// ---- PWM?? (TIM2 output set 0: CH1=D4, CH2=D3, CH3=C0, CH4=D7) ----
//#define PWM_TONE  GPIOv_from_PORT_PIN(GPIO_port_C, 7)  // PD4 = TIM2_CH1

extern int GPIO_setup(void);
extern void start_pwm(void);
extern void stop_pwm(void);
extern void tim2_pwm_init(void);
void pwm_set_freq(uint32_t hz);
void pwm_restore_default(void);
extern void tim1_int_init(void);
void tim1_int_suspend(void);
void tim1_int_resume(void);

extern "C" void TIM1_UP_IRQHandler(void) __attribute__((interrupt));
