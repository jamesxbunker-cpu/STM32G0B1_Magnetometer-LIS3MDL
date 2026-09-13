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
        (uint8_t)(LIS3MDL_SPI_WRITE | LIS3MDL_SPI_SINGLE | (reg & 0x3FU)),
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
        (uint8_t)(LIS3MDL_SPI_READ | LIS3MDL_SPI_SINGLE | (reg & 0x3FU)),
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

    uint8_t cmd = (uint8_t)(LIS3MDL_SPI_READ | LIS3MDL_SPI_AUTO_INC | (reg & 0x3FU));

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
    if (dev == NULL || mag == NULL) {
        return LIS3MDL_ERR;
    }

    uint8_t buf[6] = { 0 };
    LIS3MDL_Status_t status = LIS3MDL_ReadRegs(dev, LIS3MDL_OUT_X_L, buf, 6);
    if (status != LIS3MDL_OK) {
        return status;
    }

    mag->x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    mag->y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    mag->z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);

    return LIS3MDL_OK;
}

/**
 * @brief  Read X/Y/Z data converted to gauss.
 */
LIS3MDL_Status_t LIS3MDL_ReadMagGauss(LIS3MDL_Handle_t *dev, LIS3MDL_AxesFloat_t *mag){
    if (dev == NULL || mag == NULL) {
        return LIS3MDL_ERR;
    }

    LIS3MDL_AxesRaw_t raw;
    LIS3MDL_Status_t status = LIS3MDL_ReadMagRaw(dev, &raw);
    if (status != LIS3MDL_OK) {
        return status;
    }

    /* sensitivity is in LSB/gauss, so divide raw counts by it */
    mag->x = (float)raw.x / dev->sensitivity;
    mag->y = (float)raw.y / dev->sensitivity;
    mag->z = (float)raw.z / dev->sensitivity;

    return LIS3MDL_OK;
}

/**
 * @brief  Read temperature (degC).
 * @note   Datasheet: TEMP_OUT is 12-bit signed, 8 LSB/degC, offset ~25 degC.
 *         Some reference drivers use raw/8 + 25.0f, others use raw/8.
 *         Use the commonly accepted raw/8 + 25.0f form.
 */
LIS3MDL_Status_t LIS3MDL_ReadTemperature(LIS3MDL_Handle_t *dev, float *temp_c){
    if (dev == NULL || temp_c == NULL) {
        return LIS3MDL_ERR;
    }

    uint8_t buf[2] = { 0 };
    LIS3MDL_Status_t status = LIS3MDL_ReadRegs(dev, LIS3MDL_TEMP_OUT_L, buf, 2);
    if (status != LIS3MDL_OK) {
        return status;
    }

    int16_t raw = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);


    *temp_c = ((float)raw / 8.0f) + 25.0f;

    return LIS3MDL_OK;
}

/* =========================================================================
 * Public Helpers
 * ========================================================================= */
/**
 * @brief  Set operating mode (continuous / single / power-down).
 * @note   MD bits live in CTRL_REG3[1:0].
 *         For a proper single-shot, both must be set to single mode.
 */
LIS3MDL_Status_t LIS3MDL_SetMode(LIS3MDL_Handle_t *dev, LIS3MDL_Mode_t mode){
    if (dev == NULL) {
        return LIS3MDL_ERR;
    }

    uint8_t ctrl3 = 0;
    LIS3MDL_Status_t status = LIS3MDL_ReadReg(dev, LIS3MDL_CTRL_REG3, &ctrl3);
    if (status != LIS3MDL_OK) {
        return status;
    }

    ctrl3 &= (uint8_t)~LIS3MDL_CTRL3_MD_MASK;

    switch (mode) {
        case LIS3MDL_MODE_CONTINUOUS:
            ctrl3 |= LIS3MDL_CTRL3_MD_CONT;
            break;
        case LIS3MDL_MODE_SINGLE:
            ctrl3 |= LIS3MDL_CTRL3_MD_SINGLE;
            break;
        case LIS3MDL_MODE_POWERDOWN:
            ctrl3 |= LIS3MDL_CTRL3_MD_PD;
            break;
        default:
            ctrl3 |= LIS3MDL_CTRL3_MD_PD;
            break;
    }

    return LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG3, ctrl3);
}


