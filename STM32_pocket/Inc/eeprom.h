#ifndef _EEPROM_H_
#define _EEPROM_H_

#define SYSLOG_MAX_LENGTH   2000
#define CW_MAX_LENGTH       100
#define CALLSIGN_LENGTH     10
#define MAX_SAMPLES         256

#define SAMPLE_LIGHT        0
#define SAMPLE_VOLTAGE      1
#define SAMPLE_TEMP         2

/* enumeration of log events */
// to disable an event, use #define LOG_to_disable LOG_EVENT_LAST
typedef enum {
    LOG_RST_IWDG, LOG_RST_WWDG, LOG_RST_BOR, LOG_RST_SFTR, LOG_RST_PIN, LOG_HARDFAULT,
    LOG_FLASH_INIT_ERROR, LOG_FLASH_TIMEOUT, LOG_AUDIO_START, LOG_AUDIO_STOP,
    LOG_CAM_START, LOG_CAM_STOP, LOG_CAM_READY, LOG_CAM_SNAPSHOT, LOG_CAM_I2C_ERROR,
    LOG_CAM_DCMI_ERROR, LOG_CAM_SIZE_ERROR, LOG_JPEG_ERROR, LOG_CMD_HANDLED,
    LOG_BOOT, LOG_I2C_WDR, LOG_AUTOREBOOT,
    LOG_EVENT_LAST
} LOG_EVENT;

/* EEPROM address map */
/* When doing a write of less than 32 bytes the data in the rest of the page is
   refreshed along with the data bytes being written. This will force the entire
   page to endure a write cycle. Page is 32 bytes = 0x0020. */
#define ADDR_CONFIG     0x0000  // 0x0000-0x00xx    config struct, currently ~90B
#define ADDR_HARDFAULT  0x01E0  // 0x01E0-0x01F8    hardfault 24B
#define ADDR_EVENTS     0x0200  // 0x0200-          event counters, one per page

// 24AA64, 64kbit = 8KByte, 32B pages
#define EEPROM_ADDR         0xA0    // I2C address
#define EEPROM_TIMEOUT      20      // timeout [ms] for address ACK
#define EEPROM_PAGESIZE     0x0020  // page size for write


// hardfault log - timestamp and hardfault reason
typedef struct {
    uint32_t magic;
    uint32_t tick;
    uint32_t fault;
    uint32_t addr_fault;
    uint32_t addr_pc;
    uint32_t addr_lr;
} SYSLOG_HARDFAULT;

// backup RAM for data retention over reset
#define BKUP            ((SYSLOG_HARDFAULT *) BKPSRAM_BASE)
#define SYSLOG_MAGIC    (0xDEADBEEF)

// event - counter and timestamp
typedef struct {
    uint32_t counter;
    uint32_t last_tick;
} EVENT_COUNTER;

// camera settings
typedef struct {
    uint16_t delay;
    uint8_t qs;
    bool agc;
    bool aec;
    uint8_t agc_ceiling;
    uint16_t agc_manual;
    uint16_t aec_manual;
    uint8_t awb;
    bool rotate;
} CONFIG_CAMERA;

// nonvolatile system settings
#define CONFIG_SIZE_GENERAL 0x40
#define CONFIG_SIZE_ALL     0x60
typedef struct {
    // 0x00
    uint16_t command;
    uint16_t save_page;
    uint16_t save_count;
    uint16_t save_delay;
    uint16_t save_light_lo;
    uint16_t save_light_hi;
    uint16_t spl_light_delay;
    uint16_t spl_volt_delay;

    // 0x10
    uint16_t spl_temp_delay;
    uint16_t psk_append;
    uint16_t value_light;
    uint16_t value_volt;
    uint16_t value_temp;
    uint16_t _dummy_1[3];

    // 0x20
    uint16_t cam_agc;
    uint16_t cam_aec;
    uint16_t cam_agc_ceiling;
    uint16_t cam_agc_manual;
    uint16_t cam_aec_manual;
    uint16_t cam_awb;
    uint16_t cam_rotate;
    uint16_t psk_speed;

    // 0x30
    uint16_t psk_freq;
    uint16_t cw_wpm;
    uint16_t cw_freq;
    uint16_t _dummy_2[5];

    // 0x40
    char callsign[CALLSIGN_LENGTH]; // 10
    uint16_t sys_i2c_watchdog;
    uint16_t sys_autoreboot;
    uint16_t cam_delay;

    // 0x50
    uint16_t cam_qs;
    uint16_t sstv_ampl;
    uint16_t psk_ampl;
    uint16_t cw_ampl;
    uint16_t debug_enable;
    uint16_t light_cal;
    uint16_t _dummy_3[2];

    // 0x60 - outside addressable range
    uint32_t crc; // must be the last item in struct
} CONFIG_SYSTEM;


/* global configuration, declaration in satcam.c */
extern CONFIG_SYSTEM config;


extern bool eeprom_init(void);
extern void eeprom_set_freq(uint32_t hclk);
extern bool eeprom_read(uint16_t addr_eeprom, void *addr_ram, uint16_t length);
extern bool eeprom_write(uint16_t addr_eeprom, void *addr_ram, uint16_t length);
extern bool eeprom_erase_full(bool erase_config);

extern void config_load_default(void);
extern bool config_load_eeprom(void);
extern void config_save_eeprom(void);

extern void syslog_read_nvinfo(char *str);
extern void syslog_read_config(char *str);
extern void syslog_read_telemetry(char *str);
extern void syslog_read_samples(char *str, uint8_t what, uint16_t count);
extern void syslog_event(LOG_EVENT event);
extern uint32_t syslog_get_counter(LOG_EVENT event);

extern uint32_t auth_get_master_pin(void);
extern bool auth_check_token(uint32_t token);
extern bool auth_check_req(void);

extern void sampling_task(void);

#endif /* _EEPROM_H_ */
