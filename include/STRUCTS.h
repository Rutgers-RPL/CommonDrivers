/*
 * STRUCTS.h
 *
 *  Created on: Feb 25, 2025
 *      Author: mahirshah
 */

#ifndef INC_STRUCTS_H_
#define INC_STRUCTS_H_


#include <stdint.h> // Standard integer types for STM32

typedef struct {
  int16_t magic;                   // 2 bytes
  uint32_t status;                 // 4 bytes
  uint32_t time_us;                // 4 bytes
  float main_voltage_v;            // 4 bytes
  float pyro_voltage_v;            // 4 bytes
  uint8_t numSatellites;           // 1 byte
  uint8_t gpsFixType;              // 1 byte
  float latitude_degrees;          // 4 bytes
  float longitude_degrees;         // 4 bytes
  float gps_hMSL_m;                // 4 bytes
  float barometer_hMSL_m;          // 4 bytes
  float temperature_c;             // 4 bytes
  float acceleration_x_mss;        // 4 bytes
  float acceleration_y_mss;        // 4 bytes
  float acceleration_z_mss;        // 4 bytes
  float angular_velocity_x_rads;   // 4 bytes
  float angular_velocity_y_rads;   // 4 bytes
  float angular_velocity_z_rads;   // 4 bytes
  float gauss_x;                   // 4 bytes
  float gauss_y;                   // 4 bytes
  float gauss_z;                   // 4 bytes
  float kf_acceleration_mss;       // 4 bytes
  float kf_velocity_ms;            // 4 bytes
  float kf_position_m;             // 4 bytes
  float w;                         // 4 bytes
  float x;                         // 4 bytes
  float y;                         // 4 bytes
  float z;                         // 4 bytes
  uint32_t checksum;               // 4 bytes
} __attribute__((packed)) ganesha_II_packet;




#endif /* INC_STRUCTS_H_ */
