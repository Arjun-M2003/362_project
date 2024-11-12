#include "plant_monitor.h"

static PlantMonitor_t plant_monitor;

// Helper function to send a single character
// communication from plant monitor and uart
static void UART_SendChar(USART_TypeDef* uart, char c) {
    while (!(uart->ISR & USART_ISR_TXE));  // Wait until TX buffer is empty
    uart->TDR = c;
}

// Helper function to read a single character
// 
static char UART_ReadChar(USART_TypeDef* uart) {
    while (!(uart->ISR & USART_ISR_RXNE));  // Wait until RX buffer is not empty
    return uart->RDR;
}

// helper function to read the number sent back from plant monitor
static float ReadNumber(USART_TypeDef* uart) {
    char buffer[10];
    int idx = 0;
    
    // Wait for '=' character
    while (UART_ReadChar(uart) != '=');
    
    char c;
    while (idx < 9) {
        c = UART_ReadChar(uart);
        if (c >= '0' && c <= '9' || c == '.' || c == '-') {
            buffer[idx++] = c;
        } 
        else {
            break;
        }
    }
    buffer[idx] = '\0';
    
    return atof(buffer);
}

void PlantMonitor_Init(USART_TypeDef* uart) {
    plant_monitor.uart = uart;
    plant_monitor.water_level = 0;
    plant_monitor.temperature = 0.0f;
    plant_monitor.humidity = 0.0f;
}

void PlantMonitor_Update(void) {
    // Read water level
    // dont need water ratings but plant montior gives us these ratings
    //UART_SendChar(plant_monitor.uart, 'w');
    //plant_monitor.water_level = (int)ReadNumber(plant_monitor.uart);
    
    // Read temperature
    UART_SendChar(plant_monitor.uart, 't');
    plant_monitor.temperature = ReadNumber(plant_monitor.uart);
    
    // Read humidity
    UART_SendChar(plant_monitor.uart, 'h');
    plant_monitor.humidity = ReadNumber(plant_monitor.uart);
}

int PlantMonitor_GetWater(void) {
    return plant_monitor.water_level;
}

float PlantMonitor_GetTemperature(void) {
    return plant_monitor.temperature;
}

float PlantMonitor_GetHumidity(void) {
    return plant_monitor.humidity;
}

void PlantMonitor_LedOn(void) {
    UART_SendChar(plant_monitor.uart, 'L');
}

void PlantMonitor_LedOff(void) {
    UART_SendChar(plant_monitor.uart, 'l');
}