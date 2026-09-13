/**
 * @file    LIS3MDL_Test.c
 * @brief   LIS3MDL 3-axis magnetometer test program.
 * @note    Designed for STM32 HAL. SPI Mode 3 (CPOL=1, CPHA=1).
 * @author  James Bunker
 * @date    2026-09-10
 */

#include "LIS3MDL_Test.h"
#include "LIS3MDL_Driver.h"
#include <stdio.h>
#include "stm32g0xx_hal.h"
#include "spi.h"

/* =========================================================================
 * Test configuration
 * ========================================================================= */
#define LIS3MDL_TEST_ITERATIONS  10U     /* how many samples to print */
#define LIS3MDL_TEST_DELAY_MS    200U    /* delay between samples      */

/* =========================================================================
 * Test result counters
 * ========================================================================= */
typedef struct {
    uint32_t pass;
    uint32_t fail;
} LIS3MDL_TestStats_t;

static LIS3MDL_TestStats_t g_stats = { 0, 0 };

static void check(const char *name, LIS3MDL_Status_t s)
{
    if (s == LIS3MDL_OK) {
        printf("  [PASS] %s\r\n", name);
        g_stats.pass++;
    } else {
        printf("  [FAIL] %s (status=%d)\r\n", name, (int)s);
        g_stats.fail++;
    }
}

extern UART_HandleTypeDef huart2; // This should match what CubeMX generates

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

/* =========================================================================
 * Public entry point
 * ========================================================================= */
