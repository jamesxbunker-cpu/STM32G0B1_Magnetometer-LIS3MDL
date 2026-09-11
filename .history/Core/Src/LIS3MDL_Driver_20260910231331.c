/**
 * @file    LIS3MDL_Driver.c
 * @brief   LIS3MDL 3-axis magnetometer test program.
 * @note    Designed for STM32 HAL. SPI Mode 3 (CPOL=1, CPHA=1).
 * @author  James Bunker
 * @date    2026-09-10
 */

#include "LIS3MDL_Driver.h"



// Read 6 bytes starting from OUT_X_L (0x28) for X, Y, Z axes
uint8_t tx_buffer[7];
uint8_t rx_buffer[7];

// Command byte: Read (0x80) | Auto-increment (0x40) | Address (0x28)
tx_buffer[0] = 0x80 | 0x40 | 0x28;  // 0xE8

// Transmit command + 6 dummy bytes to receive 6 data bytes
HAL_SPI_TransmitReceive(&hspi1, tx_buffer, rx_buffer, 7, 100);

// Data starts at rx_buffer[1]
int16_t mag_x = (int16_t)((rx_buffer[2] << 8) | rx_buffer[1]);  // OUT_X_H, OUT_X_L
int16_t mag_y = (int16_t)((rx_buffer[4] << 8) | rx_buffer[3]);  // OUT_Y_H, OUT_Y_L
int16_t mag_z = (int16_t)((rx_buffer[6] << 8) | rx_buffer[5]);  // OUT_Z_H, OUT_Z_L