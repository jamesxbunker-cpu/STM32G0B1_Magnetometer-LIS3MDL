/**
 * @file    LIS3MDL_Driver.h
 * @brief   LIS3MDL 3-axis magnetometer driver header (4-wire SPI).
 * @note    Designed for STM32 HAL. SPI Mode 3 (CPOL=1, CPHA=1).
 * @author  James Bunker
 * @date    2026-09-10
 */

#ifndef LIS3MDL_DRIVER_H
#define LIS3MDL_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g0xx_hal.h"

/* =========================================================================
 * Device Identification
 * ========================================================================= */
#define LIS3MDL_WHO_AM_I_VALUE      0x3D

/* =========================================================================
 * Register Map
 * ========================================================================= */
#define LIS3MDL_WHO_AM_I            0x0F
#define LIS3MDL_CTRL_REG1           0x20
#define LIS3MDL_CTRL_REG2           0x21
#define LIS3MDL_CTRL_REG3           0x22
#define LIS3MDL_CTRL_REG4           0x23
#define LIS3MDL_CTRL_REG5           0x24
#define LIS3MDL_STATUS_REG          0x27
#define LIS3MDL_OUT_X_L             0x28
#define LIS3MDL_OUT_X_H             0x29
#define LIS3MDL_OUT_Y_L             0x2A
#define LIS3MDL_OUT_Y_H             0x2B
#define LIS3MDL_OUT_Z_L             0x2C
#define LIS3MDL_OUT_Z_H             0x2D
#define LIS3MDL_TEMP_OUT_L          0x2E
#define LIS3MDL_TEMP_OUT_H          0x2F
#define LIS3MDL_INT_CFG             0x30
#define LIS3MDL_INT_SRC             0x31
#define LIS3MDL_INT_THS_L           0x32
#define LIS3MDL_INT_THS_H           0x33

/* =========================================================================
 * CTRL_REG1 (0x20) Bit Definitions
 * ========================================================================= */
#define LIS3MDL_CTRL1_TEMP_EN       (1 << 7)        /* Enable Temperature Sensor    */
#define LIS3MDL_CTRL1_OM_MASK       (0x07 << 5)     /* X/Y operative mode           */
#define LIS3MDL_CTRL1_OM_LP         (0x00 << 5)     /* Low-power                    */
#define LIS3MDL_CTRL1_OM_MP         (0x01 << 5)     /* Medium-power                 */
#define LIS3MDL_CTRL1_OM_HP         (0x02 << 5)     /* High-power                   */
#define LIS3MDL_CTRL1_OM_UHP        (0x03 << 5)     /* Ultra-high-power             */
#define LIS3MDL_CTRL1_DO_SHIFT      (2U)    
#define LIS3MDL_CTRL1_DO_MASK       (0x07 << 2)     /* X/Y ODR                      */
#define LIS3MDL_CTRL1_FAST_ODR      (1 << 1)        /* Fast ODR Enabled             */        
#define LIS3MDL_CTRL1_ST            (1 << 0)        /* Self-test                    */

/* =========================================================================
 * CTRL_REG2 (0x21) Bit Definitions
 * ========================================================================= */
#define LIS3MDL_CTRL2_FS_MASK       (0x03 << 5)     /* Full-scale selection */
#define LIS3MDL_CTRL2_FS_4G         (0x00 << 5)     /* +/-4  gauss          */
#define LIS3MDL_CTRL2_FS_8G         (0x01 << 5)     /* +/-8  gauss          */
#define LIS3MDL_CTRL2_FS_12G        (0x02 << 5)     /* +/-12 gauss          */
#define LIS3MDL_CTRL2_FS_16G        (0x03 << 5)     /* +/-16 gauss          */
#define LIS3MDL_CTRL2_REBOOT        (1 << 3)        /* Reboot               */
#define LIS3MDL_CTRL2_SOFT_RST      (1 << 2)        /* Software Reset       */

/* =========================================================================
 * CTRL_REG3 (0x22) Bit Definitions
 * ========================================================================= */
