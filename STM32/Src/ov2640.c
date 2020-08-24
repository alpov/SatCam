/*************************************************************************
 *
 * SatCam - Camera Module for PSAT-2
 * Copyright (c) 2015-2017 Ales Povalac <alpov@alpov.net>
 * Dept. of Radio Electronics, Brno University of Technology
 *
 * This work is licensed under the terms of the MIT license
 *
 * Based on OV2640 driver from OpenMV project
 * Copyright (c) 2013/2014 Ibrahim Abdelkader <i.abdalkader@gmail.com>
 *
 *************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "cube.h"
#include "eeprom.h"
#include "ov2640.h"

#define INCLUDE_OV2640_REGS
#include "ov2640_regs.h"


static uint8_t SCCB_Write(uint8_t addr, uint8_t data)
{
    uint8_t ret;
    uint8_t buf[] = {addr, data};

    if (HAL_I2C_Master_Transmit(&hi2c2, SLAVE_ADDR, buf, 2, SCCB_TIMEOUT) != HAL_OK) {
        return 0xFF;
    }
    return ret;
}


static uint8_t SCCB_Read(uint8_t addr)
{
    uint8_t data;

    if (HAL_I2C_Master_Transmit(&hi2c2, SLAVE_ADDR, &addr, 1, SCCB_TIMEOUT) != HAL_OK) {
        return 0xFF;
    }
    if (HAL_I2C_Master_Receive(&hi2c2, SLAVE_ADDR, &data, 1, SCCB_TIMEOUT) != HAL_OK) {
        return 0xFF;
    }
    return data;
}


static void SCCB_Write_Multi(const uint8_t (*regs)[2])
{
    int i = 0;
    while (regs[i][0]) { // write until end of table (0)
        SCCB_Write(regs[i][0], regs[i][1]);
        i++;
    }
}


bool ov2640_enable(bool en)
{
    if (en) {
        /* check high speed core clock */
        if (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_SYSCLKSOURCE_STATUS_PLLCLK) return false;

        /* chip enable */
        syslog_event(LOG_CAM_START);
        HAL_GPIO_WritePin(CAM_RST_GPIO_Port, CAM_RST_Pin, 0); // assert reset
        HAL_GPIO_WritePin(CAM_ENB_GPIO_Port, CAM_ENB_Pin, 1); // enable power
        HAL_Delay(5); // min. 2ms for power supply
        HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_PLLCLK, RCC_MCODIV_5); // MCO1 - XCLK enable
        HAL_Delay(3); // camera delay
        HAL_GPIO_WritePin(CAM_RST_GPIO_Port, CAM_RST_Pin, 1); // deassert reset
        HAL_Delay(5); // camera delay

        /* check sensor connectivity */
        if (SCCB_Read(0x0A) != 0x26) {
            syslog_event(LOG_CAM_I2C_ERROR);
            return false;
        }

        /* initialize sensor */
        SCCB_Write_Multi(OV2640_RESET);
        HAL_Delay(5); // camera delay
        SCCB_Write_Multi(OV2640_JPEG_INIT);

        /* frame size; timing for XCLK=12MHz, 43% duty, CLKRC=0x00 */
        // SCCB_Write_Multi(OV2640_SENSOR_SMALL); SCCB_Write_Multi(OV2640_DSP_160x120); // 6MHz
        // SCCB_Write_Multi(OV2640_SENSOR_SMALL); SCCB_Write_Multi(OV2640_DSP_176x144); // 6MHz
        SCCB_Write_Multi(OV2640_SENSOR_SMALL); SCCB_Write_Multi(OV2640_DSP_320x240); // 6MHz
        // SCCB_Write_Multi(OV2640_SENSOR_SMALL); SCCB_Write_Multi(OV2640_DSP_352x288); // 6MHz, 13.7fps
        // SCCB_Write_Multi(OV2640_SENSOR_LARGE); SCCB_Write_Multi(OV2640_DSP_640x480); // 9MHz, 7.14fps
        // SCCB_Write_Multi(OV2640_SENSOR_LARGE); SCCB_Write_Multi(OV2640_DSP_800x600); // 18MHz, 7.14fps
        // SCCB_Write_Multi(OV2640_SENSOR_LARGE); SCCB_Write_Multi(OV2640_DSP_1024x768); // 18MHz, 7.14fps, q=10 -> 9MHz, 3.57fps
        // SCCB_Write_Multi(OV2640_SENSOR_LARGE); SCCB_Write_Multi(OV2640_DSP_1280x1024); // 18MHz, q=20
        // SCCB_Write_Multi(OV2640_SENSOR_LARGE); SCCB_Write_Multi(OV2640_DSP_1600x1200); // 18MHz, q=50

        /* enable JPEG */
        SCCB_Write_Multi(OV2640_JPEG_ON);
    } else {
        /* MCO1 - XCLK disable */
        GPIO_InitTypeDef GPIO_InitStruct;
        GPIO_InitStruct.Pin = XCLK_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(XCLK_GPIO_Port, &GPIO_InitStruct);

        HAL_GPIO_WritePin(CAM_ENB_GPIO_Port, CAM_ENB_Pin, 0); // disable power
        HAL_GPIO_WritePin(CAM_RST_GPIO_Port, CAM_RST_Pin, 0); // assert reset
        syslog_event(LOG_CAM_STOP);
    }

    return true;
}


