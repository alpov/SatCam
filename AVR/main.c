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
#include <ctype.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/signature.h>
#include <util/delay.h>
#include "common.h"
#include "i2cmaster.h"
#include "bpsk.h"
#include "adf.h"
#include "meas.h"
#include "eeprom.h"
#include "uart.h"

FUSES = {
    .low = (FUSE_CKSEL3), // full-swing XTAL, 16k+65ms
    .high = (FUSE_SPIEN & FUSE_EESAVE), // preserve EEPROM
    .extended = (FUSE_BODLEVEL1), // BOD at 2.7V
};

/*----------------------------------------------------------------------------------------------------------------*/

volatile uint32_t ClockTimer = 0;


static CONFIG_STRUCT eeprom EEMEM = {
    .MaxMode = MODE_A,
    .PeriodsRX = { PER_RX_D_DEF, PER_RX_C_DEF, PER_RX_B_DEF, PER_RX_A_DEF }, // D to A
    .ProlongTX = { 0, PROL_TX_C_DEF, PROL_TX_B_DEF, PROL_TX_A_DEF }, // D to A
    .THRvoltage = THRV_DEF,
    .PSKdiv = 4,
    .TunerC = TUNER_C_DEF,
    .TunerL = TUNER_L_DEF,
    .SSTVen = 1,
};
CONFIG_STRUCT config;
volatile STATUS_STRUCT status;

/*----------------------------------------------------------------------------------------------------------------*/

void early_wdt_handler(void) __attribute__ ((naked,section(".init3")));
void early_wdt_handler(void)
{
    MCUSR = 0;
    wdt_enable(WDTO_4S);
}


static void TX_ON(void)
{
    if (status.TX) return;
    set_TXPWR(1); // PA on
    _delay_ms(2);
    TX_Enable(); // ADF load regs
    status.TX = true;
    _delay_ms(100);
    SupervisorTask();
    BPSK_SigStart();
    BPSK_SendChar('\f');
}


static void TX_OFF(void)
{
    if (!status.TX) return;
    BPSK_SigStop();
    TX_Disable(); // ADF disable
    set_TXPWR(0); // PA off
    status.TX = false;
}


static void RX_ON(void)
{

    if (status.RX) return;
    set_RXPWR(1); // RX on
    _delay_ms(1);
    status.RX = true;
}


static void RX_OFF(void)
{
    if (!status.RX) return;
    status.RX = false;
    set_RXPWR(0); // RX off
}


static void TunerUpdate(void)
{
    DAC_Write(0, config.TunerC); // channel A - capacitance
    DAC_Write(1, (config.TunerL & 0x01) ? 4095 : 0); // channel B - inductor 1 on/off
    DAC_Write(2, (config.TunerL & 0x02) ? 4095 : 0); // channel C - inductor 2 on/off
    DAC_Write(3, 0); // channel D - unused
}


static void SetMode(uint8_t req_mode)
{
    // check MaxMode in A..D
    if (config.MaxMode > MODE_A) config.MaxMode = MODE_A;

    if (config.MaxMode >= req_mode) {
        // requested mode is allowed, set to requested
        status.Mode = req_mode;
    } else {
        // requested mode over MaxMode, set to MaxMode
        status.Mode = config.MaxMode;
    }

    if (status.Mode == MODE_D) {
        // clear counters for mode D
        status.PeriodsSSTV_noRX = 0;
        status.PeriodsSSTV_keepRX = 0;
        status.PeriodsTX = 0;
    }
}


