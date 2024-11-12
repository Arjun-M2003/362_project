/**
  ******************************************************************************
  * @file    main.c
  * @author  Weili An, Niraj Menon
  * @date    Feb 3, 2024
  * @brief   ECE 362 Lab 6 Student template
  ******************************************************************************
*/

/*******************************************************************************/

// Fill out your username, otherwise your completion code will have the 
// wrong username!
const char* username = "arejman";

/*******************************************************************************/ 

#include "stm32f0xx.h"

void set_char_msg(int, char);
void nano_wait(unsigned int);
void game(void);
void internal_clock();
void check_wiring();
void autotest();

//===========================================================================
// Configure GPIOC
//===========================================================================
void enable_ports(void) {
    // Only enable port C for the keypad
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~0xffff;
    GPIOC->MODER |= 0x55 << (4*2);
    GPIOC->OTYPER &= ~0xff;
    GPIOC->OTYPER |= 0xf0;
    GPIOC->PUPDR &= ~0xff;
    GPIOC->PUPDR |= 0x55;
}


uint8_t col; // the column being scanned

void drive_column(int);   // energize one of the column outputs
int  read_rows();         // read the four row inputs
void update_history(int col, int rows); // record the buttons of the driven column
char get_key_event(void); // wait for a button event (press or release)
char get_keypress(void);  // wait for only a button press event.
float getfloat(void);     // read a floating-point number from keypad
void show_keys(void);     // demonstrate get_key_event()

//===========================================================================
// Bit Bang SPI LED Array
//===========================================================================
int msg_index = 0;
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
extern const char font[];

//===========================================================================
// Configure PB12 (CS), PB13 (SCK), and PB15 (SDI) for outputs
//===========================================================================
void setup_bb(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    
    GPIOB->MODER &= ~((3UL << (12 * 2)) | (3UL << (13 * 2)) | (3UL << (15 * 2)));  
    GPIOB->MODER |= ((1UL << (12 * 2)) | (1UL << (13 * 2)) | (1UL << (15 * 2)));  

    // Set PB12 (CS/NSS) high, PB13 (SCK) low
    GPIOB->ODR |= (1UL << 12);   
    GPIOB->ODR &= ~(1UL << 13); 
}
void small_delay(void) {
    nano_wait(5000000);
}

//===========================================================================
// Set the MOSI bit, then set the clock high and low.
// Pause between doing these steps with small_delay().
//===========================================================================
void bb_write_bit(int val) {
    if (val) {
        GPIOB->ODR |= (1UL << 15);  
    } else {
        GPIOB->ODR &= ~(1UL << 15); 
    }
    
    small_delay();  

    // Set SCK (PB13) high
    GPIOB->ODR |= (1UL << 13);
    small_delay();  

    // Set SCK (PB13) low
    GPIOB->ODR &= ~(1UL << 13);
    
}

//===========================================================================
// Set CS (PB12) low,
// write 16 bits using bb_write_bit,
// then set CS high.
//===========================================================================
void bb_write_halfword(int halfword) {
    GPIOB->ODR &= ~(1UL << 12);
    
    // Send bits 15 to 0 of the message
    for (int i = 15; i >= 0; i--) {
        bb_write_bit((halfword >> i) & 1);  
    }

    // Set CS (PB12) back to 1 (inactive high)
    GPIOB->ODR |= (1UL << 12);
}

//===========================================================================
// Continually bitbang the msg[] array.
//===========================================================================
void drive_bb(void) {
    for(;;)
        for(int d=0; d<8; d++) {
            bb_write_halfword(msg[d]);
            nano_wait(1000000); 
        }
}

//============================================================================
// Configure Timer 15 for an update rate of 1 kHz.
// Trigger the DMA channel on each update.
// Copy this from lab 4 or lab 5.
//============================================================================
void init_tim15(void) {
    // Enable the RCC clock for TIM15
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
    
    // Set the prescaler and auto-reload to achieve a 1 kHz rate
    // Assuming a clock frequency of 48 MHz for the timer
    TIM15->PSC = 48 - 1;  // Prescaler value: 48 MHz / 48 = 1 MHz
    TIM15->ARR = 1000 - 1;  // Auto-reload value: 1 MHz / 1000 = 1 kHz
    
    // Configure the DIER register to enable DMA requests (UDE bit), without enabling interrupts (UIE)
    TIM15->DIER |= TIM_DIER_UDE; 
    TIM15->DIER &= ~TIM_DIER_UIE;  

    TIM15->CR1 |= TIM_CR1_CEN;
}

//===========================================================================
// Configure timer 7 to invoke the update interrupt at 1kHz
// Copy from lab 4 or 5.
//===========================================================================
void init_tim7(void) {
    // Enable the RCC clock for TIM7
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;

    // Set the prescaler and auto-reload to achieve a 1 kHz rate
    // Assuming a clock frequency of 48 MHz for the timer
    TIM7->PSC = 48 - 1;  
    TIM7->ARR = 1000 - 1;  

    // Enable the update interrupt (UIE) to trigger ISR
    TIM7->DIER |= TIM_DIER_UIE;

    // Enable the timer
    TIM7->CR1 |= TIM_CR1_CEN;

    // Enable the TIM7 interrupt in the NVIC
    NVIC_EnableIRQ(TIM7_IRQn);

}

