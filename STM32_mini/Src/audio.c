/*************************************************************************
 *
 * SatCam - Camera Module for PSAT-2
 * Copyright (c) 2015-2017 Ales Povalac <alpov@alpov.net>
 * Dept. of Radio Electronics, Brno University of Technology
 *
 * This work is licensed under the terms of the MIT license
 *
 *************************************************************************/

#include "cube.h"
#include <arm_math.h>
#include "sstv.h"
#include "eeprom.h"
#include "audio.h"

#define INCLUDE_VARICODE
#include "varicode.h"

#define INCLUDE_MORSE
#include "morse.h"

static uint8_t audio_buffer[AUDIO_BUFFER_LEN];
static volatile uint8_t audio_current_buffer = 0;
static uint16_t idx;
static q31_t phi;


void HAL_DACEx_ConvCpltCallbackCh2(DAC_HandleTypeDef* hdac)
{
    /* DMA reached the end of buffer */
    audio_current_buffer = 0;
    HAL_PWR_DisableSleepOnExit();
}


void HAL_DACEx_ConvHalfCpltCallbackCh2(DAC_HandleTypeDef* hdac)
{
    /* DMA at the half of buffer */
    audio_current_buffer = 1;
    HAL_PWR_DisableSleepOnExit();
}


static void sample_to_buffer(uint8_t value)
{
    audio_buffer[idx++] = value;
    if (hdma_dac2.State == HAL_DMA_STATE_READY && idx == AUDIO_BUFFER_LEN/2) {
        /* buffer filled to 1st half, DMA idle -> start audio output */
        HAL_TIM_Base_Start(&htim6);
        HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_2, (uint32_t*)audio_buffer, AUDIO_BUFFER_LEN, DAC_ALIGN_8B_R);
        audio_current_buffer = 0;
    }
    else if (idx == AUDIO_BUFFER_LEN/2) {
        /* buffer filled to 1st half, transmit 2nd half and sleep until the transmission of 1st half begins */
        HAL_PWR_EnableSleepOnExit();
        while (audio_current_buffer == 1) {}
    }
    else if (idx == AUDIO_BUFFER_LEN) {
        /* buffer filled to 2nd half, transmit 1st half and sleep until the transmission of 2nd half begins */
        HAL_PWR_EnableSleepOnExit();
        while (audio_current_buffer == 0) {}
        idx = 0;
        HAL_IWDG_Refresh(&hiwdg); // 200ms period (AUDIO_BUFFER_LEN/SAMPLE_FREQ)
    }
}


static void audio_to_buffer(uint16_t freq, q15_t ampl)
{
    phi += (q31_t)((1ULL << 31) / SAMPLE_FREQ) * freq; // phase accumulator
    phi &= 0x7fffffff;
    int8_t y = (arm_sin_q15(phi >> 16) * ampl) >> (24-1); // sinus(phi) * amplitude, convert to q7_t
    y ^= 0x80; // bias to Vcc/2
    sample_to_buffer(y);
}


static void audio_play_tone(uint32_t samples, uint16_t freq, q15_t volume)
{
    while (samples--) audio_to_buffer(freq, volume);
}


static void audio_play_line(uint16_t t, uint16_t width, uint8_t *line, q15_t volume)
{
    for (int i = 0; i < t; i++) {
        int idx = i * width / t;
        int freq = 1500 + (2300-1500) * line[idx] / 255; // convert uint8_t to 1500-2300Hz range
        audio_to_buffer(freq, volume);
    }
}


