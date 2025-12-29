#include "ch32fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "keyer_hal.h"
#include <stdio.h>
#include <stdint.h>

uint32_t count;

void tim1_int_init()
{
    // Enable TIM1 Clock
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
    RCC->APB2PCENR |= RCC_APB2Periph_TIM1;

    // Reset TIM1 to init all regs
      //RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
      //RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;

    // ??????? TIM1
    TIM1->CTLR1 = 0;
    TIM1->ATRLR = 256 - 1;                // 256 -1  256us
    TIM1->PSC = 48 - 1;  // 1000kHz        // 48 -1   1us
    TIM1->RPTCR = 0;                    // ??????????

    TIM1->SWEVGR = TIM_PSCReloadMode_Immediate;

    // ??????????
    TIM1->INTFR = (uint16_t)~TIM_IT_Update;

    // ?????????
    // PreemptionPriority = 0, SubPriority = 1
    NVIC->IPRIOR[TIM1_UP_IRQn] = 0 << 7 | 1 << 6;
    // ???????
    NVIC->IENR[((uint32_t)(TIM1_UP_IRQn) >> 5)] |= (1 << ((uint32_t)(TIM1_UP_IRQn) & 0x1F));

    // ???????
    TIM1->DMAINTENR |= TIM_IT_Update;

    // Enable
    TIM1->CTLR1 |= TIM_CEN;
}

void tim1_int_suspend()
{
    TIM1->DMAINTENR &= (uint16_t)~TIM_IT_Update;
}

void tim1_int_resume()
{
    TIM1->DMAINTENR |= TIM_IT_Update;
}

//==================================================================
//    setup
//==================================================================
int GPIO_setup()
{
    // Enable GPIO Ports A, C, D
    GPIO_port_enable(GPIO_port_A);
    GPIO_port_enable(GPIO_port_C);
    GPIO_port_enable(GPIO_port_D);
    // Set Pin Modes
    GPIO_pinMode(PIN_DOT, GPIO_pinMode_I_pullUp, GPIO_Speed_10MHz);
    GPIO_pinMode(PIN_DASH, GPIO_pinMode_I_pullUp, GPIO_Speed_10MHz);
    GPIO_pinMode(PIN_SWA, GPIO_pinMode_I_pullUp, GPIO_Speed_10MHz);
    GPIO_pinMode(PIN_SWB, GPIO_pinMode_I_pullUp, GPIO_Speed_10MHz);
    GPIO_pinMode(PIN_SPEED, GPIO_pinMode_I_analog, GPIO_Speed_10MHz);
    GPIO_pinMode(PIN_KEYOUT, GPIO_pinMode_O_pushPull, GPIO_Speed_10MHz);
    GPIO_pinMode(PIN_TONE, GPIO_pinMode_O_pushPull, GPIO_Speed_10MHz);
    GPIO_digitalWrite(PIN_TONE, low);
    return 0;
}
