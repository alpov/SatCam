/*************************************************************************
 *
 * PSK Transponder Module for PSAT-2
 * Copyright (c) 2014-2017 OK2ALP, OK2PNQ, OK2CPV
 * Dept. of Radio Electronics, Brno University of Technology
 *
 * This work is licensed under the terms of the MIT license
 *
 *************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include "common.h"
#include "i2cmaster.h"
#include "meas.h"
#include "bpsk.h"
#include "eeprom.h"

static uint16_t AddressCounter = 0;
static uint8_t RebootCnt = 0;


uint8_t GetCharHi5(uint16_t Number)
{
    Number &= 1023;
    uint8_t temp = (Number >> 5);    // bits 9..5
    if (temp < 26) return temp + 97; // a..z for 0..25
    else return temp + 39;           // A..F for 26..31
}


uint8_t GetCharLo5(uint16_t Number)
{
    Number &= 1023;
    uint8_t temp = (Number & 31);    // bits 4..0
    if (temp < 26) return temp + 97; // a..z for 0..25
    else return temp + 39;           // A..F for 26..31
}


void EncCharFull(uint16_t Number, char **p)
{
#if 1
    (*p)[0] = GetCharHi5(Number);
    (*p)[1] = GetCharLo5(Number);
    *p += 2;
#else
    *p += sprintf_P(*p, PSTR("%u "), Number & 0x03FF);
#endif
}


static void EncCharDiff(int16_t NumberNew, int16_t NumberOld, char **p)
{
    int16_t diff = NumberNew - NumberOld + 14; // no change is 14 = 'o'

    if ((diff > 31) || (diff < 0)) {
        (*p)[0] = ' ';
        *p += 1;
        EncCharFull(NumberNew, p);  // out of range, encode full symbol preceded by space
    } else {
        (*p)[0] = GetCharLo5(diff); // encode only the difference
        *p += 1;
    }
}


char GetModeChar(uint8_t mode)
{
    switch (mode) {
        case MODE_A: return 'A'; break;
        case MODE_B: return 'B'; break;
        case MODE_C: return 'C'; break;
        case MODE_D: return 'D'; break;
        default: return 'X'; break;
    }
}


static bool EEPROM_ReadFrame(uint16_t addr, EEPROM_FRAME *frame)
{
    uint8_t *p = (uint8_t*)(frame);

    if (i2c_start(M24LC64 + I2C_WRITE)) return false;
    i2c_write((uint8_t)(addr >> 8));
    i2c_write((uint8_t)(addr));
    if (i2c_rep_start(M24LC64 + I2C_READ)) return false;
    for (uint8_t i = 0; i < sizeof(EEPROM_FRAME)-1; i++) *p++ = i2c_readAck();  // read frame with ACK
    *p = i2c_readNak(); // last byte NAK
    i2c_stop();
    return true;
}


static bool EEPROM_WriteFrame(uint16_t addr, EEPROM_FRAME *frame)
{
    uint8_t *p = (uint8_t*)(frame);

    if (i2c_start(M24LC64 + I2C_WRITE)) return false;
    i2c_write((uint8_t)(addr >> 8));
    i2c_write((uint8_t)(addr));
    for (uint8_t i = 0; i < sizeof(EEPROM_FRAME); i++) i2c_write(*p++);
    i2c_stop();
    return true;
}


void EEPROM_SendTelemetry(void)
{
    char Buffer[BUFFER_LEN]; // 58 used
    char *p = Buffer;
    EEPROM_FRAME nframe, oframe;

    // prepare frame data
    memset(&nframe, 0x00, sizeof(nframe));
    nframe.ClockTimer = ClockTimer / TMSG;
    nframe.Mode = status.Mode;
    nframe.RebootCnt = RebootCnt;
    nframe.val_PSK = ADC_Read_PSK();
    nframe.val_AGC = ADC_Read_AGC();
    nframe.val_Vbat = ADC_Read_Vbat();
    nframe.val_5V = ADC_Read_5V();
    nframe.val_Ic = ADC_Read_Ic();
    nframe.val_T_RX = ADC_Read_T_RX();
    nframe.val_T_TX = ADC_Read_T_TX();

    // encode frame
    p += sprintf_P(p, PSTR("\r" CALL " %c "), GetModeChar(nframe.Mode));
    EncCharFull(nframe.ClockTimer >> 10, &p);
    EncCharFull(nframe.ClockTimer, &p);
    *p++ = ' ';
    EncCharFull(nframe.RebootCnt, &p);
    EncCharFull(nframe.val_PSK, &p);
    EncCharFull(nframe.val_AGC, &p);
    EncCharFull(nframe.val_Vbat, &p);
    EncCharFull(nframe.val_5V, &p);
    EncCharFull(nframe.val_Ic, &p);
    EncCharFull((int16_t)nframe.val_T_RX, &p);
    // EncCharFull((int16_t)nframe.val_T_TX, &p);
    *p++ = ' ';

    // extra TLM - status
    EncCharFull(((int16_t)(status.PeriodNr & 0x1f) << 5) | ((status.PeriodsSSTV_noRX + status.PeriodsSSTV_keepRX) & 0x1f), &p);
    EncCharFull(((int16_t)(status.PeriodsRX & 0x1f) << 5) | (status.PeriodsTX & 0x1f), &p);
    *p++ = ' ';

    // load random record in last 120 minutes
    uint16_t addr = AddressCounter - sizeof(EEPROM_FRAME) * (rand() / (RAND_MAX / (120*3)));
    if (addr > MEMLEN) addr += sizeof(EEPROM_FRAME);
    if (EEPROM_ReadFrame(addr, &oframe)) {
        // encode frame
        p += sprintf_P(p, PSTR("%c "), GetModeChar(oframe.Mode));
        EncCharFull(oframe.ClockTimer >> 10, &p);
        EncCharFull(oframe.ClockTimer, &p);
        *p++ = ' ';
        EncCharFull(oframe.RebootCnt, &p);
        EncCharFull(oframe.val_PSK, &p);
        EncCharFull(oframe.val_AGC, &p);
        EncCharFull(oframe.val_Vbat, &p);
        EncCharFull(oframe.val_5V, &p);
        EncCharFull(oframe.val_Ic, &p);
        EncCharFull((int16_t)oframe.val_T_RX, &p);
        // EncCharFull((int16_t)oframe.val_T_TX, &p);
        *p++ = ' ';
    }
    *p++ = '\0';

    // store frame to EEPROM
    AddressCounter += sizeof(nframe);
    if (AddressCounter >= MEMLEN) AddressCounter = 0;
    EEPROM_WriteFrame(AddressCounter, &nframe);

    // send buffer
    BPSK_SendBuffer(Buffer);
}


void EEPROM_SendMemory(uint16_t include)
{
    char Buffer[BUFFER_LEN]; // 267 max
    char *p = Buffer;
    EEPROM_FRAME nframe, oframe;
    uint16_t addr = AddressCounter;
    uint8_t diff_cnt = 0;

    BPSK_SetSpeedDiv(config.PSKdiv);
    p += sprintf_P(p, PSTR("\f\f\r" CALL "\r"));

    for (uint16_t i = 0; i < MEMLEN/sizeof(EEPROM_FRAME); i++) {
        if (!EEPROM_ReadFrame(addr, &nframe)) goto tx_end;
        if (nframe.ClockTimer == CLOCK_INVALID) goto tx_end;

        if (diff_cnt == 0) {
            // full frame
            EncCharFull(nframe.ClockTimer >> 10, &p);
            EncCharFull(nframe.ClockTimer, &p);
            if (include & INCL_MODE)   { *p++ = ' '; EncCharFull(GetModeChar(nframe.Mode) - 'A', &p); }
            if (include & INCL_REBOOT) { *p++ = ' '; EncCharFull(nframe.RebootCnt, &p); }
            if (include & INCL_PSK)    { *p++ = ' '; EncCharFull(nframe.val_PSK, &p); }
            if (include & INCL_AGC)    { *p++ = ' '; EncCharFull(nframe.val_AGC, &p); }
            if (include & INCL_Vbat)   { *p++ = ' '; EncCharFull(nframe.val_Vbat, &p); }
            if (include & INCL_5V)     { *p++ = ' '; EncCharFull(nframe.val_5V, &p); }
            if (include & INCL_Ic)     { *p++ = ' '; EncCharFull(nframe.val_Ic, &p); }
            if (include & INCL_T_RX)   { *p++ = ' '; EncCharFull((int16_t)nframe.val_T_RX, &p); }
            if (include & INCL_T_TX)   { *p++ = ' '; EncCharFull((int16_t)nframe.val_T_TX, &p); }

            diff_cnt = 7;
        } else {
            // diff frame
            EncCharDiff(nframe.ClockTimer, oframe.ClockTimer, &p);
            if (include & INCL_MODE)   EncCharDiff(nframe.Mode, oframe.Mode, &p);
            if (include & INCL_REBOOT) EncCharDiff(nframe.RebootCnt, oframe.RebootCnt, &p);
            if (include & INCL_PSK)    EncCharDiff(nframe.val_PSK, oframe.val_PSK, &p);
            if (include & INCL_AGC)    EncCharDiff(nframe.val_AGC, oframe.val_AGC, &p);
            if (include & INCL_Vbat)   EncCharDiff(nframe.val_Vbat, oframe.val_Vbat, &p);
            if (include & INCL_5V)     EncCharDiff(nframe.val_5V, oframe.val_5V, &p);
            if (include & INCL_Ic)     EncCharDiff(nframe.val_Ic, oframe.val_Ic, &p);
            if (include & INCL_T_RX)   EncCharDiff(nframe.val_T_RX, oframe.val_T_RX, &p);
            if (include & INCL_T_TX)   EncCharDiff(nframe.val_T_TX, oframe.val_T_TX, &p);

            if (--diff_cnt == 0) {
                p += sprintf_P(p, PSTR("\r"));
                if (!BPSK_SendBuffer(Buffer)) {
                    p = Buffer;
                    p += sprintf_P(p, PSTR("error"));
                    goto tx_end;
                }
                p = Buffer;
            }
        }

        memcpy(&oframe, &nframe, sizeof(EEPROM_FRAME));
        if (addr < sizeof(EEPROM_FRAME)) addr = MEMLEN;
        addr -= sizeof(EEPROM_FRAME);
    }

tx_end:
    p += sprintf_P(p, PSTR("\r" CALL "\r\f"));
    BPSK_SendBuffer(Buffer);
    BPSK_SetSpeedDiv(1);

    sprintf_P(Buffer, PSTR("\f\f"));
    BPSK_SendBuffer(Buffer);
}


uint8_t EEPROM_InitLoad(void)
{
    EEPROM_FRAME nframe, lframe;
    uint16_t laddr = 0;

    memset(&lframe, 0x00, sizeof(EEPROM_FRAME));

    for (uint16_t i = 0; i < MEMLEN/sizeof(EEPROM_FRAME); i++) {
        uint16_t naddr = i * sizeof(EEPROM_FRAME);
        bool ok = EEPROM_ReadFrame(naddr, &nframe);
        if (ok && nframe.ClockTimer != CLOCK_INVALID && (nframe.ClockTimer > lframe.ClockTimer || nframe.ClockTimer == 0)) {
            memcpy(&lframe, &nframe, sizeof(EEPROM_FRAME));
            laddr = naddr;
        }
        wdt_reset();
    }

    AddressCounter = laddr;
    ClockTimer = (uint32_t)(lframe.ClockTimer) * TMSG;
    RebootCnt = lframe.RebootCnt + 1;
    return RebootCnt;
}


void EEPROM_Erase(void)
{
    EEPROM_FRAME frame;
    uint16_t addr = 0;

    // compile-time check of EEPROM frame size
    // https://scaryreasoner.wordpress.com/2009/02/28/checking-sizeof-at-compile-time/
    (void)sizeof(char[1 - 2*!(sizeof(EEPROM_FRAME) == 16)]);

    memset(&frame, 0xFF, sizeof(EEPROM_FRAME));
    for (uint16_t i = 0; i < MEMLEN/sizeof(EEPROM_FRAME); i++) {
        EEPROM_WriteFrame(addr, &frame);
        addr += sizeof(frame);

        // wait while busy
        i2c_start_wait(M24LC64 + I2C_WRITE);
        i2c_stop();

        wdt_reset();
    }
}

