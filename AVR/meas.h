#ifndef _MEAS_H
#define _MEAS_H

// I2C
#define AD7415  0b10011100
#define AD7417  0b01011110
#define MCP4728 0b11000000

// ADC channels
#define ADC_VBAT    7
#define ADC_T_RX    0
#define ADC_MEAS3   1
#define ADC_IC      2
#define ADC_AGC     3
#define ADC_5V      4

extern int8_t ADC_Read_T_TX(void);
extern int8_t ADC_Read_T_RX(void);
extern uint16_t ADC_Read_Vbat(void);
extern uint16_t ADC_Read_5V(void);
extern uint16_t ADC_Read_Ic(void);
extern uint16_t ADC_Read_AGC(void);
extern uint8_t ADC_Read_PSK(void);
extern void ADC_Init(void);

extern uint8_t DAC_Write(uint8_t channel, uint16_t value);

#endif
