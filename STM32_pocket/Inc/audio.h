#ifndef _AUDIO_H_
#define _AUDIO_H_

#define SAMPLE_FREQ         20000   // sampling rate
#define AUDIO_BUFFER_LEN    4096    // audio buffer size
#define AUDIO_TIMEOUT       150000  // max audio transmission (150sec)
#define AUDIO_VOLUME_SSTV   (config.sstv_ampl) // peak volume in Q15
#define AUDIO_VOLUME_PSK    (config.psk_ampl) // peak volume in Q15
#define AUDIO_VOLUME_MORSE  (config.cw_ampl) // peak volume in Q15

#define AUDIO_RESET_IDX     0x01
#define AUDIO_RESET_PHI     0x02
#define AUDIO_START_PHI     0x04

#define PSK_SYM_0           0
#define PSK_SYM_1           1
#define PSK_SYM_START       2
#define PSK_SYM_STOP        3

#define DITLEN              1   // Length of a dot
#define DAHLEN              3   // Length of a dash
#define IEGLEN              1   // Length of inter-element gap
#define ICGLEN              3   // Length of inter-character gap
#define IWGLEN              7   // Length of inter-word gap

#define CHROMA_R_Y          0   // Rgb
#define CHROMA_B_Y          2   // rgB

extern void audio_start();
extern void audio_stop();

extern void audio_psk(uint16_t speed, uint16_t freq, const char *s);
extern void audio_morse(uint16_t wpm, uint16_t freq, const char *s);

extern void audio_play_vox_start();
extern void audio_play_vox_stop();
extern void audio_play_vis(uint8_t vis);
extern void audio_play_vis16(uint16_t vis16);

extern void audio_robot36_color(uint8_t *scanline);
extern void audio_robot72_color(uint8_t *scanline);
extern void audio_mp73(uint8_t *scanline);
extern void audio_mp115(uint8_t *scanline);

#endif /* _AUDIO_H_ */