/**
 * @brief  Set the low-rate output data rate (DO[2:0] in CTRL_REG1).
 * @note   Only valid when FAST_ODR = 0 (ODR 0.625–80 Hz, Table 21).
 *         This function clears FAST_ODR to guarantee the DO field is
 *         the one driving the ODR.  For the 155–1000 Hz rates you must
 *         use LIS3MDL_SetFastODR() instead, which repurposes the OM
 *         field to select the rate (Table 19).
 */
LIS3MDL_Status_t LIS3MDL_SetODR(LIS3MDL_Handle_t *dev, LIS3MDL_ODR_t odr)
{
    if (dev == NULL) {
        return LIS3MDL_ERR;
    }

    uint8_t ctrl1 = 0;
    LIS3MDL_Status_t status = LIS3MDL_ReadReg(dev, LIS3MDL_CTRL_REG1, &ctrl1);
    if (status != LIS3MDL_OK) {
        return status;
    }

    /* Set DO[2:0] */
    ctrl1 &= (uint8_t)~LIS3MDL_CTRL1_DO_MASK;
    ctrl1 |= (uint8_t)(((uint8_t)odr << LIS3MDL_CTRL1_DO_SHIFT)
                       & LIS3MDL_CTRL1_DO_MASK);

    /* Make sure FAST_ODR is 0 so DO drives the rate */
    ctrl1 &= (uint8_t)~LIS3MDL_CTRL1_FAST_ODR;

    return LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG1, ctrl1);
}

/**
 * @brief  Set a high-rate ODR (155–1000 Hz) via FAST_ODR + OM[1:0].
 * @note   Table 19: when FAST_ODR = 1, OM[1:0] selects the rate and
 *         DO[2:0] is ignored.  The OM field also controls the X/Y
 *         performance mode, so this call overwrites it — that is
 *         inherent to the part, not a limitation of this driver.
 * @param  om   one of LIS3MDL_CTRL1_OM_LP / MP / HP / UHP, corresponding
 *              to 1000 / 560 / 300 / 155 Hz respectively.
 */
LIS3MDL_Status_t LIS3MDL_SetFastODR(LIS3MDL_Handle_t *dev, uint8_t om)
{
    if (dev == NULL) {
        return LIS3MDL_ERR;
    }

    /* om must be one of the four OM[1:0] bit patterns already shifted */
    switch (om) {
        case LIS3MDL_CTRL1_OM_LP:
        case LIS3MDL_CTRL1_OM_MP:
        case LIS3MDL_CTRL1_OM_HP:
        case LIS3MDL_CTRL1_OM_UHP:
            break;
        default:
            return LIS3MDL_ERR;
    }

    uint8_t ctrl1 = 0;
    LIS3MDL_Status_t status = LIS3MDL_ReadReg(dev, LIS3MDL_CTRL_REG1, &ctrl1);
    if (status != LIS3MDL_OK) {
        return status;
    }

    ctrl1 &= (uint8_t)~LIS3MDL_CTRL1_OM_MASK;
    ctrl1 |= om;
    ctrl1 |= LIS3MDL_CTRL1_FAST_ODR;

    return LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG1, ctrl1);
}

/**
 * @brief  Set full-scale range and update sensitivity.
 * @note   DO bits are CTRL_REG1[4:2]. Also requires FAST_ODR handling:
 *         if the requested ODR > 80 Hz, FAST_ODR must be set; otherwise cleared.
 */
LIS3MDL_Status_t LIS3MDL_SetFullScale(LIS3MDL_Handle_t *dev, LIS3MDL_FullScale_t fs){
    if (dev == NULL) {
        return LIS3MDL_ERR;
    }


    uint8_t ctrl2 = 0;
    LIS3MDL_Status_t status = LIS3MDL_ReadReg(dev, LIS3MDL_CTRL_REG2, &ctrl2);
    if (status != LIS3MDL_OK) {
        return status;
    }

    ctrl2 &= (uint8_t)~LIS3MDL_CTRL2_FS_MASK;
    ctrl2 |= LIS3MDL_FS_ToRegBits(fs);

    status = LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG2, ctrl2);
    if (status != LIS3MDL_OK) {
        return status;
    }

    dev->fs          = fs;
    dev->sensitivity = LIS3MDL_FS_ToSensitivity(fs);

    return LIS3MDL_OK;
}

