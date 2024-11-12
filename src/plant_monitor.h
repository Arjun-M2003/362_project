// stm32_plant_monitor.h
#ifndef PLANT_MONITOR_H
#define PLANT_MONITOR_H

#include "stm32f0xx.h"

// Structure to hold plant monitor data
typedef struct {
    USART_TypeDef* uart;
    int water_level;
    float temperature;
    float humidity;
} PlantMonitor_t;

// Initialize the plant monitor with specified UART
void PlantMonitor_Init(USART_TypeDef* uart);

// Read all sensor values and update the structure
void PlantMonitor_Update(void);

// Get the most recent readings
int PlantMonitor_GetWater(void);
float PlantMonitor_GetTemperature(void);
float PlantMonitor_GetHumidity(void);

// LED control
void PlantMonitor_LedOn(void);
void PlantMonitor_LedOff(void);

#endif