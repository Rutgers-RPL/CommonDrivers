/*
 * CD-PA1616S.c
 *
 *  Created on: Feb 21, 2025
 *      Author: Mahir Shah
 */

#include "CD-PA1616S.h"
#include "sensor.h"

#include "stm32xxxx_hal.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

bool gps_init(gps_context *context, struct sensor *sensor)
{
    assert(context->uart != NULL);

    context->buffer = {0};
    sensor->context = context;
    sensor->read = gps_read;

    // Initialize GPS DMA Reception
    char command[] = "$PMTK314,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\x0d\x0a";
    HAL_UART_Transmit(uart, (uint8_t*) command, sizeof(command) - 1, HAL_MAX_DELAY);
    HAL_UARTEx_ReceiveToIdle_DMA(uart, context->buffer, BUFFER_SIZE);

    return true;
}

//  ParseGPSData: Single function to find and parse $GNGGA / $GPGGA
//    Returns 1 if successful, 0 otherwise
bool gps_read(struct sensor_context *context, struct packet *packet)
{
    struct gps_context context = (gps_context*) context;
    char buffer = context->buffer;

    // Search manually for either "$GNGGA" or "$GPGGA" in buffer
    const char *gga_start = NULL;
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == '$') {
            // Check if we match "$GNGGA" or "$GPGGA"
            if (strncmp(&buffer[i], "$GNGGA", 6) == 0 ||
                strncmp(&buffer[i], "$GPGGA", 6) == 0)
                {
                    gga_start = &buffer[i];
                    break;
                }
        }
    }

    // If not found, return failure
    if (!gga_start) {
        return false;
    }

    //  Copy one line (until CR or LF) into a local buffer
    char line[120];
    int idx = 0;
    while (gga_start[idx] != '\0' &&
           gga_start[idx] != '\r' &&
           gga_start[idx] != '\n' &&
           idx < (int)sizeof(line) - 1)
        {
            line[idx] = gga_start[idx];
            idx++;
        }
    line[idx] = '\0';

    // Split into fields by commas. We'll store them in fields[0..].
    char fields[20][20];
    memset(fields, 0, sizeof(fields));
    int fieldIndex = 0;
    int charIndex = 0;

    for (int j = 0; j < idx; j++) {
        if (line[j] == ',') {
            fields[fieldIndex][charIndex] = '\0';  // end current field
            fieldIndex++;
            charIndex = 0;
            if (fieldIndex >= 20) break;
        } else {
            if (charIndex < 19) {
                fields[fieldIndex][charIndex++] = line[j];
            }
        }
    }
    // Terminate the last field
    if (fieldIndex < 20) {
        fields[fieldIndex][charIndex] = '\0';
    }

    // Typical GGA format (field indices):
    //   0: GPGGA
    //   1: UTC time
    //   2: latitude
    //   3: N/S
    //   4: longitude
    //   5: E/W
    //   6: Fix Quality
    //   7: # of Satellites
    //   8: HDOP
    //   9: Altitude (M)
    //  10: Altitude units
    //  11: Geoid Separation
    //  12: Geoid units
    //  13: DGPS Age
    //  14: DGPS Station ID
    //  15: Checksum

    if (fieldIndex < 9) {
        // Not enough fields for a valid GGA
        return false;
    }

    // Inline conversion to decimal degrees (latitude)
    float lat = 0.0f;
    if (strlen(fields[2]) >= 4) {
        // For lat, first 2 digits are degrees, rest are minutes
        char deg_str[3] = {0};
        strncpy(deg_str, fields[2], 2);
        float degrees = atof(deg_str);
        float minutes = atof(fields[2] + 2);
        lat = degrees + (minutes / 60.0f);
        // South => negative
        if (fields[3][0] == 'S') {
            lat = -lat;
        }
    }

    // Inline conversion to decimal degrees (longitude)
    float lon = 0.0f;
    if (strlen(fields[4]) >= 4) {
        // For lon, first 3 digits are degrees, rest are minutes
        char deg_str[4] = {0};
        strncpy(deg_str, fields[4], 3);
        float degrees = atof(deg_str);
        float minutes = atof(fields[4] + 3);
        lon = degrees + (minutes / 60.0f);
        // West => negative
        if (fields[5][0] == 'W') {
            lon = -lon;
        }
    }

    // Fix quality, # of satellites, altitude
    uint8_t fix = (uint8_t)atoi(fields[6]);
    uint8_t sats = (uint8_t)atoi(fields[7]);
    float alt = (fields[9][0] != '\0') ? atof(fields[9]) : 0.0f;

    packet->latitude_degrees = lat;
    packet->longitude_degrees = lon;
    packet->gpsFixType = fix;
    packet->numSatellites = sats;
    packet->gps_hMSL_m = alt;
    return true;
}
