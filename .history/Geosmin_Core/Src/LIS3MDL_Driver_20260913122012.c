/**
 * @file    LIS3MDL_Driver.c
 * @brief   LIS3MDL 3-axis magnetometer driver implementation (4-wire SPI).
 * @note    Designed for STM32 HAL. SPI Mode 3 (CPOL=1, CPHA=1).
 * @author  James Bunker
 * @date    2026-09-10
 */

#include "LIS3MDL_Driver.h"

/* Temp will need to redefine ODR definitions or enum mapping 
 * not a big fan of how I am currently doing it*/
 #define LIS3MDL_SPI_TIMEOUT 100  // Timeout for SPI transactions (ms)
/* --- SPI framing: bit0 = R/W, bit1 = MS (auto-increment), bits2..7 = AD(5:0) --- */
#define LIS3MDL_SPI_READ_BIT    0x01U
#define LIS3MDL_SPI_AUTO_INC    0x02U
#define LIS3MDL_SPI_ADDR_SHIFT  2U
#define LIS3MDL_SPI_ADDR_MASK   0xFCU

/* =========================================================================
 * Private Helpers
 * ========================================================================= */
/**
 * @brief  Convert a full-scale enum to its sensitivity (LSB/gauss).
 */
static float LIS3MDL_FS_ToSensitivity(LIS3MDL_FullScale_t fs)
{
    switch (fs) {
        case LIS3MDL_FS_4G:  
            return LIS3MDL_SENS_4G;
        case LIS3MDL_FS_8G:  
            return LIS3MDL_SENS_8G;
        case LIS3MDL_FS_12G: 
            return LIS3MDL_SENS_12G;
        case LIS3MDL_FS_16G: 
            return LIS3MDL_SENS_16G;
        default:             
            return LIS3MDL_SENS_4G;
    }
}

/**
 * @brief  Convert a full-scale enum to its CTRL_REG2 FS bits (already shifted).
 */
static uint8_t LIS3MDL_FS_ToRegBits(LIS3MDL_FullScale_t fs)
{
    switch (fs) {
        case LIS3MDL_FS_4G:  
            return LIS3MDL_CTRL2_FS_4G;
        case LIS3MDL_FS_8G:  
            return LIS3MDL_CTRL2_FS_8G;
        case LIS3MDL_FS_12G: 
            return LIS3MDL_CTRL2_FS_12G;
        case LIS3MDL_FS_16G: 
            return LIS3MDL_CTRL2_FS_16G;
        default:             
            return LIS3MDL_CTRL2_FS_4G;
    }
}

LIS3MDL_Status_t LIS3MDL_CS_High(LIS3MDL_Handle_t *dev){
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
    
    if(HAL_GPIO_ReadPin(dev->cs_port, dev->cs_pin) != GPIO_PIN_SET){
        return LIS3MDL_ERR;
    }

    return LIS3MDL_OK;
}

LIS3MDL_Status_t LIS3MDL_CS_Low(LIS3MDL_Handle_t *dev){
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);

    if(HAL_GPIO_ReadPin(dev->cs_port, dev->cs_pin) != GPIO_PIN_RESET){
        return LIS3MDL_ERR;
    }

    return LIS3MDL_OK;
}

/* =========================================================================
 * Initialization
 * ========================================================================= */
/**
 * @brief  Initialize the LIS3MDL over SPI.
 * @param  dev       Pointer to driver handle (fill hspi, cs_port, cs_pin first).
 * @param  fs        Full-scale range.
 * @retval LIS3MDL_OK on success, error code otherwise.
 */