static void audio_play_psk(uint16_t samples, uint16_t freq, uint8_t symbol, q15_t volume)
{
    static int16_t invert = 1;

    if (symbol == PSK_SYM_1) {
        /* symbol 1 - keep phase */
        q15_t ampl = volume;
        ampl *= invert;
        for (uint16_t i = 0; i < samples; i++) {
            audio_to_buffer(freq, ampl);
        }
    } else {
        /* symbol 0 - reverse phase; START/STOP - half of the symbol */
        if (symbol == PSK_SYM_START) invert = 1;
        uint16_t start = (symbol == PSK_SYM_START) ? samples/2 : 0;
        uint16_t stop = (symbol == PSK_SYM_STOP) ? samples/2 : samples;
        for (uint16_t i = start; i < stop; i++) {
            q15_t ampl = ((q31_t)(volume) * arm_cos_q15((0x4000 * i) / samples)) >> (16-1);
            ampl *= invert;
            audio_to_buffer(freq, ampl);
        }
        invert *= -1;
    }
}


static void audio_psk_char(uint16_t rate, uint16_t freq, char c)
{
    uint16_t varicode = VARICODE_TABLE[c & 0x7f] >> 2;
    while (varicode) {
        audio_play_psk(rate, freq, varicode & 0x8000 ? PSK_SYM_1 : PSK_SYM_0, AUDIO_VOLUME_PSK);
        varicode <<= 1;
    }
}


void audio_start()
{
    // reset buffer index and phase accumulator
    HAL_GPIO_WritePin(GPIO_PTT_GPIO_Port, GPIO_PTT_Pin, GPIO_PIN_SET);
    syslog_event(LOG_AUDIO_START);
    idx = 0;
    phi = 0;
    // cosinus ramp up from 0V to Vcc/2 bias
    for (uint16_t i = 0; i < AUDIO_BUFFER_LEN; i++) {
        q15_t theta = i * (0x4000 / AUDIO_BUFFER_LEN);
        q15_t ramp = (arm_cos_q15(theta + 0x4000) / 2) + 0x4000;
        sample_to_buffer(ramp >> 8);
    }
}


void audio_stop()
{
    // cosinus ramp down from Vcc/2 bias to 0V
    for (uint16_t i = 0; i < AUDIO_BUFFER_LEN; i++) {
        q15_t theta = i * (0x4000 / AUDIO_BUFFER_LEN);
        q15_t ramp = (arm_cos_q15(theta) / 2) + 0x4000;
        sample_to_buffer(ramp >> 8);
    }
    for (uint16_t i = 0; i < AUDIO_BUFFER_LEN; i++) sample_to_buffer(0); // fill buffer with zero samples
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_2); // stop audio output -> ca. half buffer will be lost
    HAL_TIM_Base_Stop(&htim6);
    HAL_GPIO_WritePin(GPIO_PTT_GPIO_Port, GPIO_PTT_Pin, GPIO_PIN_RESET);
    syslog_event(LOG_AUDIO_STOP);
}


static uint16_t audio_psk_get_rate(uint16_t speed)
{
    switch (speed) {
        case 31: return (SAMPLE_FREQ / 31.25);
        case 63: return (SAMPLE_FREQ / 62.5);
        case 125: default: return (SAMPLE_FREQ / 125);
        case 250: return (SAMPLE_FREQ / 250);
        case 500: return (SAMPLE_FREQ / 500);
        case 1000: return (SAMPLE_FREQ / 1000);
    }
}


void audio_psk(uint16_t speed, uint16_t freq, const char *s)
{
    if (freq < 100) freq = PSK_FREQ;
    if (freq > 5000) freq = PSK_FREQ;

    uint32_t tickstart = HAL_GetTick();
    uint16_t rate = audio_psk_get_rate(speed);

    audio_play_psk(rate, freq, PSK_SYM_START, AUDIO_VOLUME_PSK); // start
    for (uint16_t i = 0; i < (SAMPLE_FREQ / rate); i++) audio_play_psk(rate, freq, PSK_SYM_0, AUDIO_VOLUME_PSK); // 1sec of zeros - sync
    audio_psk_char(rate, freq, '\r'); // CR
    while (*s && (HAL_GetTick() - tickstart < AUDIO_TIMEOUT)) {
        audio_psk_char(rate, freq, *s++); // data
    }
    audio_psk_char(rate, freq, '\r'); // CR
    audio_psk_char(rate, freq, ' '); // space to fix last CR
    for (uint16_t i = 0; i < (SAMPLE_FREQ / rate); i++) audio_play_psk(rate, freq, PSK_SYM_1, AUDIO_VOLUME_PSK); // 1sec of ones - no data
    audio_play_psk(rate, freq, PSK_SYM_STOP, AUDIO_VOLUME_PSK); // stop
}