static void SaveValidatedConfig(void)
{
    // current mode validation
    SetMode(status.Mode);

    // check voltage threshold range, PSK divider in [2,4]
    if ((config.THRvoltage < (THRV_MIN - THRV_STEP)) || (config.THRvoltage > THRV_MAX)) config.THRvoltage = THRV_DEF;
    if (config.PSKdiv != 2 && config.PSKdiv != 4) config.PSKdiv = 4;

    // check number of RX enabled periods in 1..6
    if (config.PeriodsRX[MODE_A] < 1 || config.PeriodsRX[MODE_A] > 6) config.PeriodsRX[MODE_A] = 1;
    if (config.PeriodsRX[MODE_B] < 1 || config.PeriodsRX[MODE_B] > 6) config.PeriodsRX[MODE_B] = 1;
    if (config.PeriodsRX[MODE_C] < 1 || config.PeriodsRX[MODE_C] > 6) config.PeriodsRX[MODE_C] = 1;
    if (config.PeriodsRX[MODE_D] < 1 || config.PeriodsRX[MODE_D] > 6) config.PeriodsRX[MODE_D] = 1;

    // check number of TX prolognation periods in 0..5
    if (config.ProlongTX[MODE_A] > 5) config.ProlongTX[MODE_A] = 0;
    if (config.ProlongTX[MODE_B] > 5) config.ProlongTX[MODE_B] = 0;
    if (config.ProlongTX[MODE_C] > 5) config.ProlongTX[MODE_C] = 0;
    config.ProlongTX[MODE_D] = 0;

    // check antenna tuner values
    if (config.TunerC > 4095) config.TunerC = 4095;
    if (config.TunerL > 0x03) config.TunerL = 0x03;

    // store changed settings to AVR EEPROM
    eeprom_update_block(&config, &eeprom, sizeof(eeprom));
}




void uart_process_char(char c)
{
    switch (c) {
        case PSK_CMD_TX_NO_RX:
            if (status.Mode != MODE_D && status.SunWire) {
                // modes A,B,C - start TX, disable RX, set max number of SSTV periods
                status.PeriodsSSTV_noRX = MAX_SSTV_PERIODS;
                RX_OFF();
                TX_ON();
                uart_putc(PSK_RSP_ACK);
            } else {
                // deny in mode D
                uart_putc(PSK_RSP_DENIED);
            }
            break;
        case PSK_CMD_TX_KEEP_RX:
            if (status.Mode != MODE_D && status.SunWire) {
                // modes A,B,C - start TX, set max number of SSTV periods
                status.PeriodsSSTV_keepRX = MAX_SSTV_PERIODS;
                TX_ON();
                uart_putc(PSK_RSP_ACK);
            } else if (status.TX) {
                // mode D - acknowledge if TX is currently on
                uart_putc(PSK_RSP_ACK);
            } else {
                // otherwise deny in mode D
                uart_putc(PSK_RSP_DENIED);
            }
            break;
        case PSK_CMD_TX_IDLE:
            if (status.TX) {
                // acknowledge if TX is currently on
                uart_putc(PSK_RSP_ACK);
            } else {
                // otherwise deny
                uart_putc(PSK_RSP_DENIED);
            }
            break;
        case PSK_CMD_STOP_TX:
            // clear number of SSTV periods
            status.PeriodsSSTV_noRX = 0;
            status.PeriodsSSTV_keepRX = 0;
            break;
    }
}


void SunWireUpdate(void)
{
    bool sunwire_new = get_SUNWIRE();
    static bool sunwire_old = false;
    static uint32_t last_sun = 0;

    // check sun wire
    bool sun_fall = sunwire_old && !sunwire_new;
    bool sun_rise = !sunwire_old && sunwire_new;
    bool timeout = last_sun && (ClockTimer - last_sun > TSUNWIRE);

    if (sun_fall) {
        // falling edge
        status.SunWire = false;
        last_sun = ClockTimer;
    }
    else if (sun_rise || timeout) {
        // rising edge
        status.SunWire = true;
        last_sun = 0;
    }
    sunwire_old = sunwire_new;
}


