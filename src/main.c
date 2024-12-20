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
void set_char_msg(int, char);
int readMoisture();
int readTemperature();
int sendChar(int c);
void readMonitor(char* buff);
void init_usart5();
void spi1_display1(char *string);
void spi1_display2(char *string);
void spi1_init_oled(void);
void drive_column(int);   // energize one of the column outputs
int  read_rows();         // read the four row inputs
void update_history(int col, int rows); // record the buttons of the driven column
char get_key_event(void); // wait for a button event (press or release)
char get_keypress(void);  // wait for only a button press event.
void show_keys(void);     // demonstrate get_key_event()
void nano_wait(int);
void init_tim7(void);
void init_spi2();
void spi2_setup_dma();
void spi2_enable_dma();
void init_spi1();
void small_delay();
int getint();
void init_tim2();


#define MINITERATIONS 15
#include "stm32f0xx.h"
#include <stdlib.h>
#include <stdio.h>

int temp = 10; // global temp
int moist = 10; // global soil moisture
int iterations = 0;
int status = 1;
float upperTemp;
float lowerTemp;
float upperMoisture;
float lowerMoisture;
int sTemp = 60;
int eTemp = 80;
int sMoist = 0;
int eMoist = 8;

int IRQTimer = 0;

//char key;

int main (void) {
    internal_clock();
    enable_ports();
    setup_tim1(2400);
    ledGreen(); // init to green
    init_usart5();


    NVIC->ICER[0] = 1<<TIM17_IRQn;
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    init_spi1();
    spi1_init_oled();

    //int temp = 10;
    //int moist = 10;
    temp = readTemperature();
    moist = readMoisture();
    char tempB[32];
    char moistB[32];
    sprintf(tempB, "Temperature:%3dF", temp);
    spi1_display1(tempB);
    sprintf(moistB, "   Moisture:%3d%%", moist);
    spi1_display2(moistB);
    
    init_tim2();
    init_tim7();

    for (;;) {
        char key = get_keypress();
        
        if (key == 'A') {
            // When 'A' is pressed, display "Enter" on the OLED
            status = 0;
            sTemp = 150;
            while(sTemp > 140){
                spi1_display1("Enter Start of   ");
                spi1_display2("Range for Temp:   ");
                small_delay();
                sTemp = getint();
            }
            eTemp = 150;
            while(eTemp >140 || eTemp <= sTemp){
                spi1_display1("Enter End of   ");
                spi1_display2("Range for Temp:   ");
                small_delay();
                eTemp = getint();
            }

            iterations = 0;
            status = 1;
        }
        if (key == 'B') {
            // When 'B' is pressed, display "Enter" on the OLED
            sMoist = 101;
            eMoist = 101;
            status = 0;
            while(sMoist > 99){
                spi1_display1("Enter Start of   ");
                spi1_display2("Range for Moist:   ");
                sMoist = getint();
                small_delay();
            }
            // Now wait for the '#' key to be pressed to display "End"
            while(eMoist > 100 || eMoist <= sMoist){
                spi1_display1("Enter End of   ");
                spi1_display2("Range for Moist:   ");
                eMoist = getint();
                small_delay();
            }

            iterations = 0;
            status = 1;
        }
        if (key == 'C') { //Display status
            sprintf(tempB, "Temperature:%3dF", temp);
            spi1_display1(tempB);
            sprintf(moistB, "   Moisture:%3d%%", moist);
            spi1_display2(moistB);
            if((eTemp < temp)){
                fanOn();
                ledRed();
            } else {
                fanOff();
            }
            if((sTemp > temp || eMoist < moist || sMoist > moist)){
                ledRed();
                status = 0;
            } else {
                ledGreen();
                status = 1;
            }

            status = 1;
        }

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

    // Turn on Green always
    TIM1 -> CCR1 = TIM1 -> ARR;
    TIM1 -> CCR2 = 0;
    TIM1 -> CCR3 = TIM1 -> ARR;
    TIM1 -> CR1 |= TIM_CR1_CEN; // reenable timer
}

void ledRed(void) {
    
    TIM1 -> CR1 &= ~TIM_CR1_CEN; // disable timer to change ARR
    TIM1 -> CCR3 = TIM1 -> ARR;
    TIM1 -> CCR2 = TIM1 -> ARR;

    // Red is on 50% duty cycle (50% * 1hz = 0.5 seconds on and off)
    TIM1 -> CCR1 = 1200;
    TIM1 -> CR1 |= TIM_CR1_CEN; // reenable timer
}

// GPIO Setup for keypad, fan, and status LED
void enable_ports(void) {
    //Enable port C for the keypad
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~0xffff;
    GPIOC->MODER |= 0x55 << (4*2);            
    GPIOC->OTYPER &= ~0xff;
    GPIOC->OTYPER |= 0xf0;
    GPIOC->PUPDR &= ~0xff;
    GPIOC->PUPDR |= 0x55;

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

// Advanced Timer 1 for PWM Output on Channels 1-3 with 1hz
void setup_tim1(int ARR) {
    RCC -> APB2ENR |= RCC_APB2ENR_TIM1EN;

    TIM1 -> BDTR |= TIM_BDTR_MOE;
    TIM1 -> PSC = 20000 - 1;
    TIM1 -> ARR = ARR - 1;
    TIM1 -> CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
    TIM1 -> CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;

    TIM1 -> CCMR2 |= TIM_CCMR2_OC4PE;
    TIM1 -> CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

    TIM1 -> CR1 |= TIM_CR1_CEN; // timer enable for PWM
}

// read the response from the plant monitor
void readMonitor(char* buff) {
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

// Send request to the plant monitor
int sendChar(int c) {
    while(!(USART5->ISR & USART_ISR_TXE));
    USART5->TDR = c;
    return c;
}

int readTemperature()
{
    sendChar('t');
    char buff[10];
    char c;
    int i = 0;
    while (1) {
        // Wait until RDR is not empty
        while (!(USART5->ISR & USART_ISR_RXNE));

        c = USART5->RDR;
        buff[i] = c;
        i++;
        
        if (c == '\n')
            break;
    }
    int celsius = atoi(buff + 2);
    return celsius * 1.8 + 32; // celsius to fahrenheit
}

int readMoisture()
{
    sendChar('w');
    char buff[10];
    readMonitor(buff);
    return atoi(buff + 2);
}

// Initialize UART to communicate with plant monitor at baud rate 9600
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

uint8_t col; // the column being scanned

int msg_index = 0;
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
extern const char font[];

void setup_bb(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->MODER &= 0x00ffffff;
    GPIOB->MODER |= 0x45000000;

    //cs to high
    GPIOB->BSRR |= 1 << 12;
    //sck to low
    GPIOB->BRR = 1 << 13;
}

void small_delay(void) {
    nano_wait(50000);
}

void bb_write_bit(int val) {
    // CS (PB12)
    // SCK (PB13)
    // SDI (PB15)
    if(val>0){
        GPIOB->BSRR |= 1 << 15;
    } else {
        GPIOB->BRR = 1 << 15;
    }
    small_delay();
    //set SCK to 0
    GPIOB->BRR = 1 << 13;
    small_delay();
    //set SCK to 1
    GPIOB->BSRR |= 1 << 13;
}

// Set CS (PB12) low,
// write 16 bits using bb_write_bit,
// then set CS high.
void bb_write_halfword(int halfword) {
    GPIOB->BRR = 1 << 12;

    //iterate though halfword
    for(int x = 15; x > -1; x--){
        int temp = (halfword >> x) & 0b1;
        bb_write_bit(temp);
    }

    // Set CS to 1
    GPIOB->BSRR |= 1 << 12;
}

void drive_bb(void) {
    for(;;)
        for(int d=0; d<8; d++) {
            bb_write_halfword(msg[d]);
            nano_wait(1000000); // wait 1 ms between digits
        }
}

void init_tim15(void) {
  //Enable RCC clock for TIM15
  RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;

  // Set PSC and ARR for 1kHz Result
  TIM15->PSC = 480 - 1;
  TIM15->ARR = 100 - 1;

  //Enable UIE
  TIM15->DIER |= TIM_DIER_UDE;
  
  //Enable timer 15
  TIM15->CR1 |= TIM_CR1_CEN;

}

// Timer 2 for temperature update at 1hz through interrupt
void init_tim2(void){
    RCC -> APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2 -> PSC = 48000 - 1;
    TIM2 -> ARR = 1000 - 1;

    TIM2 -> DIER |= TIM_DIER_UIE;

    NVIC->ISER[0] |= 1 << TIM2_IRQn;

    TIM2 -> CR1 |= TIM_CR1_CEN;
}

void TIM2_IRQHandler() {
    TIM2 -> SR &= ~TIM_SR_UIF;

    temp = readTemperature();
    moist = readMoisture();

    char tempB[32];
    char moistB[32];
    if (!status)
    {
        return;
    }

    if (temp < sTemp){
        spi1_display1("      ERROR       ");
        spi1_display2("  Temp Too Low!   ");
        ledRed();
        fanOff();
        status = 0;
    }
    else if (temp > eTemp){
        spi1_display1("      ERROR       ");
        spi1_display2("  Temp Too High!   ");
        ledRed();
        fanOn();
        //turn on led, fan
        status = 0;
    }
    else if (moist < sMoist){
        spi1_display1("      ERROR       ");
        spi1_display2("Moisture Too Low!   ");
        ledRed();
        status = 0;
    }
    else if (moist > eMoist){
        spi1_display1("      ERROR       ");
        spi1_display2("Moisture Too High!   ");
        ledRed();
        status = 0;
    } else { //no errors
        //turn off fan & led
        ledGreen();
        sprintf(tempB, "Temperature:%3dF", temp);
        spi1_display1(tempB);
        sprintf(moistB, "   Moisture:%3d%%", moist);
        spi1_display2(moistB);
        fanOff();
        status = 1;
        IRQTimer--;
    }
}


// Timer 7 for reading the keypad inputs, interrupt at 1kHz
void init_tim7(void) {
    //Enable RCC clock for TIM7
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
    
    // Set PSC and ARR for 1KHz Result
    TIM7->PSC = 480 - 1;
    TIM7->ARR = 100 - 1;

    //Enable UIE
    TIM7->DIER |= TIM_DIER_UIE;

    //Enable interupt for Timer 7
    NVIC->ISER[0] |= 1 << TIM7_IRQn;

    //Enable timer 7
    TIM7->CR1 |= TIM_CR1_CEN;
}

void TIM7_IRQHandler() {
    //Acknowledge the interrupt
    TIM7->SR &= ~TIM_SR_UIF;

    //read row values
    int rows = read_rows();

    //update history
    update_history(col,rows);
    
    //update column
    col = (col + 1) & 3;

    //Drive new column
    drive_column(col);
}

// Initialize the SPI2 peripheral.
void init_spi2(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->MODER &= 0x00ffffff;
    GPIOB->MODER |= 0x8a000000;
    //Enable SPI clock
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    
    //Clear CR1_SPE bit
    SPI2->CR1 &= ~SPI_CR1_SPE;
    //Set baud rate & master mode (/256)
    SPI2->CR1 |=  SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0;
    // Interface for 16-bit word size (1111)
    SPI2->CR2 = SPI_CR2_DS_3 | SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0;
    //master configuration
    SPI2->CR1 |= SPI_CR1_MSTR;
    //SS output, NSSP
    SPI2->CR2 |= SPI_CR2_SSOE | SPI_CR2_NSSP; 
    //TXDMAEN enable
    SPI2->CR2 |= SPI_CR2_TXDMAEN;
    //Enable SPI
    SPI2->CR1 |= SPI_CR1_SPE;
}

// Configure the SPI2 peripheral to trigger the DMA channel when the
// transmitter is empty.
void spi2_setup_dma(void) {
    //setup_dma from lab 5
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel5->CCR &= ~DMA_CCR_EN; //turn off the enable bit
    DMA1_Channel5->CMAR = (uint32_t) msg; //set CMAR to the address of the msg array.
    DMA1_Channel5->CPAR = (uint32_t) (&(SPI2->DR )); //Set CPAR to the address of the GPIOB_ODR register.
    DMA1_Channel5->CNDTR = 8; //Set CNDTR to 8. (the amount of LEDs.)
    DMA1_Channel5->CCR |= DMA_CCR_DIR   //DIRection for copying from-memory-to-peripheral.
                        | DMA_CCR_MINC  //MINC to increment the CMAR for every transfer
                        | DMA_CCR_MSIZE_0 // Memory datum SIZE to 16-bi
                        | DMA_CCR_PSIZE_0 //Peripheral datum SIZE to 16-bit
                        | DMA_CCR_CIRC; //channel for CIRCular operation
}

void spi2_enable_dma(void) {
    //enable_dma from lab 5
    DMA1_Channel5->CCR |= DMA_CCR_EN; 
}

void init_spi1() {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER &= 0x3fff33ff;
    GPIOA->MODER |= 0x80008800;
    GPIOA->AFR[0] = 0x00000000;

    //Enable SPI clock
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    //Clear CR1_SPE bit
    SPI1->CR1 &= ~SPI_CR1_SPE;
    //Set baud rate & master mode (/256)
    SPI1->CR1 |=  SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0;
    // Interface for 10-bit word size (1001)
    SPI1->CR2 = SPI_CR2_DS_3 | SPI_CR2_DS_0;
    //master configuration
    SPI1->CR1 |= SPI_CR1_MSTR;
    //SS output, NSSP
    SPI1->CR2 |= SPI_CR2_SSOE | SPI_CR2_NSSP ; 
    //TXDMAEN enable
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
    //Enable SPI
    SPI1->CR1 |= SPI_CR1_SPE;
}
void spi_cmd(unsigned int data) {
    // wait until SPI1 TX is empty
    while(!(SPI1->SR & SPI_SR_TXE));
    // copy data to SPI1 data register
    SPI1->DR = data;
}

void spi_data(unsigned int data) {
    // calls spi_cmd with (data | 0x200)
    spi_cmd(data|0x200);
}

void spi1_init_oled() {
    // wait 1 ms using nano_wait
    nano_wait(1000000);
    // call spi_cmd with 0x38 to do a "function set"
    spi_cmd(0x38);
    // call spi_cmd with 0x08 to turn the display off
    spi_cmd(0x08);
    // call spi_cmd with 0x01 to clear the display
    spi_cmd(0x01);
    // wait 2 ms using nano_wait
    nano_wait(2000000);
    // call spi_cmd with 0x06 to set the entry mode
    spi_cmd(0x06);
    // call spi_cmd with 0x02 to move the cursor to the home position
    spi_cmd(0x02);
    // call spi_cmd with 0x0c to turn the display on
    spi_cmd(0x0c);
}
void spi1_display1(char *string) {
    // move the cursor to the home position (this is the top row)
    spi_cmd(0x02);
    // for each character in the string
    while(*string){
        // call spi_data with the character
        spi_data(*string);
        string++;
    }
}
void spi1_display2(char *string) {
    // move the cursor to the second row (0xc0)
    spi_cmd(0xc0);
    // for each character in the string
    while(*string){
        // call spi_data with the character
        spi_data(*string);
        string++;
    }
}

//===========================================================================
// This is the 34-entry buffer to be copied into SPI1.
// Each element is a 16-bit value that is either character data or a command.
// Element 0 is the command to set the cursor to the first position of line 1.
// The next 16 elements are 16 characters.
// Element 17 is the command to set the cursor to the first position of line 2.
//===========================================================================
uint16_t display[34] = {
        0x002, // Command to set the cursor at the first position line 1
        0x200+'E', 0x200+'C', 0x200+'E', 0x200+'3', 0x200+'6', + 0x200+'2', 0x200+' ', 0x200+'i',
        0x200+'s', 0x200+' ', 0x200+'t', 0x200+'h', + 0x200+'e', 0x200+' ', 0x200+' ', 0x200+' ',
        0x0c0, // Command to set the cursor at the first position line 2
        0x200+'c', 0x200+'l', 0x200+'a', 0x200+'s', 0x200+'s', + 0x200+' ', 0x200+'f', 0x200+'o',
        0x200+'r', 0x200+' ', 0x200+'y', 0x200+'o', + 0x200+'u', 0x200+'!', 0x200+' ', 0x200+' ',
};

//===========================================================================
// Configure the proper DMA channel to be triggered by SPI1_TX.
// Set the SPI1 peripheral to trigger a DMA when the transmitter is empty.
//===========================================================================
void spi1_setup_dma(void) {
    //setup_dma from api_2_setup_dma
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel3->CCR &= ~DMA_CCR_EN; //turn off the enable bit
    DMA1_Channel3->CMAR = (uint32_t) display; //set CMAR to the address of the msg array.
    DMA1_Channel3->CPAR = (uint32_t) (&(SPI1->DR )); //Set CPAR to the address of the GPIOB_ODR register.
    DMA1_Channel3->CNDTR = 34; //Set CNDTR to 8. (the amount of LEDs.)
   
    DMA1_Channel3->CCR |= DMA_CCR_DIR   //DIRection for copying from-memory-to-peripheral.
                        | DMA_CCR_MINC  //MINC to increment the CMAR for every transfer
                        | DMA_CCR_MSIZE_0 // Memory datum SIZE to 16-bi
                        | DMA_CCR_PSIZE_0 //Peripheral datum SIZE to 16-bit
                        | DMA_CCR_CIRC; //channel for CIRCular operation
}

//===========================================================================
// Enable the DMA channel triggered by SPI1_TX.
//===========================================================================
void spi1_enable_dma(void) {
    DMA1_Channel3->CCR |= DMA_CCR_EN; 
}

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
