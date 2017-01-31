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
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "common.h"
#include "bpsk.h"

static const uint16_t VARICODE_CHARSET[] PROGMEM = {
    0b1000000000000000, 1 + 2,  //Space
    0b1111111110000000, 9 + 2,  //!
    0b1010111110000000, 9 + 2,  //"
    0b1111101010000000, 9 + 2,  // #
    0b1110110110000000, 9 + 2,  // $
    0b1011010101000000, 10 + 2, // %
    0b1010111011000000, 10 + 2, // &
    0b1011111110000000, 9 + 2,  // '
    0b1111101100000000, 8 + 2,  //(
    0b1111011100000000, 8 + 2,  //)
    0b1011011110000000, 9 + 2,  // *
    0b1110111110000000, 9 + 2,  //+
    0b1110101000000000, 7 + 2,  //,
    0b1101010000000000, 6 + 2,  //-
    0b1010111000000000, 7 + 2,  //.
    0b1101011110000000, 9 + 2,  ///
    0b1011011100000000, 8 + 2,  //0
    0b1011110100000000, 8 + 2,  //1
    0b1110110100000000, 8 + 2,  //2
    0b1111111100000000, 8 + 2,  //3
    0b1011101110000000, 9 + 2,  //4
    0b1010110110000000, 9 + 2,  //5
    0b1011010110000000, 9 + 2,  //6
    0b1101011010000000, 9 + 2,  //7
    0b1101010110000000, 9 + 2,  //8
    0b1101101110000000, 9 + 2,  //9
    0b1111010100000000, 8 + 2,  //:
    0b1101011110000000, 9 + 2,  ////
    0b1111011010000000, 9 + 2,  // <
    0b1010101000000000, 7 + 2,  //=
    0b1110101110000000, 9 + 2,  // >
    0b1010101111000000, 10 + 2, //?
    0b1010111101000000, 10 + 2, //@
    0b1111101000000000, 7 + 2,  //A
    0b1110101100000000, 8 + 2,  //B
    0b1010110100000000, 8 + 2,  //C
    0b1011010100000000, 8 + 2,  //D
    0b1110111000000000, 7 + 2,  //E
    0b1101101100000000, 8 + 2,  //F
    0b1111110100000000, 8 + 2,  //G
    0b1010101010000000, 9 + 2,  //H
    0b1111111000000000, 7 + 2,  //I
    0b1111111010000000, 9 + 2,  //J
    0b1011111010000000, 9 + 2,  //K
    0b1101011100000000, 8 + 2,  //L
    0b1011101100000000, 8 + 2,  //M
    0b1101110100000000, 8 + 2,  //N
    0b1010101100000000, 8 + 2,  //O
    0b1101010100000000, 8 + 2,  //P
    0b1110111010000000, 9 + 2,  //Q
    0b1010111100000000, 8 + 2,  //R
    0b1101111000000000, 7 + 2,  //S
    0b1101101000000000, 7 + 2,  //T
    0b1010101110000000, 9 + 2,  //U
    0b1101101010000000, 9 + 2,  //V
    0b1010111010000000, 9 + 2,  //W
    0b1011101010000000, 9 + 2,  //X
    0b1011110110000000, 9 + 2,  //Y
    0b1010101101000000, 10 + 2, //Z
    0b1111101110000000, 9 + 2,  //[
    0b1111011110000000, 9 + 2,  //
    0b1111110110000000, 9 + 2,  //]
    0b1010111111000000, 10 + 2, //^
    0b1011011010000000, 9 + 2,  //_
    0b1011011111000000, 10 + 2, //'
    0b1011000000000000, 4 + 2,  //a
    0b1011111000000000, 7 + 2,  //b
    0b1011110000000000, 6 + 2,  //c
    0b1011010000000000, 6 + 2,  //d
    0b1100000000000000, 2 + 2,  //e
    0b1111010000000000, 6 + 2,  //f
    0b1011011000000000, 7 + 2,  //g
    0b1010110000000000, 6 + 2,  //h
    0b1101000000000000, 4 + 2,  //i
    0b1111010110000000, 9 + 2,  //j
    0b1011111100000000, 8 + 2,  //k
    0b1101100000000000, 5 + 2,  //l
    0b1110110000000000, 6 + 2,  //m
    0b1111000000000000, 4 + 2,  //n
    0b1110000000000000, 3 + 2,  //o
    0b1111110000000000, 6 + 2,  //p
    0b1101111110000000, 9 + 2,  //q
    0b1010100000000000, 5 + 2,  //r
    0b1011100000000000, 5 + 2,  //s
    0b1010000000000000, 3 + 2,  //t
    0b1101110000000000, 6 + 2,  //u
    0b1111011000000000, 7 + 2,  //v
    0b1101011000000000, 7 + 2,  //w
    0b1101111100000000, 8 + 2,  //x
    0b1011101000000000, 7 + 2,  //y
    0b1110101010000000, 9 + 2,  //z
};