/**
 * @brief  Check whether new data is available (STATUS_REG ZYXDA bit).
 */
bool LIS3MDL_DataReady(LIS3MDL_Handle_t *dev){
    if (dev == NULL) {
        return false;
    }

    uint8_t status_reg = 0;
    if (LIS3MDL_ReadReg(dev, LIS3MDL_STATUS_REG, &status_reg) != LIS3MDL_OK) {
        return false;
    }

    return (status_reg & LIS3MDL_STATUS_ZYXDA) != 0U;
}

/**
 * @brief  Internal helper to enable/disable self-test while preserving
 *         the Z-axis operating mode (MD_Z) in CTRL_REG1[0].
 */
LIS3MDL_Status_t LIS3MDL_SelfTestEnable(LIS3MDL_Handle_t *dev, bool enable)
{
    uint8_t ctrl1 = 0;
    LIS3MDL_Status_t s = LIS3MDL_ReadReg(dev, LIS3MDL_CTRL_REG1, &ctrl1);
    if (s != LIS3MDL_OK) return s;

    if (enable) {
        ctrl1 |=  LIS3MDL_CTRL1_ST;
    } else {
        ctrl1 &= (uint8_t)~LIS3MDL_CTRL1_ST;
    }

    return LIS3MDL_WriteReg(dev, LIS3MDL_CTRL_REG1, ctrl1);
}

/**
 * @brief  Perform a full self-test and return the result.
 * @note   Implements the procedure from AN4602 Figure 6.
 *         The magnetometer must be configured in continuous mode
 *         with BDU enabled before calling this function.
 * @retval LIS3MDL_OK if self-test passed, LIS3MDL_ERR otherwise.
 */
LIS3MDL_Status_t LIS3MDL_SelfTest(LIS3MDL_Handle_t *dev)
{
    if (dev == NULL) return LIS3MDL_ERR;

    if (LIS3MDL_SelfTestEnable(dev, true) != LIS3MDL_OK) return LIS3MDL_ERR;
    HAL_Delay(60);

    /* Discard first sample (AN4602 Figure 6) */
    LIS3MDL_AxesRaw_t raw;
    if (LIS3MDL_ReadMagRaw(dev, &raw) != LIS3MDL_OK) {
        LIS3MDL_SelfTestEnable(dev, false);
        return LIS3MDL_ERR;
    }

    /* Average 5 samples */
    int32_t sx = 0, sy = 0, sz = 0;
    for (uint8_t i = 0; i < 5; i++) {
        uint32_t to = 0;
        while (!LIS3MDL_DataReady(dev)) {
            if (++to > 100000U) {
                LIS3MDL_SelfTestEnable(dev, false);
                return LIS3MDL_ERR;
            }
        }
        if (LIS3MDL_ReadMagRaw(dev, &raw) != LIS3MDL_OK) {
            LIS3MDL_SelfTestEnable(dev, false);
            return LIS3MDL_ERR;
        }
        sx += raw.x; sy += raw.y; sz += raw.z;
    }

    LIS3MDL_SelfTestEnable(dev, false);

    float ax = (float)sx / 5.0f / dev->sensitivity;
    float ay = (float)sy / 5.0f / dev->sensitivity;
    float az = (float)sz / 5.0f / dev->sensitivity;
    if (ax < 0) ax = -ax;
    if (ay < 0) ay = -ay;
    if (az < 0) az = -az;

    if (ax < LIS3MDL_ST_X_MIN || ax > LIS3MDL_ST_X_MAX) return LIS3MDL_ERR;
    if (ay < LIS3MDL_ST_Y_MIN || ay > LIS3MDL_ST_Y_MAX) return LIS3MDL_ERR;
    if (az < LIS3MDL_ST_Z_MIN || az > LIS3MDL_ST_Z_MAX) return LIS3MDL_ERR;

    return LIS3MDL_OK;
}