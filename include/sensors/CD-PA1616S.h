/*
 * CD-PA1616S.h
 *
 *  Created on: Feb 21, 2025
 *      Author: Mahir Shah
 */

#ifndef INC_CD_PA1616S_H_
#define INC_CD_PA1616S_H_

#define BUFFER_SIZE 128

// Forward declaration
typedef struct UART_HandleTypeDef UART_HandleTypeDef;

// Data struct for storing GPS info
typedef struct __attribute__((packed)) {
    float latitude_degrees;
    float longitude_degrees;
    float gps_hMSL_m;       // altitude above mean sea level
    uint8_t numSatellites;
    uint8_t gpsFixType;     // 0 = no fix, 1 = fix, 2 = DGPS fix, etc.
} gps_data_packet_t;

struct gps_dev {
    gps_data_packet_t packet;        // Parsed GPS data
    uint8_t dma_buffer[BUFFER_SIZE]; // DMA reception buffer
    UART_HandleTypeDef *uart;        // GPS UART handle
}

// Initializes GPS DMA
void gps_init(struct gps_dev *gps, UART_HandleTypeDef *uart);
// Single function to parse GGA data from a buffer
int gps_parse(struct gps_dev *gps);

#endif /* INC_CD_PA1616S_H_ */