static const uint16_t VARICODE_SPECIAL[] PROGMEM = {
    0b1111100000000000, 5 + 2,  //CR
    0b0000000000000000, 16,     //sync
    0b0000000000000000, 4,      //tail
};

#define CNT_CARRIER sizeof(tbl_Carrier)
static const int8_t tbl_Carrier[] PROGMEM = {
    0,6,11,14,15,14,11,6,0,-6,-11,-14,-15,-14,-11,-6
};

#define S_MAX       128
#define CNT_SHAPE   sizeof(tbl_Shape)
static const int8_t tbl_Shape[] PROGMEM = {
    -127,-127,-127,-127,-127,-127,-126,-126,-126,-126,-125,-125,-125,-124,-124,-123,
    -123,-122,-122,-121,-120,-120,-119,-118,-117,-117,-116,-115,-114,-113,-112,-111,
    -110,-109,-108,-107,-106,-104,-103,-102,-101,-99,-98,-97,-95,-94,-93,-91,-90,-88,
    -87,-85,-84,-82,-81,-79,-77,-76,-74,-72,-71,-69,-67,-65,-64,-62,-60,-58,-56,-54,
    -52,-51,-49,-47,-45,-43,-41,-39,-37,-35,-33,-31,-29,-27,-25,-23,-21,-19,-17,-15,
    -12,-10,-8,-6,-4,-2,0,2,4,6,8,10,12,15,17,19,21,23,25,27,29,31,33,35,37,39,41,43,
    45,47,49,51,52,54,56,58,60,62,63,65,67,69,71,72,74,76,77,79,81,82,84,85,87,88,90,
    91,93,94,95,97,98,99,101,102,103,104,106,107,108,109,110,111,112,113,114,115,116,
    117,117,118,119,120,120,121,122,122,123,123,124,124,125,125,125,126,126,126,126,
    127,127,127,127,127
};


static uint8_t BPSK_Speed = 1;

static volatile uint8_t BPSK_Phase = 0;
static volatile int8_t BPSK_next_DA;
static volatile enum { BPSK_OFF, BPSK_IDLE, BPSK_SEND_READY, BPSK_SEND_WAIT } BPSK_state = BPSK_OFF;


ISR(TIMER0_COMPA_vect)
{
    static uint8_t j;

    switch (BPSK_state) {
        case BPSK_OFF:
            set_DA(0x10);
            break;
        
        case BPSK_SEND_WAIT:
            j = 0;
        case BPSK_IDLE:
            BPSK_state = BPSK_IDLE;
            int8_t temp = (int8_t)(BPSK_Phase ? -pgm_read_byte(&tbl_Carrier[j % CNT_CARRIER]) : pgm_read_byte(&tbl_Carrier[j % CNT_CARRIER]));
            temp = (int16_t)(temp) * (int8_t)pgm_read_byte(&tbl_Shape[j * BPSK_Speed]) / S_MAX;
            set_DA((temp ^ 0x10) & 0x1F);
            j++;
            if (j >= CNT_SHAPE / BPSK_Speed) {
                BPSK_Phase = !BPSK_Phase;
                BPSK_state = BPSK_SEND_WAIT;
            }
            break;
        
        case BPSK_SEND_READY:
            set_DA((BPSK_next_DA ^ 0x10) & 0x1F);
            BPSK_state = BPSK_SEND_WAIT;
            break;
    }
}


