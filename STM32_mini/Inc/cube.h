#ifndef _CUBE_H_
#define _CUBE_H_

/* global configuration */
#define CALLSIGN_CW             "SATCAM"
#define CMD_REQUEST_TAG         "SATCAMERA:"
#define CMD_RESPONSE_HEADER     "> "
#define CMD_SEPARATOR           "."
#define ENABLE_PRINTF_DEBUG     1
#define ENABLE_SWD_DEBUG        1
#define MIN_MULTI_DELAY         60
#define DEFAULT_SSTV_MODE       36
#define CW_WPM                  25
#define CW_FREQ                 800
#define PSK_SPEED               31
#define PSK_SPEED2              125
#define PSK_FREQ                800
#define AUTH_TIME               (15*60)

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx_hal.h"

extern void enable_turbo(bool en);
extern void main_satcam();

#define set_led_red(__x)        HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, (__x))
#define set_led_yellow(__x)     HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, (__x))

extern ADC_HandleTypeDef hadc1;

extern CRC_HandleTypeDef hcrc;

extern DAC_HandleTypeDef hdac;
extern DMA_HandleTypeDef hdma_dac2;

extern DCMI_HandleTypeDef hdcmi;
extern DMA_HandleTypeDef hdma_dcmi;

extern I2C_HandleTypeDef hi2c2;

extern IWDG_HandleTypeDef hiwdg;

extern TIM_HandleTypeDef htim6;

extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;

#define	IMPORT_BIN(file, type, sym)\
        __asm__ (\
        ".section \".rodata\"\n"\
        ".balign 4\n"\
        ".global " #sym "\n"\
        #sym ":\n"\
        ".incbin \"" file "\"\n"\
        ".global _sizeof_" #sym "\n"\
        ".set _sizeof_" #sym ", . - " #sym "\n"\
        ".balign 4\n"\
        ".section \".text\"\n");\
        extern type sym[]

#endif /* _CUBE_H_ */
