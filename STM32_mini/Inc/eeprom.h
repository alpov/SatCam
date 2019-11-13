#ifndef _EEPROM_H_
#define _EEPROM_H_

#define SYSLOG_MAX_LENGTH   2000
#define CW_MAX_LENGTH       100
#define STARTUP_CMD_LENGTH  40
#define CALLSIGN_LENGTH     10

#define PSK_MESSAGE         0x01
#define PSK_CONFIG          0x02
#define PSK_NVINFO          0x04

/* enumeration of log events */
// to disable an event, use #define LOG_to_disable LOG_EVENT_LAST
typedef enum {
    LOG_RST_IWDG, LOG_RST_WWDG, LOG_RST_BOR, LOG_RST_SFTR, LOG_RST_PIN,
    LOG_HARDFAULT, LOG_EEPROM_INIT_ERROR, LOG_AUDIO_START, LOG_AUDIO_STOP,
    LOG_CAM_START, LOG_CAM_STOP, LOG_CAM_READY, LOG_CAM_SNAPSHOT,
    LOG_CAM_I2C_ERROR, LOG_CAM_DCMI_ERROR, LOG_CAM_SIZE_ERROR,
    LOG_JPEG_ERROR, LOG_CMD_HANDLED, LOG_CMD_IGNORED, LOG_BOOT,
    LOG_EVENT_LAST
} LOG_EVENT;

/* enumeration of command results */
typedef enum {
    R_OK, R_OK_SILENT, R_ERR_SYNTAX, R_ERR_AUTH, R_BOOT_OK, R_BOOT_ERR, R_QUEUED
} CMD_RESULT;

/* EEPROM address map */
/* When doing a write of less than 32 bytes the data in the rest of the page is
   refreshed along with the data bytes being written. This will force the entire
   page to endure a write cycle. Page is 32 bytes = 0x0020. */
#define ADDR_CONFIG     0x0000  // 0x0000-0x00xx    config struct, currently ~90B
#define ADDR_HARDFAULT  0x01E0  // 0x01E0-0x01F8    hardfault 24B
#define ADDR_EVENTS     0x0200  // 0x0200-          event counters, one per page

// 24AA64, 64kbit = 8KByte, 32B pages
#define EEPROM_ADDR         0xA0    // I2C address
#define EEPROM_SPEED        100000  // I2C speed
#define EEPROM_TIMEOUT      20      // timeout [ms] for address ACK
#define EEPROM_PAGESIZE     0x0020  // page size for write

#define I2C_READ    1
#define I2C_WRITE   0

#define EDGE_NONE       0
#define EDGE_RISING     1
#define EDGE_FALLING    2
#define EDGE_BOTH       3


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
typedef struct {
    CONFIG_CAMERA cam;
    char callsign[CALLSIGN_LENGTH];
    char mode_cmd[4][STARTUP_CMD_LENGTH];
    uint8_t start_edge;
    uint16_t holdoff;
    uint32_t autoreboot;
    uint32_t user_pin;
    uint32_t crc; // must be the last item in struct
} CONFIG_SYSTEM;


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
extern void syslog_event(LOG_EVENT event);
extern uint32_t syslog_get_counter(LOG_EVENT event);

extern uint32_t auth_get_master_pin(void);
extern bool auth_check_token(uint32_t token);
extern bool auth_check_req(void);

#endif /* _EEPROM_H_ */