static void BPSK_SigCarrier(void)
{
    for (uint8_t j = 0; j < CNT_SHAPE / BPSK_Speed; j++) {
        do {} while (BPSK_state != BPSK_SEND_WAIT && BPSK_state != BPSK_OFF);
        int8_t temp = BPSK_Phase ? -pgm_read_byte(&tbl_Carrier[j % CNT_CARRIER]) : pgm_read_byte(&tbl_Carrier[j % CNT_CARRIER]);
        BPSK_next_DA = -temp;
        BPSK_state = BPSK_SEND_READY;
    }
}


static void BPSK_SigReverse(uint8_t start, uint8_t stop)
{
    for (uint8_t j = start / BPSK_Speed; j < stop / BPSK_Speed; j++) {
        do {} while (BPSK_state != BPSK_SEND_WAIT && BPSK_state != BPSK_OFF);
        int8_t temp = BPSK_Phase ? -pgm_read_byte(&tbl_Carrier[j % CNT_CARRIER]) : pgm_read_byte(&tbl_Carrier[j % CNT_CARRIER]);
        BPSK_next_DA = (int16_t)(temp) * (int8_t)pgm_read_byte(&tbl_Shape[j * BPSK_Speed]) / S_MAX;
        BPSK_state = BPSK_SEND_READY;
    }
    BPSK_Phase = !BPSK_Phase;
}


void BPSK_SigStart(void)
{
    BPSK_Phase = 0;
    BPSK_SigReverse(CNT_SHAPE/2, CNT_SHAPE);
}


void BPSK_SigStop(void)
{
    BPSK_SigReverse(0, CNT_SHAPE/2);
    BPSK_state = BPSK_OFF;
}


bool BPSK_SendChar(uint8_t Byte)
{
    // Get Pointer to Table
    const uint16_t *p;
    if (Byte >= ' ' && Byte <= 'z') p = VARICODE_CHARSET + 2 * (Byte - ' ');
    else if (Byte == '\r' || Byte == '\n') p = VARICODE_SPECIAL+0; // CR
    else if (Byte == '\f') p = VARICODE_SPECIAL+2; // idle sync - form feed
    else if (Byte == '\t') p = VARICODE_SPECIAL+4; // tail - horizontal tab
    else return true;
    
    uint16_t DataByte = pgm_read_word(p++);
    uint16_t DataLength = pgm_read_word(p++);
    for (uint8_t i = 0; i < DataLength; i++) {
        if (DataByte & 0x8000) BPSK_SigCarrier();
        else BPSK_SigReverse(0, CNT_SHAPE);
        DataByte <<= 1;
    }
    return SupervisorTask();
}


bool BPSK_SendBuffer(const char *p)
{
    bool ok = true;
    while (*p && ok) {
        ok = BPSK_SendChar(*p++);
    }
    return ok;
}


void BPSK_SetSpeedDiv(uint8_t i)
{
    BPSK_Speed = i;
}


void BPSK_Init(void)
{
    // DA port output init
    DDRD |= _BV(DA0+0) | _BV(DA0+1) | _BV(DA0+2) | _BV(DA0+3) | _BV(DA0+4);

    // TIMER0 init
    TCNT0 = 0;
    OCR0A = 76; // ((36864000/8) / (76+1)) / (CNT_SHAPE*CNT_CARRIER) = 31.17Bd (should be 31.25Bd)
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS01); // fc/8, CTC
    TIMSK0 = _BV(OCIE0A);
}