void LIS3MDL_Test(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf(" LIS3MDL Driver Test\r\n");
    printf("========================================\r\n");

    /* ---------------------------------------------------------------------
     * 0. Handle setup
     *    The caller (main / test harness) must have already filled these
     *    in before calling LIS3MDL_Test().  We declare the handle here
     *    for demonstration; in your real code it will live elsewhere.
     * ------------------------------------------------------------------- */
    LIS3MDL_Handle_t dev;
    dev.hspi    = &hspi1;          /* change to your SPI handle          */
    dev.cs_port = SPI1_CS_GPIO_Port;
    dev.cs_pin  = SPI1_CS_Pin;
    dev.fs          = LIS3MDL_FS_4G;
    dev.sensitivity = 0.0f;

    /* ---------------------------------------------------------------------
     * 1. Init
     * ------------------------------------------------------------------- */
    printf("\r\n-- Init --\r\n");
    check("LIS3MDL_Init (FS = 4G)",
          LIS3MDL_Init(&dev, LIS3MDL_FS_4G));

    /* ---------------------------------------------------------------------
     * 2. WHO_AM_I
     * ------------------------------------------------------------------- */
    printf("\r\n-- WHO_AM_I --\r\n");
    check("LIS3MDL_CheckID",
          LIS3MDL_CheckID(&dev));

    /* ---------------------------------------------------------------------
     * 3. Single register read/write round-trip
     *    CTRL_REG5 has two usable bits (FAST_READ, BDU) and two reserved
     *    bits that must stay 0.  We toggle BDU and read it back.
     * ------------------------------------------------------------------- */
    printf("\r\n-- Register round-trip (CTRL_REG5.BDU) --\r\n");
    {
        uint8_t before = 0, after = 0;

        LIS3MDL_Status_t s = LIS3MDL_ReadReg(&dev, LIS3MDL_CTRL_REG5, &before);
        if (s == LIS3MDL_OK) {
            /* Clear BDU */
            uint8_t clear = (uint8_t)(before & (uint8_t)~LIS3MDL_CTRL5_BDU);
            s = LIS3MDL_WriteReg(&dev, LIS3MDL_CTRL_REG5, clear);
        }
        if (s == LIS3MDL_OK) {
            s = LIS3MDL_ReadReg(&dev, LIS3MDL_CTRL_REG5, &after);
        }
        if (s == LIS3MDL_OK) {
            check("clear BDU",
                  (after & LIS3MDL_CTRL5_BDU) == 0 ? LIS3MDL_OK : LIS3MDL_ERR);
        } else {
            check("clear BDU", LIS3MDL_ERR);
        }

        /* Restore BDU = 1 */
        s = LIS3MDL_WriteReg(&dev, LIS3MDL_CTRL_REG5,
                             (uint8_t)(after | LIS3MDL_CTRL5_BDU));
        if (s == LIS3MDL_OK) {
            uint8_t restored = 0;
            s = LIS3MDL_ReadReg(&dev, LIS3MDL_CTRL_REG5, &restored);
            check("restore BDU",
                  (s == LIS3MDL_OK && (restored & LIS3MDL_CTRL5_BDU))
                      ? LIS3MDL_OK : LIS3MDL_ERR);
        } else {
            check("restore BDU", LIS3MDL_ERR);
        }
    }

    /* ---------------------------------------------------------------------
     * 4. Data-ready polling
     * ------------------------------------------------------------------- */
    printf("\r\n-- Data-ready --\r\n");
    {
        uint32_t timeout = 0;
        while (!LIS3MDL_DataReady(&dev)) {
            if (++timeout > 100000U) break;
        }
        check("LIS3MDL_DataReady within timeout",
              (timeout <= 100000U) ? LIS3MDL_OK : LIS3MDL_ERR);
    }

    /* ---------------------------------------------------------------------
     * 5. Raw + gauss + temperature stream
     * ------------------------------------------------------------------- */
    printf("\r\n-- Streaming %u samples --\r\n", LIS3MDL_TEST_ITERATIONS);
    printf("  %-4s  %-8s %-8s %-8s   %-9s %-9s %-9s   %-8s\r\n",
           "n", "rawX", "rawY", "rawZ", "gaussX", "gaussY", "gaussZ", "tempC");

    for (uint32_t i = 0; i < LIS3MDL_TEST_ITERATIONS; i++) {
        LIS3MDL_AxesRaw_t  raw;
        LIS3MDL_AxesFloat_t gauss;
        float temp_c = 0.0f;

        LIS3MDL_Status_t s = LIS3MDL_ReadMagRaw(&dev, &raw);
        if (s == LIS3MDL_OK) {
            s = LIS3MDL_ReadMagGauss(&dev, &gauss);
        }
        if (s == LIS3MDL_OK) {
            s = LIS3MDL_ReadTemperature(&dev, &temp_c);
        }

        if (s == LIS3MDL_OK) {
            printf("  %-4lu  %-8d %-8d %-8d   %-9.4f %-9.4f %-9.4f   %-8.2f\r\n",
                   (unsigned long)(i + 1),
                   (int)raw.x, (int)raw.y, (int)raw.z,
                   gauss.x, gauss.y, gauss.z,
                   temp_c);
        } else {
            printf("  %-4lu  read failed (status=%d)\r\n",
                   (unsigned long)(i + 1), (int)s);
        }

        HAL_Delay(LIS3MDL_TEST_DELAY_MS);
    }

    /* ---------------------------------------------------------------------
     * 6. Full-scale change
     * ------------------------------------------------------------------- */
    printf("\r\n-- Full-scale change --\r\n");
    check("SetFullScale(16G)",
          LIS3MDL_SetFullScale(&dev, LIS3MDL_FS_16G));
    printf("  cached sensitivity = %.1f LSB/gauss\r\n", dev.sensitivity);

    check("SetFullScale(4G, restore)",
          LIS3MDL_SetFullScale(&dev, LIS3MDL_FS_4G));

    /* ---------------------------------------------------------------------
     * 7. Mode change
     * ------------------------------------------------------------------- */
    printf("\r\n-- Mode change --\r\n");
    check("SetMode(POWERDOWN)",
          LIS3MDL_SetMode(&dev, LIS3MDL_MODE_POWERDOWN));
    HAL_Delay(10);
    check("SetMode(CONTINUOUS)",
          LIS3MDL_SetMode(&dev, LIS3MDL_MODE_CONTINUOUS));
    HAL_Delay(10);

    /* ---------------------------------------------------------------------
     * 8. ODR change
     * ------------------------------------------------------------------- */
    printf("\r\n-- ODR change --\r\n");
    check("SetODR(10 Hz)",
          LIS3MDL_SetODR(&dev, LIS3MDL_ODR_10Hz));
    check("SetODR(80 Hz)",
          LIS3MDL_SetODR(&dev, LIS3MDL_ODR_80Hz));
    check("SetFastODR(UHP = 155 Hz)",
          LIS3MDL_SetFastODR(&dev, LIS3MDL_CTRL1_OM_UHP));

    /* ---------------------------------------------------------------------
     * 9. Self-test (AN4602 Figure 6)
     * ------------------------------------------------------------------- */
    printf("\r\n-- Self-test (AN4602) --\r\n");
    check("LIS3MDL_SelfTest",
          LIS3MDL_SelfTest(&dev));

    /* ---------------------------------------------------------------------
     * 10. Summary
     * ------------------------------------------------------------------- */
    printf("\r\n========================================\r\n");
    printf(" Test summary: %lu passed, %lu failed\r\n",
           (unsigned long)g_stats.pass,
           (unsigned long)g_stats.fail);
    printf(" Overall: %s\r\n",
           (g_stats.fail == 0) ? "PASS" : "FAIL");
    printf("========================================\r\n");
}