static void audio_play_morse(uint16_t samples, uint16_t freq, bool key)
{
    static uint16_t shaping = 0;

    if (key) {
        shaping = 0.010*SAMPLE_FREQ; // 10ms rise/fall time for CW
        audio_play_psk(shaping, freq, PSK_SYM_START, AUDIO_VOLUME_MORSE);
        audio_play_psk(samples - shaping, freq, PSK_SYM_1, AUDIO_VOLUME_MORSE);
        audio_play_psk(shaping, freq, PSK_SYM_STOP, AUDIO_VOLUME_MORSE);
        phi = 0; // reset phase accumulator to keep DAC at bias during gap (CW is not coherent)
    } else {
        audio_play_tone(samples - shaping, 0, AUDIO_VOLUME_MORSE);
        shaping = 0;
    }
}


void audio_morse(uint16_t wpm, uint16_t freq, const char *s)
{
    if (wpm < 5) wpm = 5;
    if (wpm > 40) wpm = 40;
    if (freq < 100) freq = CW_FREQ;
    if (freq > 5000) freq = CW_FREQ;

    uint32_t tickstart = HAL_GetTick();
    uint16_t samples = (SAMPLE_FREQ * 6) / (5 * wpm); // u = 1.2/c for PARIS

    while (*s && (HAL_GetTick() - tickstart < AUDIO_TIMEOUT)) {
        char chr = *s++;
        uint8_t code = 0x80;

        if (chr >= '0' && chr <= '9') code = morse[chr - '0' + 0];
        else if (chr >= 'A' && chr <= 'Z') code = morse[chr - 'A' + 10];
        else if (chr >= 'a' && chr <= 'z') code = morse[chr - 'a' + 10];
        else {
            for (uint8_t i = 0; i < sizeof(spechar); i++) // Read through the array
                if (chr == spechar[i]) code = morse[i+36]; // Map it to morse code
        }

        if (chr == ' ') {
            audio_play_morse((IWGLEN - ICGLEN) * samples, freq, false); // ICG was already played after previous char
        } else {
            while (code != 0x80) {
                if (code & 0x80) audio_play_morse(DAHLEN * samples, freq, true); // Play a dash
                else audio_play_morse(DITLEN * samples, freq, true); // Play a dot
                audio_play_morse(IEGLEN * samples, freq, false); // Inter Element gap
                code <<= 1;
            }
            audio_play_morse((ICGLEN - IEGLEN) * samples, freq, false); // IEG was already played after element
        }
    }
}


