/*
 * CD-PA1616S.h
 *
 *  Created on: Feb 21, 2025
 *      Author: Mahir Shah
 */

#ifndef INC_CD_PA1616S_H_
#define INC_CD_PA1616S_H_

#include "defs.h"
#include "sensor.h"

#include <stdint.h>
#include <stdbool.h>

#define BUFFER_SIZE 128

struct gps_ctx {
    uint8_t buffer[BUFFER_SIZE]; // used for DMA reception buffer
    struct handle handle;
};
bool gps_init(struct gps_ctx *ctx, struct sensor *sensor);

#endif /* INC_CD_PA1616S_H_ */
