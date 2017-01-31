#ifndef _COMM_H_
#define _COMM_H_

#define PSK_BUFFER_LEN      128  // serial ringbuffer length for PSK board
#define CMD_BUFFER_LEN      4096 // serial ringbuffer length for APRS commanding
#define CMD_MAX_LEN         256  // max command length for APRS commanding
#define ADC_AVERAGE         32  // ADC averaging factor

#define PSK_CMD_TX_NO_RX    'B'
#define PSK_CMD_TX_KEEP_RX  'K'
#define PSK_CMD_TX_IDLE     'I'
#define PSK_CMD_STOP_TX     'E'
#define PSK_RSP_ACK         'a'
#define PSK_RSP_DENIED      'x'
#define PSK_RSP_UPLINK_CMD  'm'
#define PSK_RSP_AUTO_CMD    'n'
#define PSK_RSP_SSTV_36     '3'
#define PSK_RSP_SSTV_73     '7'
#define PSK_RSP_TLM         't'

// temperature sensor calibration
#define TS_CAL_1        *(uint16_t*)(0x1FFF7A2C)
#define TS_CAL_2        *(uint16_t*)(0x1FFF7A2E)
#define HOT_CAL_TEMP    110
#define COLD_CAL_TEMP   30

typedef enum { SRC_CMD, SRC_UPLINK, SRC_AUTO, SRC_STARTUP } CMD_SOURCE;

extern void cmd_handler(char *cmd, CMD_SOURCE src);
extern void psk_uplink_handler(char cmd);
extern void psk_auto_handler(char cmd);

extern void cmd_handler_const(const char *cmd, CMD_SOURCE src);
extern bool psk_request(char c);
extern void cmd_response(const char *format, ...);
extern void printf_debug(const char *format, ...);
#define streq(__s1, __s2) (strcasecmp(__s1, __s2) == 0)

extern void comm_init();
extern void comm_cmd_task();
extern void comm_psk_task();

extern uint16_t adc_read_light();
extern int16_t adc_read_temperature();

#endif /* _COMM_H_ */
