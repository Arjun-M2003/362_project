/**
  ******************************************************************************
  * @file    main.c
  * @author  HyunJoon Cho, Austin Rejman, Nikhil Vaidyanath, Arjun Mangipudi
  * @date    Nov 3, 2024
  * @brief   ECE 362 Team 78 Project Code
  ******************************************************************************
*/

#define MINITERATIONS 100
#include "stm32f0xx.h"
float temp; // global temp
float moisture; // global soil moisture
float upperTemp;
float lowerTemp;
float upperMoisture;
float lowerMoisture;

int main (void) {
    internal_clock();
    enable_ports();
    setup_tim1(4800);
    ledGreen(); // init to green


    int iterations = 0; // for fan and led to not constantly change

    for (;;) {
        // read some temperature and moisture
        if ((temp > upperTemp || temp < lowerTemp || moisture > upperMoisture || moisture < lowerMoisture) 
        && iterations >= MINITERATIONS)
        {
            ledRed();
            if (temp > upperTemp)
                fanOn();
            else
                fanOff();

            // reset iterations when LED is changed
            iterations = 0;
        } else if (iterations >= MINITERATIONS)
        {
            ledGreen();
            fanOff();
        }

        // output to OLED here
        iterations++;
    }
}

void fanOn(void) {
    GPIOB -> ODR = 0x0100;
}

void fanOff(void) {
    GPIOB -> ODR = 0x0000;
}

void ledGreen(void) {
    TIM1 -> CR1 &= ~TIM_CR1_CEN; // disable timer to change ARR
    TIM1 -> ARR = 4800; // very small period
    int rgb = 0x00FF00;
    setrgb(rgb);
    TIM1 -> CR1 |= TIM_CR1_CEN; // reenable timer
}

void ledRed(void) {
    TIM1 -> CR1 &= ~TIM_CR1_CEN; // disable timer to change ARR
    TIM1 -> ARR = 9600000; // 0.5 sec
    int rgb = 0xFF0000;
    setrgb(rgb);
    TIM1 -> CR1 |= TIM_CR1_CEN; // reenable timer
}

//===========================================================================
// Configure GPIOB for Fan, Status LED, and Keypad
//===========================================================================
void enable_ports(void) {
    // Configure PB 8 to be output to turn on the fan
    RCC -> AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB -> MODER &= ~GPIO_MODER_MODER8;
    GPIOB -> MODER |= GPIO_MODER_MODER8_0;
    // Now GPIOB ODR can be set to 0x0100 to turn on Fan at any point

    // Set PA8, PA9, and PA10 for the status LED
    RCC -> AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA -> MODER &= ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER9 | GPIO_MODER_MODER10 | GPIO_MODER_MODER11);
    GPIOA -> MODER |= GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1 | GPIO_MODER_MODER10_1 | GPIO_MODER_MODER11_1;
    GPIOA -> AFR[1] |= (0x2 << GPIO_AFRH_AFSEL8_Pos) | (0x2 << GPIO_AFRH_AFSEL9_Pos) | (0x2 << GPIO_AFRH_AFSEL10_Pos) |
        (0x2 << GPIO_AFRH_AFSEL11_Pos);
}

// set the timer for PWM
/*void setup_tim1(void) {
    RCC -> APB2ENR |= RCC_APB2ENR_TIM1EN;
    TIM1 -> BDTR |= TIM_BDTR_MOE;
    TIM1 -> PSC = 1 - 1;
    TIM1 -> ARR = 2400 - 1;
    TIM1 -> CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
    TIM1 -> CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;

    TIM1 -> CCMR2 |= TIM_CCMR2_OC4PE;
    TIM1 -> CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM1 -> CR1 |= TIM_CR1_CEN;
} */

//============================================================================
// PWM Lab Functions
//============================================================================
void setup_tim1(int ARR) {
    RCC -> APB2ENR |= RCC_APB2ENR_TIM1EN;
    /*RCC -> AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA -> MODER |= GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1 | GPIO_MODER_MODER10_1 | GPIO_MODER_MODER11_1;
    GPIOA -> AFR[1] |= (0x2 << GPIO_AFRH_AFSEL8_Pos) | (0x2 << GPIO_AFRH_AFSEL9_Pos) | (0x2 << GPIO_AFRH_AFSEL10_Pos) |
        (0x2 << GPIO_AFRH_AFSEL11_Pos); */

    TIM1 -> BDTR |= TIM_BDTR_MOE;
    TIM1 -> PSC = 1 - 1;
    TIM1 -> ARR = ARR - 1;
    TIM1 -> CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
    TIM1 -> CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;

    TIM1 -> CCMR2 |= TIM_CCMR2_OC4PE;
    TIM1 -> CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM1 -> CR1 |= TIM_CR1_CEN; // timer enable for PWM
}

// Accept a byte in BCD format and convert it to decimal
uint8_t bcd2dec(uint8_t bcd) {
    // Lower digit
    uint8_t dec = bcd & 0xF;

    // Higher digit
    dec += 10 * (bcd >> 4);
    return dec;
}

void setrgb(int rgb) {
    uint8_t b = bcd2dec(rgb & 0xFF);
    uint8_t g = bcd2dec((rgb >> 8) & 0xFF);
    uint8_t r = bcd2dec((rgb >> 16) & 0xFF);

    // Set rgb values
    int arr = TIM1 -> ARR + 1;
    TIM1 -> CCR1 = ((100 - r) * (arr)) / 100;
    TIM1 -> CCR3 = ((100 - b) * (arr)) / 100;
    TIM1 -> CCR2 = ((100 - g) * (arr)) / 100;
}
