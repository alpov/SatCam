#ifndef _EEPROM_H
#define _EEPROM_H

// I2C
#define M24LC64 0b10100000

// telemetry buffer length
#define BUFFER_LEN  350

// data to include in download
#define INCL_MODE   0x0001
#define INCL_REBOOT 0x0002
#define INCL_PSK    0x0004
#define INCL_AGC    0x0008
#define INCL_Vbat   0x0010
#define INCL_5V     0x0020
#define INCL_Ic     0x0040
#define INCL_T_RX   0x0080
#define INCL_T_TX   0x0100

// EEPROM structure
#define MEMLEN          8192
#define CLOCK_INVALID   0x000FFFFF

typedef struct {
    uint32_t ClockTimer:20; // 20bit clock per 20sec ~ 242 days turnaround
    uint32_t __dummy_ClockTimer:4;
    uint8_t Mode;           // 8bit mode
    uint8_t RebootCnt;      // 8bit reboot counter
    uint8_t val_PSK;        // PSK signal 0-99%
    uint16_t val_AGC:10;    // AGC value 0-1023
    uint16_t __dummy_AGC:6;
    uint16_t val_Vbat:10;   // voltage, step 10mV, 10bit ~ 0..10.23V
    uint16_t __dummy_Vbat:6;
    uint16_t val_5V:10;     // voltage, step 10mV, 10bit ~ 0..10.23V
    uint16_t __dummy_5V:6;
    uint16_t val_Ic:10;     // current, step 1mA, 10bit ~ 0..1023mA
    uint16_t __dummy_Ic:6;
    int8_t val_T_RX;        // temperature, -128..+127°C as unsigned 0..255
    int8_t val_T_TX;        // temperature, -128..+127°C as unsigned 0..255
} EEPROM_FRAME;

extern uint8_t GetCharHi5(uint16_t Number);
extern uint8_t GetCharLo5(uint16_t Number);
extern void EncCharFull(uint16_t Number, char **p);
extern char GetModeChar(uint8_t mode);

extern void EEPROM_SendTelemetry(void);
extern void EEPROM_SendMemory(uint16_t include);
extern uint8_t EEPROM_InitLoad(void);
extern void EEPROM_Erase(void);

#endif
