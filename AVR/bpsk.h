#ifndef _BPSK_H
#define _BPSK_H

extern void BPSK_SigStart(void);
extern void BPSK_SigStop(void);
extern bool BPSK_SendChar(uint8_t Byte);
extern bool BPSK_SendBuffer(const char *p);
extern void BPSK_SetSpeedDiv(uint8_t i);
extern void BPSK_Init(void);

#endif
