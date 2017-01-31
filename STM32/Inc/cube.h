#ifndef _CUBE_H_
#define _CUBE_H_

/* global configuration */
#define CALLSIGN_SSTV_PSK       "PSAT-2"
#define CALLSIGN_CW             "PSAT2"
#define CMD_REQUEST_TAG         ":PSAT-2CAM:"
#define CMD_RESPONSE_HEADER     "APRS:  "
#define ENABLE_PRINTF_DEBUG     1
#define ENABLE_SWD_DEBUG        1
#define ENABLE_PSK_COMM         0
#define STARTUP_CMD_DELAY       25
#define MIN_MULTI_DELAY         60
#define DEFAULT_SSTV_MODE       36
#define DISABLE_AUTH            1
#define CW_WPM                  25
#define CW_FREQ                 800
#define PSK_SPEED               31
#define PSK_FREQ                800
#define PSK_TLM_FREQ            "280"

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

extern SPI_HandleTypeDef hspi2;
extern DMA_HandleTypeDef hdma_spi2_rx;
extern DMA_HandleTypeDef hdma_spi2_tx;

extern TIM_HandleTypeDef htim6;

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart3_rx;

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
