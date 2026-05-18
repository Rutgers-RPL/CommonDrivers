/*
 * CD-PA1616S.h
 *
 *  Created on: Feb 21, 2025
 *      Author: Mahir Shah
 */

#ifndef INC_CD_PA1616S_H_
#define INC_CD_PA1616S_H_

#include "stm32h7xx_hal.h"

#define BUFFER_SIZE 128

// Data struct for storing GPS info
typedef struct __attribute__((packed)) {
    float latitude_degrees;
    float longitude_degrees;
    float gps_hMSL_m;       // altitude above mean sea level
    uint8_t numSatellites;
    uint8_t gpsFixType;     // 0 = no fix, 1 = fix, 2 = DGPS fix, etc.
} gps_data_packet_t;

// Global GPS data (updated by DMA callback)
extern gps_data_packet_t gps_packet;
// DMA buffer
extern uint8_t gps_dma_buffer[BUFFER_SIZE];

// Initializes GPS DMA
void GPS_Init(void);

// DMA & Error Callbacks
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t offset);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

// Single function to parse GGA data from a buffer
int ParseGPSData(const char *buffer, gps_data_packet_t *gps_data);

#endif /* INC_CD_PA1616S_H_ */