bool ov2640_enable_safe(bool en)
{
    if (en) {
        for (uint8_t i = 0; i < SENSOR_INIT_RETRY; i++) {
            if (ov2640_enable(true)) return true;
            ov2640_enable(false); // init failed - shutdown, wait a while, and try again
            HAL_Delay(50);
            HAL_IWDG_Refresh(&hiwdg); // 50ms period
        }
        return false;
    } else {
        ov2640_enable(false);
        return true;
    }
}


uint32_t ov2640_snapshot(uint8_t *buffer, uint32_t length)
{
    /* Convert length from byte to dword */
    length = (length + 3) / 4;

    /* Start the DCMI */
    syslog_event(LOG_CAM_SNAPSHOT);
    __HAL_DCMI_ENABLE(&hdcmi);
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)(buffer), length);

    /* Wait for frame */
    uint32_t snapshot_start = HAL_GetTick();
    while ((hdcmi.Instance->CR & DCMI_CR_CAPTURE) != 0) {
        if ((HAL_GetTick() - snapshot_start) >= SENSOR_TIMEOUT) {
            /* Sensor timeout, most likely a HW issue */
            HAL_DCMI_Stop(&hdcmi);
            syslog_event(LOG_CAM_DCMI_ERROR);
            return 0;
        }
    }

    /* The frame is finished, but DMA still waiting for data because we
       set max frame size, so we need to abort the DMA transfer here */
    HAL_DCMI_Stop(&hdcmi);

    /* Buffer full -> part of JPEG has been dropped */
    if (hdma_dcmi.Instance->NDTR == 0) syslog_event(LOG_CAM_SIZE_ERROR);

    /* Read the number of data items transferred */
    return (length - hdma_dcmi.Instance->NDTR) * 4;
}


void ov2640_set_awb(uint8_t mode)
{
    switch (mode) {
        case AWB_AUTO:
        default:
            SCCB_Write_Multi(OV2640_AWB_AUTO);
            break;
        case AWB_CLOUDY:
            SCCB_Write_Multi(OV2640_AWB_CLOUDY);
            break;
        case AWB_HOME:
            SCCB_Write_Multi(OV2640_AWB_HOME);
            break;
        case AWB_OFFICE:
            SCCB_Write_Multi(OV2640_AWB_OFFICE);
            break;
        case AWB_SUNNY:
            SCCB_Write_Multi(OV2640_AWB_SUNNY);
            break;
    }
}


uint16_t ov2640_get_current_agc(void)
{
    uint16_t reg00 = ov2640_get_register(BANK_SEL_SENSOR, 0x00);
    uint16_t reg45 = ov2640_get_register(BANK_SEL_SENSOR, 0x45);

    return (reg00 | ((reg45 & 0xc0) << 2));
}