#define LIS3MDL_CTRL3_LP            (1 << 5)        /* Low Power Config         */
#define LIS3MDL_CTRL3_SIM           (1 << 2)        /* 0 = 4-wire, 1 = 3-wire   */
#define LIS3MDL_CTRL3_MD_MASK       (0x03)
#define LIS3MDL_CTRL3_MD_CONT       (0x00)          /* Continuous-conversion    */
#define LIS3MDL_CTRL3_MD_SINGLE     (0x01)          /* Single-conversion        */
#define LIS3MDL_CTRL3_MD_PD         (0x03)          /* Power-down               */

/* =========================================================================
 * CTRL_REG4 (0x23) Bit Definitions
 * ========================================================================= */
#define LIS3MDL_CTRL4_OMZ_MASK      (0x03 << 2)     /* Z operative mode                     */
#define LIS3MDL_CTRL4_OMZ_LP        (0x00 << 2)     /* Z operative mode Low Power           */
#define LIS3MDL_CTRL4_OMZ_MP        (0x01 << 2)     /* Z operative mode Medium Power        */
#define LIS3MDL_CTRL4_OMZ_HP        (0x02 << 2)     /* Z operative mode High Power          */
#define LIS3MDL_CTRL4_OMZ_UHP       (0x03 << 2)     /* Z operative mode Ultra-high Power    */
#define LIS3MDL_CTRL4_BLE           (1 << 1)        /* Big/little endian                    */

/* =========================================================================
 * CTRL_REG5 (0x24) Bit Definitions
 * ========================================================================= */
#define LIS3MDL_CTRL5_FAST_READ     (1 << 7)        /* Fast Read            */
#define LIS3MDL_CTRL5_BDU           (1 << 6)        /* Block Data Update    */


/* =========================================================================
 * STATUS_REG (0x27) Bit Definitions
 * ========================================================================= */
#define LIS3MDL_STATUS_ZYXOR        (1 << 7)
#define LIS3MDL_STATUS_ZOR          (1 << 6)
#define LIS3MDL_STATUS_YOR          (1 << 5)
#define LIS3MDL_STATUS_XOR          (1 << 4)
#define LIS3MDL_STATUS_ZYXDA        (1 << 3)
#define LIS3MDL_STATUS_ZDA          (1 << 2)
#define LIS3MDL_STATUS_YDA          (1 << 1)
#define LIS3MDL_STATUS_XDA          (1 << 0)

/* =========================================================================
 * SPI Command Byte Macros
 * ========================================================================= */
#define LIS3MDL_SPI_READ           0x80  /* RW bit  = 1 */
#define LIS3MDL_SPI_WRITE          0x00  /* RW bit  = 0 */
#define LIS3MDL_SPI_AUTO_INC       0x40  /* MS bit  = 1 */
#define LIS3MDL_SPI_SINGLE         0x00  /* MS bit  = 0 */

/* =========================================================================
 * Sensitivity (LSB/gauss) per full-scale range
 * ========================================================================= */
#define LIS3MDL_SENS_4G            6842.0f
#define LIS3MDL_SENS_8G            3421.0f
#define LIS3MDL_SENS_12G           2281.0f
#define LIS3MDL_SENS_16G           1711.0f


/* --- Self-test limits (AN4602 Table 17, FS = 12 gauss) --- */
#define LIS3MDL_ST_X_MIN  1.0f   /* gauss */
#define LIS3MDL_ST_X_MAX  3.0f   /* gauss */
#define LIS3MDL_ST_Y_MIN  1.0f   /* gauss */
#define LIS3MDL_ST_Y_MAX  3.0f   /* gauss */
#define LIS3MDL_ST_Z_MIN  0.1f   /* gauss */
#define LIS3MDL_ST_Z_MAX  1.0f   /* gauss */

/* =========================================================================
 * Enumerations
 * ========================================================================= */
typedef enum {
    LIS3MDL_OK      = 0,
    LIS3MDL_ERR     = -1,
    LIS3MDL_ERR_ID  = -2,
} LIS3MDL_Status_t;

typedef enum {
    LIS3MDL_FS_4G  = 0,
    LIS3MDL_FS_8G  = 1,
    LIS3MDL_FS_12G = 2,
    LIS3MDL_FS_16G = 3,
} LIS3MDL_FullScale_t;

typedef enum {
    LIS3MDL_ODR_0_625Hz = 0,
    LIS3MDL_ODR_1_25Hz  = 1,
    LIS3MDL_ODR_2_5Hz   = 2,
    LIS3MDL_ODR_5Hz     = 3,
    LIS3MDL_ODR_10Hz    = 4,
    LIS3MDL_ODR_20Hz    = 5,
    LIS3MDL_ODR_40Hz    = 6,
    LIS3MDL_ODR_80Hz    = 7,
} LIS3MDL_ODR_t;