void audio_play_vox_start()
{
    audio_play_vox_stop();
    audio_play_tone(0.100*SAMPLE_FREQ, 2300, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.100*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.100*SAMPLE_FREQ, 2300, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.100*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
}


void audio_play_vox_stop()
{
    audio_play_tone(0.100*SAMPLE_FREQ, 1900, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.100*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.100*SAMPLE_FREQ, 1900, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.100*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
}


void audio_play_vis(uint8_t vis)
{
    audio_play_tone(0.300*SAMPLE_FREQ, 1900, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.010*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.300*SAMPLE_FREQ, 1900, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.030*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
    for (uint8_t i = 0; i < 8; i++) {
        audio_play_tone(0.030*SAMPLE_FREQ, (vis & 0x01) ? 1100 : 1300, AUDIO_VOLUME_SSTV);
        vis >>= 1;
    }
    audio_play_tone(0.030*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
}


void audio_play_vis16(uint16_t vis)
{
    audio_play_tone(0.300*SAMPLE_FREQ, 1900, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.010*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.300*SAMPLE_FREQ, 1900, AUDIO_VOLUME_SSTV);
    audio_play_tone(0.030*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
    for (uint8_t i = 0; i < 16; i++) {
        audio_play_tone(0.030*SAMPLE_FREQ, (vis & 0x0001) ? 1100 : 1300, AUDIO_VOLUME_SSTV);
        vis >>= 1;
    }
    audio_play_tone(0.030*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
}


static void audio_compute_luma(uint8_t *scanline, uint8_t *luma, uint8_t lines)
{
    for (int i = 0; i < IMG_WIDTH * lines; i++) {
        uint16_t y = 0;
        y += (uint16_t)scanline[i*3+0] * 30;
        y += (uint16_t)scanline[i*3+1] * 59;
        y += (uint16_t)scanline[i*3+2] * 11;
        y /= 100;
        luma[i] = y;
    }
}


static void audio_compute_chroma(uint8_t *scanline, uint8_t *luma, uint8_t *chroma, uint8_t mode)
{
     for (int i = 0; i < IMG_WIDTH; i++) {
        uint16_t y = luma[i];
        uint16_t c = scanline[i*3+mode];
        chroma[i] = (c-y + 255) / 2;
    }
}


static void audio_compute_chroma_2lines(uint8_t *scanline, uint8_t *luma, uint8_t *chroma, uint8_t mode)
{
    for (int i = 0; i < IMG_WIDTH; i++) {
        uint16_t y = ((uint16_t)luma[i] + luma[i+IMG_WIDTH])/2;
        uint16_t c = ((uint16_t)scanline[i*3+mode] + scanline[i*3+mode+IMG_WIDTH*3])/2;
        chroma[i] = (c-y + 255) / 2;
    }
}


void audio_robot36_color(uint8_t *scanline)
{
    uint8_t luma[IMG_WIDTH*2];
    uint8_t chroma[IMG_WIDTH];

    for (int line = 0; line < IMG_HEIGHT; line += 2) {
        // luma 1st line
        audio_compute_luma(scanline, luma, 2);
        audio_play_tone(0.009*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
        audio_play_tone(0.003*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
        audio_play_line(0.088*SAMPLE_FREQ, IMG_WIDTH, luma, AUDIO_VOLUME_SSTV);

        // chroma R-Y
        audio_compute_chroma_2lines(scanline, luma, chroma, CHROMA_R_Y);
        audio_play_tone(0.0045*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
        audio_play_tone(0.0015*SAMPLE_FREQ, 1900, AUDIO_VOLUME_SSTV);
        audio_play_line(0.044*SAMPLE_FREQ, IMG_WIDTH, chroma, AUDIO_VOLUME_SSTV);

        // luma 2nd line
        audio_play_tone(0.009*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
        audio_play_tone(0.003*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
        audio_play_line(0.088*SAMPLE_FREQ, IMG_WIDTH, luma+IMG_WIDTH, AUDIO_VOLUME_SSTV);

        // chroma B-Y
        audio_compute_chroma_2lines(scanline, luma, chroma, CHROMA_B_Y);
        audio_play_tone(0.0045*SAMPLE_FREQ, 2300, AUDIO_VOLUME_SSTV);
        audio_play_tone(0.0015*SAMPLE_FREQ, 1900, AUDIO_VOLUME_SSTV);
        audio_play_line(0.044*SAMPLE_FREQ, IMG_WIDTH, chroma, AUDIO_VOLUME_SSTV);

        scanline += IMG_WIDTH*2*3;
    }
}


void audio_robot72_color(uint8_t *scanline)
{
    uint8_t luma[IMG_WIDTH];
    uint8_t chroma[IMG_WIDTH];

    for (int line = 0; line < IMG_HEIGHT; line += 1) {
        // luma
        audio_compute_luma(scanline, luma, 1);
        audio_play_tone(0.009*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
        audio_play_tone(0.003*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
        audio_play_line(0.138*SAMPLE_FREQ, IMG_WIDTH, luma, AUDIO_VOLUME_SSTV);

        // chroma R-Y
        audio_compute_chroma(scanline, luma, chroma, CHROMA_R_Y);
        audio_play_tone(0.0045*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
        audio_play_tone(0.0015*SAMPLE_FREQ, 1900, AUDIO_VOLUME_SSTV);
        audio_play_line(0.069*SAMPLE_FREQ, IMG_WIDTH, chroma, AUDIO_VOLUME_SSTV);

        // chroma B-Y
        audio_compute_chroma(scanline, luma, chroma, CHROMA_B_Y);
        audio_play_tone(0.0045*SAMPLE_FREQ, 2300, AUDIO_VOLUME_SSTV);
        audio_play_tone(0.0015*SAMPLE_FREQ, 1900, AUDIO_VOLUME_SSTV);
        audio_play_line(0.069*SAMPLE_FREQ, IMG_WIDTH, chroma, AUDIO_VOLUME_SSTV);

        scanline += IMG_WIDTH*3;
    }
}


void audio_mp73(uint8_t *scanline)
{
    uint8_t luma[IMG_WIDTH*2];
    uint8_t chroma[IMG_WIDTH];

    for (int line = 0; line < IMG_HEIGHT; line += 2) {
        // luma 1st line
        audio_compute_luma(scanline, luma, 2);
        audio_play_tone(0.009*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
        audio_play_tone(0.001*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
        audio_play_line(0.140*SAMPLE_FREQ, IMG_WIDTH, luma, AUDIO_VOLUME_SSTV);

        // chroma R-Y
        audio_compute_chroma_2lines(scanline, luma, chroma, CHROMA_R_Y);
        audio_play_line(0.140*SAMPLE_FREQ, IMG_WIDTH, chroma, AUDIO_VOLUME_SSTV);

        // chroma B-Y
        audio_compute_chroma_2lines(scanline, luma, chroma, CHROMA_B_Y);
        audio_play_line(0.140*SAMPLE_FREQ, IMG_WIDTH, chroma, AUDIO_VOLUME_SSTV);

        // luma 2nd line
        audio_play_line(0.140*SAMPLE_FREQ, IMG_WIDTH, luma+IMG_WIDTH, AUDIO_VOLUME_SSTV);

        scanline += IMG_WIDTH*2*3;
    }
}


void audio_mp115(uint8_t *scanline)
{
    uint8_t luma[IMG_WIDTH*2];
    uint8_t chroma[IMG_WIDTH];

    for (int line = 0; line < IMG_HEIGHT; line += 2) {
        // luma 1st line
        audio_compute_luma(scanline, luma, 2);
        audio_play_tone(0.009*SAMPLE_FREQ, 1200, AUDIO_VOLUME_SSTV);
        audio_play_tone(0.001*SAMPLE_FREQ, 1500, AUDIO_VOLUME_SSTV);
        audio_play_line(0.223*SAMPLE_FREQ, IMG_WIDTH, luma, AUDIO_VOLUME_SSTV);

        // chroma R-Y
        audio_compute_chroma_2lines(scanline, luma, chroma, CHROMA_R_Y);
        audio_play_line(0.223*SAMPLE_FREQ, IMG_WIDTH, chroma, AUDIO_VOLUME_SSTV);

        // chroma B-Y
        audio_compute_chroma_2lines(scanline, luma, chroma, CHROMA_B_Y);
        audio_play_line(0.223*SAMPLE_FREQ, IMG_WIDTH, chroma, AUDIO_VOLUME_SSTV);

        // luma 2nd line
        audio_play_line(0.223*SAMPLE_FREQ, IMG_WIDTH, luma+IMG_WIDTH, AUDIO_VOLUME_SSTV);

        scanline += IMG_WIDTH*2*3;
    }
}


