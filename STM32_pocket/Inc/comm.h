#ifndef _COMM_H_
#define _COMM_H_

#define CMD_BUFFER_LEN      4096 // serial ringbuffer length for APRS commanding
#define CMD_MAX_LEN         256  // max command length for APRS commanding
#define ADC_AVERAGE         32  // ADC averaging factor

// temperature sensor calibration
#define TS_CAL_1        *(uint16_t*)(0x1FFF7A2C)
#define TS_CAL_2        *(uint16_t*)(0x1FFF7A2E)
#define HOT_CAL_TEMP    110
#define COLD_CAL_TEMP   30

extern void cmd_handler(uint16_t command);

#define printf_debug(...)
#define streq(__s1, __s2) (strcasecmp(__s1, __s2) == 0)

extern void comm_init();
extern void comm_cmd_task();

extern uint16_t adc_read_light();
extern uint16_t adc_read_voltage();
extern int16_t adc_read_temperature();

#endif /* _COMM_H_ */
