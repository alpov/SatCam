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
#include <util/delay.h>
#include "common.h"
#include "adf.h"

static void TX_SetRegisterValue(uint32_t Data)
{
    for (uint8_t i = 0; i < 32; i++) {
        set_SDATA(Data & 0x80000000);
        set_SCLK(0);  //clk 0
        _delay_us(2);
        set_SCLK(1); //clk 1
        Data <<= 1;
    }
    set_SCLK(0);  //clk 0
    _delay_us(2);
    set_SLE(1);
    _delay_us(10);
    set_SLE(0);
}

void TX_Enable(void)
{
    TX_SetRegisterValue(
        ((uint32_t) 0) |                //addr - REG0
        ((uint32_t) ((FRQ_ERR_CORR) & 0x07FF) << 2) |//freq error correction
        ((uint32_t) R_CNTR << 13) |     //R
        ((uint32_t) 0 << 17) |          //xtal doubler enable
        ((uint32_t) 1 << 18) |          //xtal osc enable
        ((uint32_t) 0b1000 << 19) |     //clkout default div 16
        ((uint32_t) VCO_ADJ << 23) |    //vco adjust
        ((uint32_t) OUT_DIV << 25));    //div output
    _delay_us(10);
    TX_SetRegisterValue(
        ((uint32_t) 1) |                //addr - REG1
        ((uint32_t) N_CNTR_FRAC << 2) | //fractional N
        ((uint32_t) N_CNTR_INT << 14)); //integer N
    _delay_us(10);
    TX_SetRegisterValue(
        ((uint32_t) 2) |                //addr - REG2
        ((uint32_t) PA_LEVEL << 5));    //PA level
    _delay_us(10);
    TX_SetRegisterValue(
        ((uint32_t) 3) |                //addr - REG3
        ((uint32_t) 0b11 << 2) |        //PLL enable, PA enable
        ((uint32_t) CP_CURRENT << 6) |  //charge pump current
        ((uint32_t) BLEED << 8) |       //bleed
        ((uint32_t) 0 << 10) |          //VCO on0/off1
        ((uint32_t) VCO_BIAS << 16) |   //VCO bias
        ((uint32_t) PA_BIAS << 20));    //PA bias
}

void TX_Disable(void)
{
    TX_SetRegisterValue(
        ((uint32_t) 3) |        //addr - REG3
        ((uint32_t) 0b00 << 2) |//PLL, PA disable
        ((uint32_t) 1 << 10));  //VCO off
}