LIS3MDL_Status_t LIS3MDL_Init(LIS3MDL_Handle_t *dev, LIS3MDL_FullScale_t fs) {
    if (dev == NULL || dev->hspi == NULL || dev->cs_port == NULL) {
        return LIS3MDL_ERR;
    }

    /* Ensure CS is idle high */
    LIS3MDL_Status_t status = LIS3MDL_CS_High(dev);
    if (status != LIS3MDL_OK) {
        return status;
    }

    /* Small power-up delay (datasheet recommends ~10 ms) */
    HAL_Delay(10);

    /* Verify device identity */
    status = LIS3MDL_CheckID(dev);
    if (status != LIS3MDL_OK) {
        return status;
    }

    /* Software reset (CTRL_REG2: SOFT_RST = 1, REBOOT = 1) */
    status = LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG2,
                              LIS3MDL_CTRL2_SOFT_RST | LIS3MDL_CTRL2_REBOOT);
    if (status != LIS3MDL_OK) {
        return status;
    }
    HAL_Delay(10);

    /* CTRL_REG1: temperature enable, high-power mode, Fast ODR, ODR = 560 Hz */
    status = LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG1,
                              LIS3MDL_CTRL1_TEMP_EN |
                              LIS3MDL_CTRL1_OM_MP |
                              LIS3MDL_CTRL1_FAST_ODR);
    if (status != LIS3MDL_OK) {
        return status;
    }

    /* CTRL_REG2: full-scale (no reboot/reset now) */
    status = LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG2,
                              LIS3MDL_FS_ToRegBits(fs));
    if (status != LIS3MDL_OK) {
        return status;
    }

    /* CTRL_REG3: continuous-conversion mode, 4-wire SPI */
    status = LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG3,
                              LIS3MDL_CTRL3_MD_CONT);
    if (status != LIS3MDL_OK) {
        return status;
    }

    /* CTRL_REG4: Z-axis ultra-high-power mode, little-endian */
    status = LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG4,
                              LIS3MDL_CTRL4_OMZ_MP);
    if (status != LIS3MDL_OK) {
        return status;
    }

    /* CTRL_REG5: block data update enabled */
    status = LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG5,
                              LIS3MDL_CTRL5_BDU);
    if (status != LIS3MDL_OK) {
        return status;
    }

    /* Cache configuration */
    dev->fs          = fs;
    dev->sensitivity = LIS3MDL_FS_ToSensitivity(fs);

    return LIS3MDL_OK;
}

/**
 * @brief  Verify device ID (WHO_AM_I == 0x3D).
 */
LIS3MDL_Status_t LIS3MDL_CheckID(LIS3MDL_Handle_t *dev){
    uint8_t id = 0;
    LIS3MDL_Status_t status = LIS3MDL_ReadReg(dev, LIS3MDL_WHO_AM_I, &id);
    if (status != LIS3MDL_OK) {
        return status;
    }

    if (id != LIS3MDL_WHO_AM_I_VALUE) {
        return LIS3MDL_ERR;
    }

    return LIS3MDL_OK;
}

/* =========================================================================
 * Register Access
 * ========================================================================= */
/**
 * @brief  Write a single register.
 * @note   SPI write: bit0 = 0 (write), bit1 = 0 (auto-increment).
 */
LIS3MDL_Status_t LIS3MDL_WriteReg(LIS3MDL_Handle_t *dev, uint8_t reg, uint8_t value){
    if (dev == NULL || dev->hspi == NULL || dev->cs_port == NULL) {
        return LIS3MDL_ERR;
    }

    uint8_t tx[2] = {
        (uint8_t)((reg << LIS3MDL_SPI_ADDR_SHIFT) & LIS3MDL_SPI_ADDR_MASK), /* R/W=0, MS=0 */
        value
    };
    uint8_t rx[2] = { 0 };

    LIS3MDL_Status_t status = LIS3MDL_CS_Low(dev);
    if (status != LIS3MDL_OK) {
        return status;
    }

    if (HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 2, LIS3MDL_SPI_TIMEOUT) != HAL_OK) {
        LIS3MDL_CS_High(dev);
        return LIS3MDL_ERR;
    }

    return LIS3MDL_CS_High(dev);
}

/**
 * @brief  Read a single register.
 * @note   SPI write: bit0 = 1 (write), bit1 = 0 (auto-increment).
 */
