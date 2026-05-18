/*
 * CD-PA1616S.c
 *
 *  Created on: Feb 21, 2025
 *      Author: Mahir Shah
 */

#include "CD-PA1616S.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Global variables
gps_data_packet_t gps_packet;          // Parsed GPS data
uint8_t gps_dma_buffer[BUFFER_SIZE];   // DMA reception buffer

extern UART_HandleTypeDef huart8;      // GPS UART handle


// Initialize GPS DMA Reception
void GPS_Init(void) {

	char command[] = "$PMTK314,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\x0d\x0a";
	HAL_UART_Transmit(&huart8, (uint8_t*)command, sizeof(command) - 1, HAL_MAX_DELAY);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart8, gps_dma_buffer, BUFFER_SIZE);
}


// DMA Callback: Called when GPS Buffer is filled or there is a gap in data transfer between GPS and the Pin
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t offset) {
    if (huart->Instance == UART8) {
        // Clear any Overrun error
        __HAL_UART_CLEAR_OREFLAG(&huart8);

        // Parse the entire DMA buffer (if a GGA sentence is found, store into gps_packet)
        ParseGPSData((char *)gps_dma_buffer, &gps_packet);

        // Restart DMA reception
        HAL_UARTEx_ReceiveToIdle_DMA(&huart8, gps_dma_buffer, BUFFER_SIZE);
    }
}


// UART Error Callback in case error occurs in DMA call back
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == UART8) {
        // Clear Overrun
        __HAL_UART_CLEAR_OREFLAG(&huart8);
        // Restart DMA
        HAL_UARTEx_ReceiveToIdle_DMA(&huart8, gps_dma_buffer, BUFFER_SIZE);
    }
}

//  ParseGPSData: Single function to find and parse $GNGGA / $GPGGA
//    Returns 1 if successful, 0 otherwise
int ParseGPSData(const char *buffer, gps_data_packet_t *gps_data)
{
    //Search manually for either "$GNGGA" or "$GPGGA" in buffer
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
        return 0;
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
        return 0;
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

    // Store in gps_data struct
    gps_data->latitude_degrees = lat;
    gps_data->longitude_degrees = lon;
    gps_data->gpsFixType = fix;
    gps_data->numSatellites = sats;
    gps_data->gps_hMSL_m = alt;

    return 1;  // Success
}
