#ifndef _EEPROM_H_
#define _EEPROM_H_

#define SYSLOG_MAX_LENGTH   2000
#define CW_MAX_LENGTH       100
#define LIGHT_MAX_SAMPLES   100
#define STARTUP_CMD_LENGTH  50

#define PSK_MESSAGE         0x01
#define PSK_CONFIG          0x02
#define PSK_NVINFO          0x04
#define PSK_TLM             0x08
#define PSK_LIGHT           0x10

/* enumeration of log events */
// to disable an event, use #define LOG_to_disable LOG_EVENT_LAST
typedef enum {
    LOG_RST_IWDG, LOG_RST_WWDG, LOG_RST_BOR, LOG_RST_SFTR, LOG_RST_PIN, LOG_HARDFAULT,
    LOG_FLASH_INIT_ERROR, LOG_FLASH_TIMEOUT, LOG_EEPROM_INIT_ERROR, LOG_AUDIO_START,
    LOG_AUDIO_STOP, LOG_CAM_START, LOG_CAM_STOP, LOG_CAM_READY, LOG_CAM_SNAPSHOT, LOG_CAM_I2C_ERROR,
    LOG_CAM_DCMI_ERROR, LOG_CAM_SIZE_ERROR, LOG_JPEG_ERROR, LOG_PSK_TIMEOUT, LOG_CMD_HANDLED,
    LOG_CMD_IGNORED, LOG_PSK_UPLINK, LOG_BOOT, LOG_AUTH_ERROR,
    LOG_EVENT_LAST
} LOG_EVENT;

/* enumeration of command results */
typedef enum {
    R_OK, R_OK_SILENT, R_ERR_SYNTAX, R_ERR_AUTH, R_TX_DENIED, R_BOOT_OK, R_BOOT_ERR, R_QUEUED
} CMD_RESULT;

/* authorization mask for each subsystem */
#define AUTH_SSTV               0x00000001
#define AUTH_SSTV_LIVE          0x00000002
#define AUTH_SSTV_LIVE_MULTI    0x00000004
#define AUTH_SSTV_SAVE          0x00000008
#define AUTH_SSTV_SAVE_MULTI    0x00000010
#define AUTH_SSTV_LOAD          0x00000020
#define AUTH_SSTV_ROM           0x00000040
#define AUTH_SSTV_THUMBS        0x00000080
#define AUTH_PSK                0x00000100
#define AUTH_PSK_LOG            0x00000200
#define AUTH_PSK_MULTI          0x00000400
#define AUTH_CW                 0x00000800
#define AUTH_CW_MULTI           0x00001000
#define AUTH_AUTH_SET           0x00002000
#define AUTH_CAMCFG             0x00004000
#define AUTH_CAMCFG_STARTUP     0x00008000
#define AUTH_CAMCFG_SAVE        0x00010000
#define AUTH_DEBUG              0x00020000
#define AUTH_MULTI_HIGH_DUTY    0x00040000
#define AUTH_TCMD               0x00080000
#define AUTH_PSK_LIGHT          0x00100000


/* EEPROM address map */
/* When doing a write of less than 32 bytes the data in the rest of the page is
   refreshed along with the data bytes being written. This will force the entire
   page to endure a write cycle. Page is 32 bytes = 0x0020. */
#define ADDR_CONFIG     0x0000  // 0x0000-0x00xx    config struct, currently ~90B
#define ADDR_HARDFAULT  0x00E0  // 0x00E0-0x00F8    hardfault 24B
#define ADDR_TOKENS     0x0100  // 0x0100-0x1FF     auth tokens, bit field
#define ADDR_EVENTS     0x0200  // 0x0200-          event counters, one per page

// 24AA64, 64kbit = 8KByte, 32B pages
#define EEPROM_ADDR         0xA0    // I2C address
#define EEPROM_SPEED        100000  // I2C speed
#define EEPROM_TIMEOUT      20      // timeout [ms] for address ACK
#define EEPROM_PAGESIZE     0x0020  // page size for write

#define I2C_READ    1
#define I2C_WRITE   0


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
} CONFIG_CAMERA;

// nonvolatile system settings
typedef struct {
    CONFIG_CAMERA cam;
    bool sstv_keep_rx;
    uint32_t auth_req;
    uint16_t idle_time;
    char startup_cmd[STARTUP_CMD_LENGTH];
    uint32_t crc; // must be the last item in struct
} CONFIG_SYSTEM;

// event planning
typedef struct {
    struct {
        uint8_t mode;
        uint16_t count;
        uint16_t delay_curr;
        uint16_t delay_next;
    } sstv_live;
    struct {
        uint8_t page;
        uint16_t count;
        uint16_t delay_curr;
        uint16_t delay_next;
        uint16_t light_low;
        uint16_t light_high;
    } sstv_save;
    struct {
        uint16_t speed;
        uint16_t freq;
        uint8_t what;
        uint16_t count;
        uint16_t delay_curr;
        uint16_t delay_next;
        char buffer[SYSLOG_MAX_LENGTH];
    } psk;
    struct {
        uint16_t wpm;
        uint16_t freq;
        uint16_t count;
        uint16_t delay_curr;
        uint16_t delay_next;
        char buffer[CW_MAX_LENGTH];
    } cw;
    struct {
        uint16_t idx;
        uint16_t count;
        uint16_t delay_curr;
        uint16_t delay_next;
        uint16_t samples[LIGHT_MAX_SAMPLES];
    } light;
    uint32_t auth;
} CONFIG_PLAN;


extern bool eeprom_init(void);
extern void eeprom_set_freq(uint32_t hclk);
extern bool eeprom_read(uint16_t addr_eeprom, void *addr_ram, uint16_t length);
extern bool eeprom_write(uint16_t addr_eeprom, void *addr_ram, uint16_t length);
extern bool eeprom_erase_full(void);

extern void config_load_default(void);
extern bool config_load_eeprom(void);
extern void config_save_eeprom(void);

extern void syslog_read_nvinfo(char *str);
extern void syslog_read_config(char *str);
extern void syslog_read_telemetry(char *str);
extern void syslog_read_light(char *str);
extern void syslog_event(LOG_EVENT event);
extern uint32_t syslog_get_counter(LOG_EVENT event);

extern bool auth_check_token(uint32_t token);
extern bool auth_check_req(uint32_t req);

#endif /* _EEPROM_H_ */