uint16_t ov2640_get_current_aec(void)
{
    uint16_t reg04 = ov2640_get_register(BANK_SEL_SENSOR, 0x04);
    uint16_t reg10 = ov2640_get_register(BANK_SEL_SENSOR, 0x10);
    uint16_t reg45 = ov2640_get_register(BANK_SEL_SENSOR, 0x45);

    return ((reg04 & 0x03) | (reg10 << 2) | ((reg45 & 0x3f) << 10));
}


void ov2640_set_register(uint8_t bank, uint8_t reg, uint8_t value)
{
    SCCB_Write(BANK_SEL, bank);
    SCCB_Write(reg, value);
}


uint8_t ov2640_get_register(uint8_t bank, uint8_t reg)
{
    SCCB_Write(BANK_SEL, bank);
    return SCCB_Read(reg);
}


bool ov2640_hilevel_init(CONFIG_CAMERA cam)
{
    if (!ov2640_enable_safe(true)) return false;

    ov2640_set_register(BANK_SEL_DSP, 0x44, cam.qs); // 0~100%, 255~0%, default 95%

    uint8_t reg13 = 0xc0; // banding off
    if (cam.agc) reg13 |= 0x04; // AGC
    if (cam.aec) reg13 |= 0x01; // AEC
    ov2640_set_register(BANK_SEL_SENSOR, 0x13, reg13);

    if (cam.agc) {
        switch (cam.agc_ceiling) {
            case 2: ov2640_set_register(BANK_SEL_SENSOR, 0x14, (0 << 5) | 0x08); break;
            case 4: ov2640_set_register(BANK_SEL_SENSOR, 0x14, (1 << 5) | 0x08); break;
            case 8: ov2640_set_register(BANK_SEL_SENSOR, 0x14, (2 << 5) | 0x08); break;
            case 16: default: ov2640_set_register(BANK_SEL_SENSOR, 0x14, (3 << 5) | 0x08); break;
            case 32: ov2640_set_register(BANK_SEL_SENSOR, 0x14, (4 << 5) | 0x08); break;
            case 64: ov2640_set_register(BANK_SEL_SENSOR, 0x14, (5 << 5) | 0x08); break;
            case 128: ov2640_set_register(BANK_SEL_SENSOR, 0x14, (6 << 5) | 0x08); break;
        }
    } else {
        uint8_t reg00 = (cam.agc_manual) & 0xff;
        uint8_t reg45 = (ov2640_get_register(BANK_SEL_SENSOR, 0x45) & 0x3f) | ((cam.agc_manual >> 2) & 0xc0);
        ov2640_set_register(BANK_SEL_SENSOR, 0x00, reg00);
        ov2640_set_register(BANK_SEL_SENSOR, 0x45, reg45);
    }

    if (!cam.aec) {
        uint8_t reg04 = (ov2640_get_register(BANK_SEL_SENSOR, 0x04) & 0xfc) | (cam.aec_manual & 0x03);
        uint8_t reg10 = (cam.aec_manual >> 2) & 0xff;
        uint8_t reg45 = (ov2640_get_register(BANK_SEL_SENSOR, 0x45) & 0xc0) | ((cam.aec_manual >> 10) & 0x3f);
        ov2640_set_register(BANK_SEL_SENSOR, 0x04, reg04);
        ov2640_set_register(BANK_SEL_SENSOR, 0x10, reg10);
        ov2640_set_register(BANK_SEL_SENSOR, 0x45, reg45);
    }

    ov2640_set_awb(cam.awb);

    /* camera init delay - to adjust AGC, AEC and AWB */
    for (uint16_t i = 0; i < cam.delay/50; i++) {
        HAL_Delay(50);
        HAL_IWDG_Refresh(&hiwdg); // 50ms period
    }

    syslog_event(LOG_CAM_READY);
    return true;
}