//===========================================================================
// Copy the Timer 7 ISR from lab 5
//===========================================================================
// TODO To be copied
void TIM7_IRQHandler(void) {
    // Acknowledge the interrupt by clearing the update interrupt flag
    TIM7->SR &= ~TIM_SR_UIF;

    // Read the current state of the rows (active-low signals)
    int rows = read_rows();
    
    // Update the button history
    update_history(col, rows);
    
    // Update the column, rotating through 0-3 (for 4 columns)
    col = (col + 1) & 3;
    
    // Drive the next column
    drive_column(col);
}

//===========================================================================
// Initialize the SPI2 peripheral.
//===========================================================================
void init_spi2(void) {
    RCC -> AHBENR |= RCC_AHBENR_GPIOBEN;

    GPIOB -> MODER &= ~0xcf000000;
    GPIOB -> MODER |=  0x8a000000;

    GPIOB -> AFR[1] &= ~0xf0ff << (4 * (12 - 8));
    GPIOB -> AFR[1] |=  0x0000 << (4 * (12 - 8));


    RCC -> APB1ENR |= RCC_APB1ENR_SPI2EN;

    SPI2 -> CR1 &= ~SPI_CR1_SPE;                     
    SPI2 -> CR1 |=  SPI_CR1_MSTR | SPI_CR1_BR;       
    SPI2 -> CR2 =   SPI_CR2_DS_3 | SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0; 
    SPI2 -> CR2 |=  SPI_CR2_SSOE | SPI_CR2_NSSP;     
    SPI2 -> CR2 |=  SPI_CR2_TXDMAEN;                
    SPI2 -> CR1 |=  SPI_CR1_SPE;                     
}

//===========================================================================
// Configure the SPI2 peripheral to trigger the DMA channel when the
// transmitter is empty.  Use the code from setup_dma from lab 5.
//===========================================================================
void spi2_setup_dma(void) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    
    // Turn off the DMA channel before configuring it (TIM15 uses DMA1 Channel 5)
    DMA1_Channel5->CCR &= ~DMA_CCR_EN;
    
    // Set the memory address (CMAR) to the address of the msg array
    DMA1_Channel5->CMAR = (uint32_t)msg;  
    
    // Set the peripheral address (CPAR) to the address of GPIOB->ODR
    DMA1_Channel5->CPAR = (uint32_t)&SPI2->DR; 
    
    // Set the number of data to transfer (CNDTR) to 8
    DMA1_Channel5->CNDTR = 8;
    
    // Configure the DMA channel
    // Memory to Peripheral direction
    DMA1_Channel5->CCR &= ~DMA_CCR_DIR;  
    DMA1_Channel5->CCR |= DMA_CCR_DIR;   
    
    // Enable memory increment (MINC)
    DMA1_Channel5->CCR |= DMA_CCR_MINC;
    
    // Set memory size (MSIZE) to 16-bit
    DMA1_Channel5->CCR &= ~DMA_CCR_MSIZE;
    DMA1_Channel5->CCR |= DMA_CCR_MSIZE_0; 
    
    // Set peripheral size (PSIZE) to 16-bit
    DMA1_Channel5->CCR &= ~DMA_CCR_PSIZE;
    DMA1_Channel5->CCR |= DMA_CCR_PSIZE_0;  
    
    // Enable circular mode
    DMA1_Channel5->CCR |= DMA_CCR_CIRC;
    
    // Enable DMA on TIM15 update event
    TIM15->DIER |= TIM_DIER_UDE;

    SPI2->CR2 |= SPI_CR2_TXDMAEN;
}

//===========================================================================
// Enable the DMA channel.
//===========================================================================
void spi2_enable_dma(void) {
    DMA1_Channel5->CCR |= DMA_CCR_EN;
}