bool SupervisorTask(void)
{
    // reset internal watchdog
    wdt_reset();

    // reset external watchdog
    static uint32_t last_wdr;
    static bool wdr_state = false;
    if (wdr_state && ClockTimer - last_wdr > 4) {
        set_HEARTBEAT(0);
        wdr_state = false;
        last_wdr = ClockTimer;
    }
    else if (!wdr_state && ClockTimer - last_wdr > TWATCHDOG) {
        if (!status.HeartbeatOff) { set_HEARTBEAT(1); }
        wdr_state = true;
        last_wdr = ClockTimer;
    }

    // handle UART communication with reentrancy check
    static volatile bool busy = false;
    if (!busy) {
        busy = true;
        uint16_t c = uart_getc();
        while (c < 0x100) {
            uart_process_char(c);
            c = uart_getc();
        }
        busy = false;
    }

    // PA temperature check
    static uint32_t last_adc;
    static bool T_TX_over = false;
    if (ClockTimer - last_adc > 10*4) {
        T_TX_over = (ADC_Read_T_TX() > THRT_DEF); // PA temperature check every 10sec
        if (T_TX_over) SetMode(MODE_D);
        last_adc = ClockTimer;
    }

    // battery voltage check
    uint16_t Voltage = ADC_Read_Vbat();
    if (Voltage < 100) Voltage = THRV_MAX * 10; // voltage <1.0V => measurement failed, use default max
    if (Voltage < ((uint16_t)((config.THRvoltage - 3*THRV_STEP) * 10))) { // volt < THRvoltage-3*step
        SetMode(MODE_D);
        return false; // kill TX immediately
    }
    switch (status.Mode) {
        case MODE_A:
            if (Voltage < ((uint16_t)(config.THRvoltage * 10))) SetMode(MODE_B); // volt < THRvoltage -> B
            break;
        case MODE_B:
            if (Voltage < ((uint16_t)((config.THRvoltage - 1*THRV_STEP) * 10))) SetMode(MODE_C); // volt < THRvoltage-1*step -> C
            else if (Voltage > ((uint16_t)((config.THRvoltage + 1*THRV_STEP) * 10))) SetMode(MODE_A); // volt > THRvoltage+1*step -> A
            break;
        case MODE_C:
            if (Voltage < ((uint16_t)((config.THRvoltage - 2*THRV_STEP) * 10))) SetMode(MODE_D); // volt < THRvoltage-2*step -> D
            else if (Voltage > ((uint16_t)((config.THRvoltage + 0*THRV_STEP) * 10))) SetMode(MODE_B); // volt > THRvoltage -> B
            break;
        case MODE_D:
            if (Voltage > ((uint16_t)((config.THRvoltage - 1*THRV_STEP) * 10)) && !T_TX_over) SetMode(MODE_C); // volt > THRvoltage-1*step && !T_TX_over -> C
            break;
    }
    return true; // continue with TX
}


/*----------------------------------------------------main----------------------------------------------------*/

