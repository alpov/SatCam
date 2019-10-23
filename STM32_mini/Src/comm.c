/*************************************************************************
 *
 * SatCam - Camera Module for PSAT-2
 * Copyright (c) 2015-2017 Ales Povalac <alpov@alpov.net>
 * Dept. of Radio Electronics, Brno University of Technology
 *
 * This work is licensed under the terms of the MIT license
 *
 *************************************************************************/

#define _GNU_SOURCE
#include <stdarg.h>
#include <ctype.h>
#include "cube.h"
#include "eeprom.h"
#include "comm.h"

static uint8_t cmd_rx_buf[CMD_BUFFER_LEN];
static uint16_t cmd_rx_read_ptr = 0;
#define cmd_rx_write_ptr (CMD_BUFFER_LEN - hdma_usart2_rx.Instance->NDTR)


int _write(int file, char const *buf, int n)
{
    /* stdout redirection to UART2 */
    HAL_UART_Transmit(&huart2, (char*)(buf), n, HAL_MAX_DELAY);
    return n;
}


static void cmd_process_line(char *line)
{
    char *start = strcasestr(line, CMD_REQUEST_TAG);
    if (start == NULL) {
        syslog_event(LOG_CMD_IGNORED);
        return;
    }
    start += strlen(CMD_REQUEST_TAG); // command starts after call found

    char *stop = strchr(start, '{');
    if (stop != NULL) *stop = '\0'; // crop command string on '{' if found

    syslog_event(LOG_CMD_HANDLED);
    cmd_handler(start); // process command
}


static void cmd_process_char(uint8_t c)
{
    static uint16_t cnt;
    static char data[CMD_MAX_LEN];
    static uint32_t comm_timeout;

    if (HAL_GetTick() - comm_timeout > 2000) cnt = 0; // timeout - clear buffer
    comm_timeout = HAL_GetTick();

    if (c >= 32 && c <= 126 && cnt < CMD_MAX_LEN) data[cnt++] = c; // only ASCII chars
    if ((c == '\n' || c == '\r') && (cnt > 0)) {
        data[cnt] = '\0';
        cmd_process_line(data);
        cnt = 0;
    }
}


void cmd_handler_const(const char *cmd)
{
    char cmd_rw[CMD_MAX_LEN];

    strncpy(cmd_rw, cmd, sizeof(cmd_rw));
    cmd_handler(cmd_rw);
}


void cmd_response(const char *format, ...)
{
    printf(CMD_RESPONSE_HEADER);
    va_list vl;
    va_start(vl, format);
    vprintf(format, vl);
    va_end(vl);
    printf("\r");
    fflush(stdout);
}


void printf_debug(const char *format, ...)
{
#if ENABLE_PRINTF_DEBUG
    printf("DEBUG: ");
    va_list vl;
    va_start(vl, format);
    vprintf(format, vl);
    va_end(vl);
    printf("\r");
    fflush(stdout);
    HAL_IWDG_Refresh(&hiwdg);
#endif
}


/* FIXME: This is not always called - bug in HAL? Better to test DMAR every time => call comm_init() periodically */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    /* restart DMA ring buffer RX after any error */
    comm_init();
}


void comm_init(void)
{
    /* start DMA ring buffers if not already started; need to clear overflow flags first, otherwise DMA will not start */
    if (HAL_IS_BIT_CLR(huart2.Instance->CR3, USART_CR3_DMAR)) {
        __HAL_UART_CLEAR_OREFLAG(&huart2);
        HAL_UART_Receive_DMA(&huart2, (uint8_t*)cmd_rx_buf, CMD_BUFFER_LEN);
        cmd_rx_read_ptr = 0;
    }
}


void comm_cmd_task(void)
{
    /* ring buffer processing */
    while (cmd_rx_read_ptr != cmd_rx_write_ptr) {
        uint16_t p = cmd_rx_read_ptr;
        if (++cmd_rx_read_ptr >= CMD_BUFFER_LEN) cmd_rx_read_ptr = 0;
        cmd_process_char(cmd_rx_buf[p]);
    }
}


static uint16_t adc_read(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {
        .Channel = channel,
        .Rank = 1,
        .SamplingTime = ADC_SAMPLETIME_144CYCLES,
    };
    uint32_t avg = 0;

    /* configure ADC channel, start and add values to average */
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);

    for (uint8_t i = 0; i < ADC_AVERAGE; i++) {
        HAL_ADC_PollForConversion(&hadc1, 100);
        __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC);
        avg += HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    return (avg / ADC_AVERAGE); // compute average
}


uint16_t adc_read_voltage()
{
    return adc_read(ADC_CHANNEL_0) * VDD_VALUE * (47+10) / 10 / 4096;
}


int16_t adc_read_temperature()
{
    uint32_t tempAVG = adc_read(ADC_CHANNEL_TEMPSENSOR);

    /* Correction factor if VDD <> 3V3 */
    tempAVG = tempAVG * VDD_VALUE / 3300;

    /* Calculate temperature in °C from ADC value; AN3964 - Temperature_sensor */
    int temperature_C = ((int32_t)(tempAVG) - (int32_t)(TS_CAL_1));
    temperature_C = temperature_C * (int32_t)(HOT_CAL_TEMP - COLD_CAL_TEMP);
    temperature_C = temperature_C / (int32_t)(TS_CAL_2 - TS_CAL_1);
    temperature_C = temperature_C + COLD_CAL_TEMP;
    return temperature_C;
}

