#ifndef _COMMON_H
#define _COMMON_H

#include <stdbool.h>

#define CALL "PSAT-2"

// ---------- threshold
#define TMSG        (20 * 4UL)  // ticks => every 20sec
#define TPER1       6           // 2min
#define TPER2       9           // 3min
#define TWATCHDOG   (TMSG)      // 20 sec
#define TSUNWIRE    (30 * 60*4UL) // 30 min

#define THRV_STEP   2
#define THRV_MAX    84
#define THRV_MIN    44
#define THRV_DEF    64

#define THRT_DEF    80

#define PER_RX_A_DEF    6
#define PER_RX_B_DEF    6
#define PER_RX_C_DEF    3
#define PER_RX_D_DEF    2

#define PROL_TX_A_DEF   1
#define PROL_TX_B_DEF   1
#define PROL_TX_C_DEF   1

#define TUNER_L_DEF     1
#define TUNER_C_DEF     1600
// ---------- threshold end

// BPSK sense
#define SENSE_THUP  21
#define SENSE_THDWN 10

// Mode
#define MODE_A  3
#define MODE_B  2
#define MODE_C  1
#define MODE_D  0

#define MAX_SSTV_PERIODS    8   // 160sec

// commands between SSTV and PSK boards
#define PSK_CMD_TX_NO_RX    'B'
#define PSK_CMD_TX_KEEP_RX  'K'
#define PSK_CMD_TX_IDLE     'I'
#define PSK_CMD_STOP_TX     'E'
#define PSK_RSP_ACK         'a'
#define PSK_RSP_DENIED      'x'
#define PSK_RSP_UPLINK_CMD  'm'
#define PSK_RSP_AUTO_CMD    'n'
#define PSK_RSP_SSTV_36     '3'
#define PSK_RSP_SSTV_73     '7'
#define PSK_RSP_TLM         't'

typedef struct {
    uint8_t MaxMode;
    uint8_t PeriodsRX[4];
    uint8_t ProlongTX[4];
    uint8_t THRvoltage;
    uint8_t PSKdiv;
    uint16_t TunerC;
    uint8_t TunerL;
    uint8_t SSTVen;
} CONFIG_STRUCT;

typedef struct {
    uint8_t Mode;
    uint8_t PeriodNr;
    uint8_t Period2SSTV;
    uint8_t TX;
    uint8_t RX;
    uint8_t SunWire;
    uint16_t BitIndex;
    uint32_t SignalSense;
    uint8_t SigPSK;
    uint8_t PeriodsTX;
    uint8_t PeriodsRX;
    uint8_t PeriodsSSTV_noRX;
    uint8_t PeriodsSSTV_keepRX;
    uint8_t HeartbeatOff;
} STATUS_STRUCT;

// MCU pins
#define DA0                 PD2
#define set_DA(_x)          PORTD = ((_x) << DA0) & (_BV(DA0+0) | _BV(DA0+1) | _BV(DA0+2) | _BV(DA0+3) | _BV(DA0+4))
#define set_SCLK(_x)        if (_x) PORTB |= _BV(PB5); else PORTB &= ~_BV(PB5)
#define set_SDATA(_x)       if (_x) PORTB |= _BV(PB3); else PORTB &= ~_BV(PB3)
#define set_SLE(_x)         if (_x) PORTB |= _BV(PB4); else PORTB &= ~_BV(PB4)
#define set_TXPWR(_x)       if (_x) PORTB |= _BV(PB0); else PORTB &= ~_BV(PB0)
#define set_RXPWR(_x)       if (_x) PORTC |= _BV(PC1); else PORTC &= ~_BV(PC1)
#define set_SHUTDOWN(_x)    if (_x) PORTC |= _BV(PC2); else PORTC &= ~_BV(PC2)
#define set_HEARTBEAT(_x)   if (_x) PORTC |= _BV(PC0); else PORTC &= ~_BV(PC0)
#define get_SQUELCH_PSK()   bit_is_set(PINB, PB1)
#define get_UPLINK_CW()     bit_is_set(PINB, PB2)
#define get_SUNWIRE()       bit_is_set(PINC, PC3)

extern bool SupervisorTask(void);

extern volatile uint32_t ClockTimer;

extern CONFIG_STRUCT config;
extern volatile STATUS_STRUCT status;

#endif