int main(void)
{
    // init outputs
    DDRB |= _BV(PB0); set_TXPWR(0); // PA off
    DDRB |= _BV(PB3) | _BV(PB4) | _BV(PB5); // SCK, SDATA, SLE
    DDRC |= _BV(PC1); set_RXPWR(0); // RX off
    DDRC |= _BV(PC2); set_SHUTDOWN(0); // SHUTDOWN = SSTV off
    DDRC |= _BV(PC0); set_HEARTBEAT(0); // HEARTBEAT off

    // TIMER1 init
    TCNT1 = 0;
    OCR1A = (F_CPU / 1024) / 16 - 1; // sample inputs (3686400/1024)/fs(3600Hz max)
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS12) | _BV(CS10); // fc/1024, CTC
    TIMSK1 = _BV(OCIE1A);

    // init subsystems
    ADC_Init();
    BPSK_Init();
    i2c_init();
    uart_init(UART_BAUD_SELECT(9600,F_CPU));
    sei();

    // EEPROM_Erase();
    uint8_t RebootCnt = EEPROM_InitLoad();
    srand(ClockTimer);

    // init sun wire
    status.SunWire = true;
    SunWireUpdate();

    // read flags from AVR EEPROM and handle undefined values
    eeprom_read_block(&config, &eeprom, sizeof(config));
    SetMode(MODE_A);
    SaveValidatedConfig();
    set_SHUTDOWN(config.SSTVen); // enable/disable SSTV module

    // set stored DAC
    TunerUpdate();

    // send reboot message
    char s[32]; // 23 used
    sprintf_P(s, PSTR("\r\r" CALL " restart %u\r\f"), RebootCnt);
    TX_ON();
    BPSK_SendBuffer(s);
    TX_OFF();

    // always send telemetry and enable RX in 1st period after reboot
    status.PeriodNr--;

    // main loop
    while (1)
    {
        // wait for 20sec timer - TX sync
        do {
            SupervisorTask();
        } while (ClockTimer % TMSG != 0);

        // if sun wire not high, power off RX/TX and wait
        SunWireUpdate();
        if (!status.SunWire) {
            TX_OFF();
            RX_OFF();
            status.PeriodsSSTV_noRX = 0;
            status.PeriodsSSTV_keepRX = 0;
            status.PeriodsTX = 0;
            _delay_ms(500);
            continue;
        }

        // update PSK signal detect (with hysteresis)
        if (ADC_Read_PSK() > SENSE_THUP) status.SigPSK = true;
        else if (ADC_Read_PSK() < SENSE_THDWN) status.SigPSK = false;

        // get period number
        if (++status.PeriodNr >= ((status.Mode != MODE_D) ? TPER1 : TPER2)) {
            status.PeriodNr = 0;
            status.Period2SSTV = !status.Period2SSTV;
        }
        
        // TX time scheduler - before telemetry
        if (status.Mode != MODE_D && status.SigPSK) {
            // modes A,B,C - TX is enabled when PSK detected
            TX_ON();
            status.PeriodsTX = config.ProlongTX[status.Mode]+1;
        }
        else if (status.PeriodNr == 0) {
            // all modes - TX enabled during 1st period
            TX_ON();
        }
        else if (status.PeriodsTX == 0 && status.PeriodsSSTV_noRX == 0 && status.PeriodsSSTV_keepRX == 0) {
            // all modes, period counters for TX and SSTVs reached zero - disable TX
            TX_OFF();
        }

        // if RX is on, decode and execute command
        if (status.RX) {
        }

        // RX time scheduler - enable RX for specified periods or during TX or if CW command received, not if SSTV_noRX is non-zero
        if ((status.PeriodNr < config.PeriodsRX[status.Mode] || status.TX || status.PeriodsRX == 1) && status.PeriodsSSTV_noRX == 0) RX_ON();
        else RX_OFF();

        // SSTV scheduler
        bool sstv_req = (status.PeriodsSSTV_noRX > 0 || status.PeriodsSSTV_keepRX > 0 || status.PeriodsRX > 0); // SSTV ready to start?
        switch (status.Mode) {
            case MODE_A:
                // mode A - always transmit SSTV/Robot36 in 7/12 and 8/12
                if (status.PeriodNr == 0 && status.Period2SSTV && !sstv_req) {
                    TX_ON();
                    uart_putc(PSK_RSP_AUTO_CMD);
                    uart_putc(PSK_RSP_SSTV_36);
                    sstv_req = true;
                }
                // intentionally no break -> allow transmission in 2/12-5/12 with PSK activity
            case MODE_B:
                // mode B - when PSK activity
                if (status.PeriodNr == 1 && status.PeriodsTX >= 4 && !sstv_req) {
                    // transmit SSTV/MP73 in 2/6, 3/6, 4/6 and 5/6
                    uart_putc(PSK_RSP_AUTO_CMD);
                    uart_putc(PSK_RSP_SSTV_73);
                    sstv_req = true;
                }
                else if (status.PeriodNr == 1 && status.PeriodsTX >= 2 && !sstv_req) {
                    // transmit SSTV/Robot36 in 2/6 and 3/6
                    uart_putc(PSK_RSP_AUTO_CMD);
                    uart_putc(PSK_RSP_SSTV_36);
                    sstv_req = true;
                }
                break;
            case MODE_C:
            case MODE_D:
                // no auto SSTV
                break;
        }
        if (status.TX && !sstv_req) {
            // transmit telemetry if TX already on and no SSTV is running or just started
            uart_putc(PSK_RSP_AUTO_CMD);
            uart_putc(PSK_RSP_TLM);
        }

        // if TX is on and CW command not just received, then send telemetry data
        if (status.TX && status.PeriodsRX < 2) {
            EEPROM_SendTelemetry();
        } else {
            // wait to tick out from current 0.25sec
            _delay_ms(500);
        }

        // TX time scheduler - after telemetry
        if (status.PeriodsSSTV_noRX > 0) status.PeriodsSSTV_noRX--;
        if (status.PeriodsSSTV_keepRX > 0) status.PeriodsSSTV_keepRX--;

        if (status.PeriodsTX > 0) status.PeriodsTX--;
        else if (status.PeriodsSSTV_noRX == 0 && status.PeriodsSSTV_keepRX == 0) TX_OFF();

        if (status.PeriodsRX > 0) status.PeriodsRX--;

    } // main loop
}


/*----------------------------------------------------interrupts----------------------------------------------------*/

ISR(TIMER1_COMPA_vect)
{
    static uint8_t TimerDiv = 0;

    // save BPSK signal detect sample
    if (TimerDiv != 0) TimerDiv--;
    else {
        // 4 times per second
        status.SignalSense <<= 1;
        if (get_SQUELCH_PSK() == 0 && status.RX) status.SignalSense |= 1;
        TimerDiv = 3;

        ClockTimer++;
        if (ClockTimer >= CLOCK_INVALID * TMSG) ClockTimer = 0;
    }

}

