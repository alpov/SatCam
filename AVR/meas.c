/*************************************************************************
 *
 * PSK Transponder Module for PSAT-2
 * Copyright (c) 2014-2017 OK2ALP, OK2PNQ, OK2CPV
 * Dept. of Radio Electronics, Brno University of Technology
 *
 * This work is licensed under the terms of the MIT license
 *
 *************************************************************************/

#include <avr/io.h>
#include "common.h"
#include "i2cmaster.h"
#include "meas.h"


static uint16_t AD7417_Read(uint8_t channel)
{
    if (i2c_start(AD7417 + I2C_WRITE)) return 0;        // set device address and write mode
    i2c_write(0x01);                                    // set address pointer - Config reg
    i2c_write((channel << 5));                          // select ADC channel
    if (i2c_rep_start(AD7417 + I2C_WRITE)) return 0;    // set device address and write mode
    i2c_write((channel == ADC_T_RX) ? 0x00 : 0x04);     // set address pointer - ADC reg or TEMP reg
    if (i2c_rep_start(AD7417 + I2C_READ)) return 0;     // read mode
    uint16_t result;
    result = i2c_readAck() << 8;
    result |= i2c_readNak();
    if (i2c_rep_start(AD7417 + I2C_WRITE)) return 0;    // set device address and write mode
    i2c_write(0x01);                                    // set address pointer - Config reg
    i2c_write(0x01);                                    // pwr down
    i2c_stop();                                         // stop
    return (result >> 6);
}


int8_t ADC_Read_T_TX(void)
{
    if (i2c_start(AD7415 + I2C_READ)) return 0;
    uint16_t result = i2c_readAck() << 8;
    result |= i2c_readNak();
    i2c_stop();
    return (result >> 8); // [°C]
}


int8_t ADC_Read_T_RX(void)
{
    return (AD7417_Read(ADC_T_RX) >> 2); // [°C]
}


uint16_t ADC_Read_Vbat(void)
{
    ADCSRA |= _BV(ADSC);
    do {} while (bit_is_set(ADCSRA, ADSC));
    return (ADC /* * 3300UL * 147 / 47 / 1024 / 10 */); // [10mV]
}


uint16_t ADC_Read_5V(void)
{
    return (AD7417_Read(ADC_5V) /* * 2500UL * 409 / 100 / 1024 / 10 */); // [10mV]
}


uint16_t ADC_Read_Ic(void)
{
    return (AD7417_Read(ADC_IC) /* * 2500UL * 409 / 100 / 1024 / 10 */); // [1mA]
}


uint16_t ADC_Read_AGC(void)
{
/*
    uint16_t adcAGClevel = AD7417_Read(ADC_AGC);
    if (adcAGClevel > 776) adcAGClevel = 776;

    uint16_t result;
    if (adcAGClevel > 511) result = (776 - adcAGClevel) / 8;
    else result = 545 - adcAGClevel;

    if (result > 99) result = 99;

    return result; // 0-99%
*/
    return (AD7417_Read(ADC_AGC));
}


uint8_t ADC_Read_PSK(void)
{
    uint32_t ss = status.SignalSense;
    uint8_t count = 0;

    for (uint8_t i = 0; i < 32; i++) {
        if (ss & 1) count++;
        ss >>= 1;
    }

    return (99 * count) / 32; // 0-99%
}


void ADC_Init(void)
{
    ADMUX = _BV(REFS0) | ADC_VBAT;
    ADCSRA = _BV(ADEN) | _BV(ADPS0) | _BV(ADPS1) | _BV(ADPS2); // clk/128, single conv, ADC enable
}


uint8_t DAC_Write(uint8_t channel, uint16_t value)
{
    if (i2c_start(MCP4728 + I2C_WRITE)) return 0;
    i2c_write(((channel & 0x03) << 1) | 0x40); // Multi-write, UDAC=0
    i2c_write(((value >> 8) & 0x0F) | 0x90); // Vref=1, PD=00, Gx=1
    i2c_write(((value >> 0) & 0xFF));
    i2c_stop();
    return 1;
}

