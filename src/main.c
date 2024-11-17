#include "stm32f0xx.h"
// #include <stdio.h>

void set_char_msg(int, char);
void nano_wait(unsigned int);
void internal_clock();

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

//===========================================================================
// Set the MOSI bit, then set the clock high and low.
// Pause between doing these steps with small_delay().
//===========================================================================
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

//===========================================================================
// Set CS (PB12) low,
// write 16 bits using bb_write_bit,
// then set CS high.
//===========================================================================
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

//===========================================================================
// Continually bitbang the msg[] array.
//===========================================================================
void drive_bb(void) {
    for(;;)
        for(int d=0; d<8; d++) {
            bb_write_halfword(msg[d]);
            nano_wait(1000000); // wait 1 ms between digits
        }
}

//============================================================================
// Configure Timer 15 for an update rate of 1 kHz.
// Trigger the DMA channel on each update.
// Copy this from lab 4 or lab 5.
//============================================================================
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


//===========================================================================
// Configure timer 7 to invoke the update interrupt at 1kHz
// Copy from lab 4 or 5.
//===========================================================================
void init_tim7(void) {
    //Enable RCC clock for TIM7
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
    
    // Set PSC and ARR for 1kHz Result
    TIM7->PSC = 480 - 1;
    TIM7->ARR = 100 - 1;

    //Enable UIE
    TIM7->DIER |= TIM_DIER_UIE;

    //Enable interupt for Timer 7
    NVIC->ISER[0] = 1 << TIM7_IRQn;

    //Enable timer 7
    TIM7->CR1 |= TIM_CR1_CEN;
}

//===========================================================================
// Copy the Timer 7 ISR from lab 5
//===========================================================================
// TODO To be copied
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

//===========================================================================
// Initialize the SPI2 peripheral.
//===========================================================================
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

//===========================================================================
// Configure the SPI2 peripheral to trigger the DMA channel when the
// transmitter is empty.  Use the code from setup_dma from lab 5.
//===========================================================================
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

//===========================================================================
// Enable the DMA channel.
//===========================================================================
void spi2_enable_dma(void) {
    //enable_dma from lab 5
    DMA1_Channel5->CCR |= DMA_CCR_EN; 
}

//===========================================================================
// 4.4 SPI OLED Display
//===========================================================================
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
void spi1_display1(const char *string) {
    // move the cursor to the home position (this is the top row)
    spi_cmd(0x02);
    // for each character in the string
    while(*string){
        // call spi_data with the character
        spi_data(*string);
        string++;
    }
}
void spi1_display2(const char *string) {
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

//===========================================================================
// Main function
//===========================================================================

int main(void) {
    internal_clock();
    enable_ports();

    // GPIO enable
    enable_ports();
    // setup keyboard
    init_tim7();


    NVIC->ICER[0] = 1<<TIM17_IRQn;
    print("temp 80F");
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    //get data & add string "temperature ", temp, "F"
    spi1_dma_display1("temperature 77F ");
    //get data & add string "  humidity  ", humid, "%"
    spi1_dma_display2("  humidity  26% ");
    //Oled line space "                "
    
    init_spi1();
    spi1_init_oled();

    int sTemp = 66;
    int eTemp = 76;
    int sMoist = 0;
    int eMoist = 20;
    int temp = 72;
    int moist = 10;
    char tempB[32];
    char moistB[32];
    sprintf(tempB, "Temperature:%3dF", temp);
    spi1_display1(tempB);
    sprintf(moistB, "   Moisture:%3d%%", moist);
    spi1_display2(moistB);
    while (1) {
        char key = get_keypress();

        if (key == 'A') {
            // When 'A' is pressed, display "Enter" on the OLED
            sTemp = 150;
            while(sTemp > 140){
                spi1_display1("Enter Start of   ");
                spi1_display2("Range for Temp:   ");
                small_delay();
                sTemp = getint();
            }
            eTemp = 150;
            while(eTemp >140){
                spi1_display1("Enter End of   ");
                spi1_display2("Range for Temp:   ");
                small_delay();
                eTemp = getint();
            }
        }
        if (key == 'B') {
            // When 'B' is pressed, display "Enter" on the OLED
            spi1_display1("Enter Start of   ");
            spi1_display2("Range for Moist:   ");
            sMoist = getint();
            small_delay();
            // Now wait for the '#' key to be pressed to display "End"
            spi1_display1("Enter End of   ");
            spi1_display2("Range for Moist:   ");
            eMoist = getint();
            small_delay();
        }
        if (key == 'C') { //Display status
            sprintf(tempB, "Temperature:%3dF", temp);
            spi1_display1(tempB);
            sprintf(moistB, "   Moisture:%3d%%", moist);
            spi1_display2(moistB);
        }
        else if (temp < sTemp){
            spi1_display1("      ERROR       ");
            spi1_display2("  Temp Too Low!   ");
        }
        else if (temp > eTemp){
            spi1_display1("      ERROR       ");
            spi1_display2("  Temp Too High!   ");
        }
        else if (moist < sMoist){
            spi1_display1("      ERROR       ");
            spi1_display2("Moisture Too Low!   ");
        }
        else if (moist > eMoist){
            spi1_display1("      ERROR       ");
            spi1_display2("Moisture Too High!   ");
        } else {
            sprintf(tempB, "Temperature:%3dF", temp);
            spi1_display1(tempB);
            sprintf(moistB, "   Moisture:%3d%%", moist);
            spi1_display2(moistB);
        }
        
    }
}
