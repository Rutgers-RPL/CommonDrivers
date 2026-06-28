/*
 * CD-PA1616S.h
 *
 *  Created on: Feb 21, 2025
 *      Author: Mahir Shah
 */

#ifndef INC_CD_PA1616S_H_
#define INC_CD_PA1616S_H_

#define BUFFER_SIZE 128

// Forward declarations
typedef struct UART_HandleTypeDef UART_HandleTypeDef;
typedef struct sensor sensor;

struct gps_context {
    uint8_t buffer[BUFFER_SIZE]; // used for DMA reception buffer
    UART_HandleTypeDef *uart;
};
bool gps_init(gps_context *context, struct sensor *sensor);

#endif /* INC_CD_PA1616S_H_ */
