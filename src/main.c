/**
  ******************************************************************************
  * @file    main.c
  * @author  HyunJoon Cho, Austin Rejman, Nikhil Vaidyanath, Arjun Mangipudi
  * @date    Nov 3, 2024
  * @brief   ECE 362 Team 78 Project Code
  ******************************************************************************
*/

void internal_clock();
void fanOn(void);
void fanOff(void);
void ledGreen(void);
void ledRed(void);
void enable_ports(void);
void setup_tim1(int ARR);


#define MINITERATIONS 100000
#include "stm32f0xx.h"
#include <stdlib.h>

float temp; // global temp
float moisture; // global soil moisture
float upperTemp;
float lowerTemp;
float upperMoisture;
float lowerMoisture;

int main (void) {
    internal_clock();
    enable_ports();
    setup_tim1(2400);
    ledGreen(); // init to green

    int iterations = 0; // for fan and led to not constantly change
    upperTemp = 10;
    lowerTemp = 0;
    moisture;
    upperMoisture = 11;
    lowerMoisture = 9;
    temp;

    for (;;) {
        // read some temperature and moisture
        temp = readTemperature();
        moisture = readMoisture();
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
    TIM1 -> CCR1 = TIM1 -> ARR;
    TIM1 -> CCR2 = 0;
    TIM1 -> CCR3 = TIM1 -> ARR;
    //TIM1 -> ARR = ; // very small period
    //int rgb = 0x00FF00;
    //setrgb(rgb);
    TIM1 -> CR1 |= TIM_CR1_CEN; // reenable timer
}

void ledRed(void) {
    
    TIM1 -> CR1 &= ~TIM_CR1_CEN; // disable timer to change ARR
    TIM1 -> CCR3 = TIM1 -> ARR;
    TIM1 -> CCR2 = TIM1 -> ARR;
    TIM1 -> CCR1 = 1200;
    //TIM1 -> ARR = 9600000; // 0.5 sec
    //int rgb = 0xFF0000;
    //setrgb(rgb);
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

void setup_tim1(int ARR) {
    RCC -> APB2ENR |= RCC_APB2ENR_TIM1EN;
    /*RCC -> AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA -> MODER |= GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1 | GPIO_MODER_MODER10_1 | GPIO_MODER_MODER11_1;
    GPIOA -> AFR[1] |= (0x2 << GPIO_AFRH_AFSEL8_Pos) | (0x2 << GPIO_AFRH_AFSEL9_Pos) | (0x2 << GPIO_AFRH_AFSEL10_Pos) |
        (0x2 << GPIO_AFRH_AFSEL11_Pos); */

    TIM1 -> BDTR |= TIM_BDTR_MOE;
    TIM1 -> PSC = 20000 - 1;
    TIM1 -> ARR = ARR - 1;
    TIM1 -> CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
    TIM1 -> CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;

    TIM1 -> CCMR2 |= TIM_CCMR2_OC4PE;
    TIM1 -> CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

    TIM1 -> CR1 |= TIM_CR1_CEN; // timer enable for PWM
}

void readMonitor(char* buff) {
    //while (!(USART5->ISR & USART_ISR_RXNE));
    int i = 0;
    char c;
    
    // loop until '\n'
    while (1) {
        // Wait until RDR is not empty
        while (!(USART5->ISR & USART_ISR_RXNE));

        c = USART5->RDR;
        buff[i] = c;
        i++;
        
        if (c == '\n')
            break;
    }

    buff[i] = '\0';
}

int sendChar(int c) {
    while(!(USART5->ISR & USART_ISR_TXE));
    USART5->TDR = c;
    return c;
}

int readTemperature()
{
    sendChar('t');
    char buff[10];
    readMonitor(buff);
    int celsius = atoi(buff + 2);
    return celsius * 1.8 + 32;
}

int readMoisture()
{
    sendChar('w');
    char buff[10];
    readMonitor(buff);
    return atoi(buff + 2);
}

void init_usart5() {
    RCC -> AHBENR |= RCC_AHBENR_GPIOCEN;
    RCC -> AHBENR |= RCC_AHBENR_GPIODEN;
    
    GPIOC -> MODER &= ~GPIO_MODER_MODER12;
    GPIOD -> MODER &= ~GPIO_MODER_MODER2;
    GPIOC -> MODER |= GPIO_MODER_MODER12_1;
    GPIOD -> MODER |= GPIO_MODER_MODER2_1;

    GPIOC -> AFR[1] &= ~GPIO_AFRH_AFRH4;
    GPIOD -> AFR[0] &= ~GPIO_AFRL_AFRL2;
    GPIOC -> AFR[1] |= 0x2 << GPIO_AFRH_AFRH4_Pos; // AFR 2
    GPIOD -> AFR[0] |= 0x2 << GPIO_AFRL_AFRL2_Pos;

    RCC -> APB1ENR |= RCC_APB1ENR_USART5EN;
    USART5 -> CR1 &= ~USART_CR1_UE;
    USART5 -> CR1 &= ~(USART_CR1_M1 | USART_CR1_M0);

    USART5 -> CR2 &= ~USART_CR2_STOP;
    USART5 -> CR1 &= ~USART_CR1_PCE;
    USART5 -> CR1 &= ~USART_CR1_OVER8;
    USART5 -> BRR = 0x1388; // baud rate of 9600
    USART5 -> CR1 |= USART_CR1_TE | USART_CR1_RE;

    USART5 -> CR1 |= USART_CR1_UE;
    while (!(USART5 -> ISR & USART_ISR_TEACK) | !(USART5 -> ISR & USART_ISR_REACK));
}
