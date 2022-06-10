/*************************************************************************
 *
 * SatCam - Camera Module for PSAT-2
 * Copyright (c) 2015-2017 Ales Povalac <alpov@alpov.net>
 * Dept. of Radio Electronics, Brno University of Technology
 *
 * This work is licensed under the terms of the MIT license
 *
 *************************************************************************/

#include "cube.h"
#include "eeprom.h"
#include "m25p16.h"


static bool flash_fail = false;

static void flash_spi_write(uint8_t *buffer, uint16_t length, bool keep_nss)
{
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, 0);
    HAL_SPI_Transmit_DMA(&hspi2, buffer, length);
    while (hspi2.State == HAL_SPI_STATE_BUSY_TX) {
        /* enter SLEEP mode for next 1ms if transferring more than 4 bytes */
        if (length > 4) HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    }
    if (!keep_nss) HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, 1);
}


static void flash_spi_read(uint8_t *buffer, uint16_t length)
{
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, 0);
    HAL_SPI_Receive_DMA(&hspi2, buffer, length);
    while (hspi2.State == HAL_SPI_STATE_BUSY_RX) {
        /* enter SLEEP mode for next 1ms if transferring more than 4 bytes */
        if (length > 4) HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    }
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, 1);
}


static void flash_write_enable(void)
{
    const uint8_t cmd = 0x06;
    flash_spi_write((uint8_t*)(&cmd), 1, false);
    /* t_SHSL - S# deselect time min. 100ns; 8 NOPs for 80MHz core */
    for (uint8_t delay = 0; delay < 8; delay++) __NOP();
}


static uint8_t flash_read_status(void)
{
    const uint8_t cmd = 0x05;
    uint8_t status;
    flash_spi_write((uint8_t*)(&cmd), 1, true);
    flash_spi_read(&status, 1);
    return status;
}


static bool flash_wait_wip(uint32_t timeout)
{
    uint32_t tickstart = HAL_GetTick();
    bool wip; // write in progress
    do {
        if (timeout > 1000) {
            /* insert SLEEPing delay and watchdog reset for longer timeouts */
            HAL_Delay(50);
            HAL_IWDG_Refresh(&hiwdg); // 50ms period
        }
        wip = flash_read_status() & 0x01;
    } while (wip && (HAL_GetTick() - tickstart < timeout));
    if (wip) syslog_event(LOG_FLASH_TIMEOUT);
    return !wip;
}


bool flash_init(void)
{
    const uint8_t cmd = 0x9F;
    uint8_t id[3];

    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, 1);
    __HAL_SPI_ENABLE(&hspi2);
    flash_fail = false;

    for (uint8_t i = 0; i < M25P16_INIT_RETRY; i++) {
        HAL_Delay(2);
        flash_spi_write((uint8_t*)(&cmd), 1, true);
        flash_spi_read(id, 3);
        if (id[0] == M25P16_ID_MANUFACTURER && id[1] == M25P16_ID_DEVICE_HI && id[2] == M25P16_ID_DEVICE_LO) return true;
        if (id[0] == AT25SF161_ID_MANUFACTURER && id[1] == AT25SF161_ID_DEVICE_HI && id[2] == AT25SF161_ID_DEVICE_LO) return true;
        if (id[0] == W25Q16JV_ID_MANUFACTURER && id[1] == W25Q16JV_ID_DEVICE_HI && id[2] == W25Q16JV_ID_DEVICE_LO) return true;
        syslog_event(LOG_FLASH_INIT_ERROR);
    }
    flash_fail = true;
    return false;
}


void flash_read(uint32_t addr, uint8_t *buffer, uint16_t length)
{
    uint32_t cmd = __REV((0x03 << 24) | (addr & 0x001FFFFF));
    if (flash_fail) return;
    flash_spi_write((uint8_t*)(&cmd), 4, true);
    flash_spi_read(buffer, length);
}


bool flash_program_page(uint32_t addr, uint8_t *buffer)
{
    uint32_t cmd = __REV((0x02 << 24) | (addr & 0x001FFF00));
    if (flash_fail) return false;
    flash_write_enable();
    flash_spi_write((uint8_t*)(&cmd), 4, true);
    flash_spi_write(buffer, 0x100, false);
    return flash_wait_wip(M25P16_TIMEOUT_PAGE);
}


bool flash_program(uint32_t addr, uint8_t *buffer, uint16_t length)
{
    int32_t count = length;
    if (flash_fail) return false;

    while (count > 0) {
        if (!flash_program_page(addr, buffer)) return false;
        addr += 0x100;
        buffer += 0x100;
        count -= 0x100;
    }
    return true;
}


bool flash_erase_sector(uint32_t addr)
{
    uint32_t cmd = __REV((0xD8 << 24) | (addr & 0x001F0000));
    if (flash_fail) return false;
    flash_write_enable();
    flash_spi_write((uint8_t*)(&cmd), 4, false);
    return flash_wait_wip(M25P16_TIMEOUT_SECTOR);
}


bool flash_erase_bulk(void)
{
    const uint8_t cmd = 0xC7;
    if (flash_fail) return false;
    flash_write_enable();
    flash_spi_write((uint8_t*)(&cmd), 1, false);
    return flash_wait_wip(M25P16_TIMEOUT_BULK);
}