typedef enum {
    LIS3MDL_MODE_CONTINUOUS = 0,
    LIS3MDL_MODE_SINGLE     = 1,
    LIS3MDL_MODE_POWERDOWN  = 2,
} LIS3MDL_Mode_t;

/* =========================================================================
 * Data Structures
 * ========================================================================= */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} LIS3MDL_AxesRaw_t;

typedef struct {
    float x;    /* gauss */
    float y;    /* gauss */
    float z;    /* gauss */
} LIS3MDL_AxesFloat_t;

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
    LIS3MDL_FullScale_t fs;
    float              sensitivity;   /* LSB/gauss */
} LIS3MDL_Handle_t;

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief  Initialize the LIS3MDL over SPI.
 * @param  dev       Pointer to driver handle (fill hspi, cs_port, cs_pin first).
 * @param  fs        Full-scale range.
 * @retval LIS3MDL_OK on success, error code otherwise.
 */
LIS3MDL_Status_t LIS3MDL_Init(LIS3MDL_Handle_t *dev, LIS3MDL_FullScale_t fs);

/**
 * @brief  Verify device ID (WHO_AM_I == 0x3D).
 */
LIS3MDL_Status_t LIS3MDL_CheckID(LIS3MDL_Handle_t *dev);

/**
 * @brief  Write a single register.
 */
LIS3MDL_Status_t LIS3MDL_WriteReg(LIS3MDL_Handle_t *dev, uint8_t reg, uint8_t value);

/**
 * @brief  Read a single register.
 */
LIS3MDL_Status_t LIS3MDL_ReadReg(LIS3MDL_Handle_t *dev, uint8_t reg, uint8_t *value);

/**
 * @brief  Read multiple registers (auto-increment).
 */
LIS3MDL_Status_t LIS3MDL_ReadRegs(LIS3MDL_Handle_t *dev, uint8_t reg, uint8_t *buffer, uint16_t len);

/**
 * @brief  Read raw X/Y/Z magnetic data.
 */
LIS3MDL_Status_t LIS3MDL_ReadMagRaw(LIS3MDL_Handle_t *dev, LIS3MDL_AxesRaw_t *mag);

/**
 * @brief  Read X/Y/Z data converted to gauss.
 */
LIS3MDL_Status_t LIS3MDL_ReadMagGauss(LIS3MDL_Handle_t *dev, LIS3MDL_AxesFloat_t *mag);

/**
 * @brief  Read temperature (degC).
 */
LIS3MDL_Status_t LIS3MDL_ReadTemperature(LIS3MDL_Handle_t *dev, float *temp_c);

/**
 * @brief  Set operating mode (continuous / single / power-down).
 */
LIS3MDL_Status_t LIS3MDL_SetMode(LIS3MDL_Handle_t *dev, LIS3MDL_Mode_t mode);

/**
 * @brief  Set output data rate for X/Y axes.
 */
LIS3MDL_Status_t LIS3MDL_SetODR(LIS3MDL_Handle_t *dev, LIS3MDL_ODR_t odr);

/**
 * @brief  Set output data rate for X/Y axes.
 */
LIS3MDL_Status_t LIS3MDL_SetFastODR(LIS3MDL_Handle_t *dev, uint8_t om)

/**
 * @brief  Set full-scale range and update sensitivity.
 */
LIS3MDL_Status_t LIS3MDL_SetFullScale(LIS3MDL_Handle_t *dev, LIS3MDL_FullScale_t fs);

/**
 * @brief  Check whether new data is available (STATUS_REG ZYXDA bit).
 */
bool LIS3MDL_DataReady(LIS3MDL_Handle_t *dev);

/**
 * @brief  Enable/disable self-test.
 */
LIS3MDL_Status_t LIS3MDL_SelfTestEnable(LIS3MDL_Handle_t *dev, bool enable);

/**
 * @brief  Self-test.
 */
LIS3MDL_Status_t LIS3MDL_SelfTest(LIS3MDL_Handle_t *dev);

#endif /* LIS3MDL_DRIVER_H */