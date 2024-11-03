/**
  ******************************************************************************
  * @file    main.c
  * @author  HyunJoon Cho, Austin Rejman, Nikhil Vaidyanath, Arjun Mangipudi
  * @date    Nov 3, 2024
  * @brief   ECE 362 Team 78 Project Code
  ******************************************************************************
*/

#include "stm32f0xx.h"

int main (void) {
    internal_clock();
    enable_ports();
}

//===========================================================================
// Configure GPIOB for Fan, Status LED, and Keypad
//===========================================================================
void enable_ports(void) {
    // Configure PB 8 to be output to turn on the fan
    RCC -> AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB -> MODER &= ~GPIO_MODER_MODER8;
    GPIOB -> MODER |= GPIO_MODER_MODER8_0;
    // Now ODR can be set to 0100 to turn on Fan

    // Set PA8, PA9, and PA10 for the status LED
    RCC -> AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA -> MODER &= ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER9 | GPIO_MODER_MODER10 | GPIO_MODER_MODER11);
    GPIOA -> MODER |= GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1 | GPIO_MODER_MODER10_1 | GPIO_MODER_MODER11_1;
    GPIOA -> AFR[1] |= (0x2 << GPIO_AFRH_AFSEL8_Pos) | (0x2 << GPIO_AFRH_AFSEL9_Pos) | (0x2 << GPIO_AFRH_AFSEL10_Pos) |
        (0x2 << GPIO_AFRH_AFSEL11_Pos);
}

// set the timer for PWM
void setup_tim1(void) {
    RCC -> APB2ENR |= RCC_APB2ENR_TIM1EN;
    TIM1 -> BDTR |= TIM_BDTR_MOE;
    TIM1 -> PSC = 1 - 1;
    TIM1 -> ARR = 2400 - 1;
    TIM1 -> CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
    TIM1 -> CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;

    TIM1 -> CCMR2 |= TIM_CCMR2_OC4PE;
    TIM1 -> CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM1 -> CR1 |= TIM_CR1_CEN;
}
