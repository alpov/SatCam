#ifndef _ADF_H
#define _ADF_H

#define TX_FREQ         435350000.0

// ---------- ADF7012
// fout=fPFD*(N_INT+N_FRAC/2^12)
//register 0
#define OUT_DIV         1       //output divider: disabled
#define FRQ_ERR_CORR    -237    //freq error correction (+1023 .. -1024); step is 112.5Hz
#define R_CNTR          1       // (1 .. 15)
#define VCO_ADJ         2       //vco adjust (0 .. 3)
//register 1
#define N_CNTR_INT      (uint16_t)(TX_FREQ/F_CPU)                             // (0 .. 255)
#define N_CNTR_FRAC     (uint16_t)(((TX_FREQ/F_CPU) - N_CNTR_INT) * (2<<11))  // (0 .. 4095)
//register 2
#define PA_LEVEL        63      //PA level (0 .. 63)
//register 3
#define CP_CURRENT      3       //charge pump current (0 .. 3)
#define BLEED           0       //bleed (1=up, 2=down, 0=off)
#define VCO_BIAS        5       //VCO bias (0 .. 15)
#define PA_BIAS         7       //PA bias (0 .. 7)
// ---------- ADF7012 end

extern void TX_Enable(void);
extern void TX_Disable(void);

#endif