LIS3MDL_Status_t LIS3MDL_ReadReg(LIS3MDL_Handle_t *dev, uint8_t reg, uint8_t *value){
    if (dev == NULL || dev->hspi == NULL || dev->cs_port == NULL || value == NULL) {
        return LIS3MDL_ERR;
    }

    uint8_t tx[2] = {
        (uint8_t)(((reg << LIS3MDL_SPI_ADDR_SHIFT) & LIS3MDL_SPI_ADDR_MASK)
                  | LIS3MDL_SPI_READ_BIT),   /* R/W=1, MS=0 */
        0x00
    };
    uint8_t rx[2] = { 0 };

    LIS3MDL_Status_t status = LIS3MDL_CS_Low(dev);
    if (status != LIS3MDL_OK) {
        return status;
    }

    if (HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 2, LIS3MDL_SPI_TIMEOUT) != HAL_OK) {
        LIS3MDL_CS_High(dev);
        return LIS3MDL_ERR;
    }

    status = LIS3MDL_CS_High(dev);
    if (status != LIS3MDL_OK) {
        return status;
    }

    *value = rx[1];
    return LIS3MDL_OK;
}

/**
 * @brief  Read multiple registers (auto-increment).
 */
LIS3MDL_Status_t LIS3MDL_ReadRegs(LIS3MDL_Handle_t *dev, uint8_t reg, uint8_t *buffer, uint16_t len){
    if (dev == NULL || dev->hspi == NULL || dev->cs_port == NULL ||
        buffer == NULL || len == 0U || len > 255U) {
        return LIS3MDL_ERR;
    }

    uint8_t cmd = (uint8_t)(reg | LIS3MDL_SPI_READ_BIT | LIS3MDL_SPI_AUTO_INC);

    LIS3MDL_Status_t status = LIS3MDL_CS_Low(dev);
    if (status != LIS3MDL_OK) {
        return status;
    }

    /* Send command byte */
    if (HAL_SPI_Transmit(dev->hspi, &cmd, 1, LIS3MDL_SPI_TIMEOUT) != HAL_OK) {
        LIS3MDL_CS_High(dev);
        return LIS3MDL_ERR;
    }

    /* Receive len bytes */
    if (HAL_SPI_Receive(dev->hspi, buffer, len, LIS3MDL_SPI_TIMEOUT) != HAL_OK) {
        LIS3MDL_CS_High(dev);
        return LIS3MDL_ERR;
    }

    return LIS3MDL_CS_High(dev);
}

/* =========================================================================
 * Data Acquisition
 * ========================================================================= */
/**
 * @brief  Read raw X/Y/Z magnetic data.
 */
LIS3MDL_Status_t LIS3MDL_ReadMagRaw(LIS3MDL_Handle_t *dev, LIS3MDL_AxesRaw_t *mag){

}

/**
 * @brief  Read X/Y/Z data converted to gauss.
 */
LIS3MDL_Status_t LIS3MDL_ReadMagGauss(LIS3MDL_Handle_t *dev, LIS3MDL_AxesFloat_t *mag){

}

/**
 * @brief  Read temperature (degC).
 */
LIS3MDL_Status_t LIS3MDL_ReadTemperature(LIS3MDL_Handle_t *dev, float *temp_c){

}

/* =========================================================================
 * Public Helpers
 * ========================================================================= */
/**
 * @brief  Set operating mode (continuous / single / power-down).
 */
LIS3MDL_Status_t LIS3MDL_SetMode(LIS3MDL_Handle_t *dev, LIS3MDL_Mode_t mode){

}

/**
 * @brief  Set output data rate for X/Y axes.
 */
LIS3MDL_Status_t LIS3MDL_SetODR(LIS3MDL_Handle_t *dev, LIS3MDL_ODR_t odr){

}

/**
 * @brief  Set full-scale range and update sensitivity.
 */
LIS3MDL_Status_t LIS3MDL_SetFullScale(LIS3MDL_Handle_t *dev, LIS3MDL_FullScale_t fs){

}

/**
 * @brief  Check whether new data is available (STATUS_REG ZYXDA bit).
 */
bool LIS3MDL_DataReady(LIS3MDL_Handle_t *dev){

}

/**
 * @brief  Enable/disable self-test.
 */
LIS3MDL_Status_t LIS3MDL_SelfTest(LIS3MDL_Handle_t *dev, bool enable){

}




/*
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
*/