//===========================================================================
// 4.4 SPI OLED Display
//===========================================================================
void init_spi1() {
    RCC -> AHBENR |= RCC_AHBENR_GPIOAEN;

    GPIOA -> MODER &= ~0xc000fc00;
    GPIOA -> MODER |=  0x8000a800;

    GPIOA -> AFR[0] &= ~0xfff << (4 * 5);
    GPIOA -> AFR[0] |=  0x000 << (4 * 5);
    GPIOA -> AFR[1] &= ~0xf << (4 * (15 - 8));
    GPIOA -> AFR[1] |=  0x0 << (4 * (15 - 8));

    RCC -> APB2ENR |= RCC_APB2ENR_SPI1EN;

    SPI1 -> CR1 &= ~SPI_CR1_SPE;                    
    SPI1 -> CR1 |=  SPI_CR1_MSTR | SPI_CR1_BR;       
    SPI1 -> CR2 =   SPI_CR2_DS_3 | SPI_CR2_DS_0; 
    SPI1 -> CR2 |=  SPI_CR2_SSOE | SPI_CR2_NSSP;     
    SPI1 -> CR2 |=  SPI_CR2_TXDMAEN;                
    SPI1 -> CR1 |=  SPI_CR1_SPE;                    
}
void spi_cmd(unsigned int data) {
    while(!(SPI1->SR & SPI_SR_TXE)) {

    }
    SPI1->DR = data;

}
void spi_data(unsigned int data) {
    spi_cmd(data | 0x200);
}
void spi1_init_oled() {
    nano_wait(1000000);

    spi_cmd(0x38);
    spi_cmd(0x08);
    spi_cmd(0x01);

    nano_wait(2000000);

    spi_cmd(0x06);
    spi_cmd(0x02);
    spi_cmd(0x0c);
}
void spi1_display1(const char *string) {
    spi_cmd(0x02);

    while(*string != '\0') {
        spi_data(*string);
        string++;
    }
}
void spi1_display2(const char *string) {
    spi_cmd(0xc0);

    while(*string != '\0') {
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
    RCC -> AHBENR |= RCC_AHBENR_DMA1EN;

    DMA1_Channel3 -> CCR &= ~DMA_CCR_EN;
    DMA1_Channel3 -> CMAR = (uint32_t) display;
    DMA1_Channel3 -> CPAR = &(SPI1 -> DR);
    DMA1_Channel3 -> CNDTR = 34;

    DMA1_Channel3 -> CCR |= DMA_CCR_DIR;
    DMA1_Channel3 -> CCR &= ~DMA_CCR_MSIZE;
    DMA1_Channel3 -> CCR |= DMA_CCR_MSIZE_0;
    DMA1_Channel3 -> CCR &= ~DMA_CCR_PSIZE;
    DMA1_Channel3 -> CCR |= DMA_CCR_PSIZE_0;
    DMA1_Channel3 -> CCR |= DMA_CCR_MINC;
    DMA1_Channel3 -> CCR |= DMA_CCR_CIRC;

    SPI1 -> CR2 |= SPI_CR2_TXDMAEN;
}

//===========================================================================
// Enable the DMA channel triggered by SPI1_TX.
//===========================================================================
void spi1_enable_dma(void) {
    DMA1_Channel3 ->CCR |= DMA_CCR_EN;
}

//===========================================================================
// Main function
//===========================================================================

int main(void) {
    internal_clock();

    msg[0] |= font['E'];
    msg[1] |= font['C'];
    msg[2] |= font['E'];
    msg[3] |= font[' '];
    msg[4] |= font['3'];
    msg[5] |= font['6'];
    msg[6] |= font['2'];
    msg[7] |= font[' '];

    // GPIO enable
    enable_ports();
    // setup keyboard
    init_tim7();
    init_spi1();
    spi1_init_oled();

    spi1_display1("Temperature: 70F");
    //spi1_display1(temp);
    spi1_display2("Moisture: 0.8%   ");
    //spi1_display2(moist);
    
    while (1) {
        char key = get_keypress();
        int sTemp = 66;
        int eTemp = 76;
        int sMoist = 0;
        int eMoist = 2;
        int temp = 72;
        int moist = 1;

        if (key == 'A') {
            // When 'A' is pressed, display "Enter" on the OLED
            spi1_display1("Enter Start of   ");
            spi1_display2("Range for Temp:   ");
            char sTemp1 = get_keypress();
            small_delay();
            char sTemp2 = get_keypress();
            small_delay();
            spi1_display1("Enter End of   ");
            spi1_display2("Range for Temp:   ");
            char eTemp1 = get_keypress();
            small_delay();
            char eTemp2 = get_keypress();
            small_delay();
            spi1_display1("Temperature: 70F");
            //spi1_display1(temp);
            spi1_display2("Moisture: 0.8%     ");
            //spi1_display2(moist);
        }
        if (key == 'B') {
            // When 'A' is pressed, display "Enter" on the OLED
            spi1_display1("Enter Start of   ");
            spi1_display2("Range for Moist:   ");
            char sMoist1 = get_keypress();
            small_delay();
            char sMoist2 = get_keypress();
            small_delay();
            // Now wait for the '#' key to be pressed to display "End"
            
            spi1_display1("Enter End of   ");
            spi1_display2("Range for Moist:   ");
            char eMoist1 = get_keypress();
            small_delay();
            char eMoist2 = get_keypress();
            small_delay();
            spi1_display1("Temperature: 70F");
            //spi1_display1(temp);
            spi1_display2("Moisture: 0.8%   ");
            //spi1_display2(moist);
        }
        if (temp < sTemp){
            spi1_display1("      ERROR       ");
            spi1_display2("  Temp Too Low!   ");
        }
        if (temp > eTemp){
            spi1_display1("      ERROR       ");
            spi1_display2("  Temp Too High!   ");
        }
        if (moist < sMoist){
            spi1_display1("      ERROR       ");
            spi1_display2("Moisture Too Low!   ");
        }
        if (moist > eMoist){
            spi1_display1("      ERROR       ");
            spi1_display2("Moisture Too High!   ");
        }
        
    }
}


