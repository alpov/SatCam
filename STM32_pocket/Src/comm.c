/*************************************************************************
 *
 * SatCam - Camera Module for PSAT-2
 * Copyright (c) 2015-2017 Ales Povalac <alpov@alpov.net>
 * Dept. of Radio Electronics, Brno University of Technology
 *
 * This work is licensed under the terms of the MIT license
 *
 *************************************************************************/

#define _GNU_SOURCE
#include <stdarg.h>
#include <ctype.h>
#include "cube.h"
#include "eeprom.h"
#include "comm.h"

// emulated I2C RAM
static uint8_t *cfg_memory = (uint8_t*)(&config);
static uint8_t cfg_addr;        // index of current RAM cell
static uint8_t cfg_first = 1;   // first byte --> new offset

static uint32_t last_i2c_access;

IMPORT_BIN("Inc/lux.bin", uint16_t, LuxTable);
#define ADC_LUX_THRESHOLD 800


void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
    set_led_yellow(false);
    HAL_I2C_EnableListen_IT(hi2c); // slave is ready again
}


void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
    set_led_yellow(true);
    if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
        cfg_first = 1;

        // I2C write - addressing byte
        HAL_I2C_Slave_Sequential_Receive_IT(hi2c, &cfg_addr, 1, I2C_NEXT_FRAME);
    } else {
        // addressing byte received, restart condition - need to reset I2C state
        if (hi2c->State == HAL_I2C_STATE_BUSY_RX_LISTEN) hi2c->State = HAL_I2C_STATE_LISTEN;

        // I2C read - data memory
        HAL_I2C_Slave_Sequential_Transmit_IT(hi2c, &cfg_memory[cfg_addr], CONFIG_SIZE_ALL-cfg_addr, I2C_NEXT_FRAME);
    }
    last_i2c_access = HAL_GetTick();
}


void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    static uint8_t dummy;

    if (cfg_first && cfg_addr < (service_enable ? CONFIG_SIZE_ALL : CONFIG_SIZE_GENERAL)) {
        // addressing byte (cfg_addr) received, address valid
        // I2C write - data memory
        HAL_I2C_Slave_Sequential_Receive_IT(hi2c, &cfg_memory[cfg_addr], (service_enable ? CONFIG_SIZE_ALL : CONFIG_SIZE_GENERAL)-cfg_addr, I2C_NEXT_FRAME);
    } else {
        // I2C write - dummy
        HAL_I2C_Slave_Sequential_Receive_IT(hi2c, &dummy, 1, I2C_NEXT_FRAME);
    }
    cfg_first = 0;
}


void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    static uint8_t dummy = 0xFF;

    // I2C read - dummy
    HAL_I2C_Slave_Sequential_Transmit_IT(hi2c, &dummy, 1, I2C_NEXT_FRAME);
}


void comm_init(void)
{
    HAL_I2C_EnableListen_IT(&hi2c2);
}


void comm_cmd_task(void)
{
    if (config.command && (HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_LISTEN)) {
        // handle command
        syslog_event(LOG_CMD_HANDLED);
        cmd_handler(config.command);
        config.command = 0;
    }

    if (config.sys_i2c_watchdog && (HAL_GetTick() > last_i2c_access + (config.sys_i2c_watchdog * 1000UL))) {
        // no I2C communication for defined time, trigger reboot
        syslog_event(LOG_I2C_WDR);
        NVIC_SystemReset(); // trigger NVIC system reset
    }

    if (config.sys_autoreboot && (HAL_GetTick() > (config.sys_autoreboot * 60000UL))) {
        // autoreboot
        syslog_event(LOG_AUTOREBOOT);
        NVIC_SystemReset(); // trigger NVIC system reset
    }
}


static uint16_t adc_read(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {
        .Channel = channel,
        .Rank = 1,
        .SamplingTime = ADC_SAMPLETIME_144CYCLES,
    };
    uint32_t avg = 0;

    /* configure ADC channel, start and add values to average */
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);

    for (uint8_t i = 0; i < ADC_AVERAGE; i++) {
        HAL_ADC_PollForConversion(&hadc1, 100);
        __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC);
        avg += HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    return (avg / ADC_AVERAGE); // compute average
}


uint16_t adc_read_voltage()
{
    return adc_read(ADC_CHANNEL_8) * VDD_VALUE * (47+10) / 10 / 4096;
}


uint16_t adc_read_light()
{
    uint8_t range = 0;
    uint16_t adc;

    HAL_GPIO_WritePin(SENS_RNG1_GPIO_Port, SENS_RNG1_Pin, 1); // range 1 - 10k divider
    HAL_Delay(2);
    adc = __USAT(adc_read(ADC_CHANNEL_0) >> 2, 10);
    HAL_GPIO_WritePin(SENS_RNG1_GPIO_Port, SENS_RNG1_Pin, 0);

    if (adc > ADC_LUX_THRESHOLD) {
        HAL_GPIO_WritePin(SENS_RNG2_GPIO_Port, SENS_RNG2_Pin, 1); // range 2 - 470R divider
        HAL_Delay(2);
        adc = __USAT(adc_read(ADC_CHANNEL_0) >> 2, 10);
        range = 1;
        HAL_GPIO_WritePin(SENS_RNG2_GPIO_Port, SENS_RNG2_Pin, 0);
    }

    uint16_t lux = LuxTable[range ? (adc+1024) : (adc)];
    // printf_debug("Light sensor range %d, ADC=%d, R=%uohm, E=%02ulx%u", range+1, adc*4, (1024-adc)*(range?470:10000)/adc, lux%100,lux/100);

    uint32_t lux_expand = lux%100;
    for (uint8_t i = 0; i < lux/100; i++) lux_expand *= 10;

    lux_expand *= config.light_cal;
    lux_expand /= 512;
    // printf_debug("Light sensor corrected %ulx", lux_expanded);

    lux = 0;
    while (lux_expand > 100) {
        lux += 100;
        lux_expand /= 10;
    }
    lux += lux_expand;

    return lux;
}


int16_t adc_read_temperature()
{
    uint32_t tempAVG = adc_read(ADC_CHANNEL_TEMPSENSOR);

    /* Correction factor if VDD <> 3V3 */
    tempAVG = tempAVG * VDD_VALUE / 3300;

    /* Calculate temperature in °C from ADC value; AN3964 - Temperature_sensor */
    int temperature_C = ((int32_t)(tempAVG) - (int32_t)(TS_CAL_1));
    temperature_C = temperature_C * (int32_t)(HOT_CAL_TEMP - COLD_CAL_TEMP);
    temperature_C = temperature_C / (int32_t)(TS_CAL_2 - TS_CAL_1);
    temperature_C = temperature_C + COLD_CAL_TEMP;
    return temperature_C;
}
