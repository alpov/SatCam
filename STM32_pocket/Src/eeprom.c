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
#include "comm.h"
#include "eeprom.h"

/* access to global configuration in satcam.c */
extern CONFIG_SYSTEM config;
extern const CONFIG_SYSTEM config_default;

static uint16_t sample_light[MAX_SAMPLES], sample_volt[MAX_SAMPLES];
static int16_t sample_temp[MAX_SAMPLES];
static uint16_t sample_light_wr, sample_volt_wr, sample_temp_wr;


static const char *counter_name[] = {
    [LOG_RST_IWDG] = "rst-iwdg",
    [LOG_RST_WWDG] = "rst-wwdg",
    [LOG_RST_BOR] = "rst-bor",
    [LOG_RST_SFTR] = "rst-sftr",
    [LOG_RST_PIN] = "rst-pin",
    [LOG_HARDFAULT] = "hardfault",
    [LOG_FLASH_INIT_ERROR] = "flash-init-error",
    [LOG_FLASH_TIMEOUT] = "flash-timeout",
    [LOG_AUDIO_START] = "audio-start",
    [LOG_AUDIO_STOP] = "audio-stop",
    [LOG_CAM_START] = "cam-start",
    [LOG_CAM_STOP] = "cam-stop",
    [LOG_CAM_READY] = "cam-ready",
    [LOG_CAM_SNAPSHOT] = "cam-snapshot",
    [LOG_CAM_I2C_ERROR] = "cam-i2c-error",
    [LOG_CAM_DCMI_ERROR] = "cam-dcmi-error",
    [LOG_CAM_SIZE_ERROR] = "cam-size-error",
    [LOG_JPEG_ERROR] = "jpeg-error",
    [LOG_CMD_HANDLED] = "cmd-handled",
    [LOG_I2C_WDR] = "i2c-wdr",
    [LOG_AUTOREBOOT] = "autoreboot",
    [LOG_BOOT] = "boot",
};

#define i2c_delay()     for (uint8_t __delay = 0; __delay < i2c_delay_value; __delay++) __NOP();
#define i2c_wscl(__x)   EEP_SCL_GPIO_Port->BSRR = EEP_SCL_Pin << ((__x) ? 0 : 16)
#define i2c_wsda(__x)   EEP_SDA_GPIO_Port->BSRR = EEP_SDA_Pin << ((__x) ? 0 : 16)
#define i2c_rsda()      (EEP_SDA_GPIO_Port->IDR & EEP_SDA_Pin)

#include "soft_i2c.h"


void HardFault_Handler(void) __attribute__ ((naked)); // otherwise we need to check LR depth in MSP
void HardFault_Handler(void)
{
    uint32_t *stack_frame = (uint32_t*)__get_MSP();

    /* save current CPU state to BKUP memory */
    BKUP->tick = HAL_GetTick();
    BKUP->fault = SCB->CFSR; // Configurable fault status register
    if (BKUP->fault & (1 << 7)) BKUP->addr_fault = SCB->MMFAR; // MMARVALID
    else if (BKUP->fault & (1 << 15)) BKUP->addr_fault = SCB->BFAR; // BFARVALID
    else BKUP->addr_fault = 0;
    BKUP->addr_lr =  stack_frame[5]; // Link register from stack
    BKUP->addr_pc =  stack_frame[6]; // Program counter from stack
    BKUP->magic = SYSLOG_MAGIC; // hardfault descriptor valid

#ifdef ENABLE_SWD_DEBUG
    /* debug output */
    printf_debug("hardfault: fault %#x at %#x, pc %#x, lr %#x",
        (unsigned int)BKUP->fault, (unsigned int)BKUP->addr_fault, (unsigned int)BKUP->addr_pc, (unsigned int)BKUP->addr_lr);
#endif

    /* trigger system reset */
    NVIC_SystemReset();
}


bool eeprom_init(void)
{
    /* enable BKUP SRAM */
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_BKPSRAM_CLK_ENABLE();

    /* check EEPROM connectivity */
    i2c_init();
    if (i2c_start_wait(EEPROM_ADDR+I2C_READ, EEPROM_TIMEOUT) != 0) return false;
    i2c_stop();

    /* log hardfault reason */
    if (BKUP->magic == SYSLOG_MAGIC) {
        eeprom_write(ADDR_HARDFAULT, BKUP, sizeof(SYSLOG_HARDFAULT));
        BKUP->magic = 0;
        syslog_event(LOG_HARDFAULT);
    }

    /* log reset source */
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) syslog_event(LOG_RST_IWDG);
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) syslog_event(LOG_RST_WWDG);
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST)) syslog_event(LOG_RST_BOR);
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) syslog_event(LOG_RST_SFTR);
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) syslog_event(LOG_RST_PIN);
    __HAL_RCC_CLEAR_RESET_FLAGS();

    return true;
}


bool eeprom_read(uint16_t addr_eeprom, void *addr_ram, uint16_t length)
{
    uint8_t *buffer = (uint8_t *)(addr_ram);

    if (i2c_start_wait(EEPROM_ADDR+I2C_WRITE, EEPROM_TIMEOUT) != 0) return false;
    i2c_write(addr_eeprom >> 8);
    i2c_write(addr_eeprom >> 0);
    i2c_rep_start(EEPROM_ADDR+I2C_READ);
    while (length > 1) {
        *buffer++ = i2c_read(1); // ACK
        length--;
    }
    *buffer = i2c_read(0); // NAK
    i2c_stop();
    return true;
}


bool eeprom_write(uint16_t addr_eeprom, void *addr_ram, uint16_t length)
{
    uint8_t *buffer = (uint8_t *)(addr_ram);

next_page:
    if (i2c_start_wait(EEPROM_ADDR+I2C_WRITE, EEPROM_TIMEOUT) != 0) return false;
    i2c_write(addr_eeprom >> 8);
    i2c_write(addr_eeprom >> 0);
    while (length > 0) {
        i2c_write(*buffer++);
        addr_eeprom++;
        length--;
        if ((addr_eeprom & 0x001F) == 0 && length > 0) {
            /* page boundary reached - store the page and continue with next one */
            i2c_stop();
            HAL_IWDG_Refresh(&hiwdg);
            goto next_page;
        }
    }
    i2c_stop();
    return true;
}


bool eeprom_erase_full(bool erase_config)
{
    uint16_t addr = erase_config ? ADDR_CONFIG : ADDR_EVENTS;
    uint8_t buffer[128]; // min. 2 pages to reset watchdog

    memset(buffer, 0xFF, sizeof(buffer));
    if (!erase_config) eeprom_write(ADDR_HARDFAULT, buffer, 0x0020);
    while (addr < 0x2000) {
        if (!eeprom_write(addr, buffer, sizeof(buffer))) return false;
        addr += sizeof(buffer);
    }
    return true;
}


void config_load_default(void)
{
    memcpy(&config, &config_default, sizeof(config));
}


bool config_load_eeprom(void)
{
    uint32_t crc;
    uint8_t *c = (uint8_t*)&config;

    if (!eeprom_read(ADDR_CONFIG, &config, sizeof(config))) {
        config_load_default();
        return false;
    }

    HAL_CRC_Calculate(&hcrc, NULL, 0); // reset CRC
    for (uint16_t i = 0; i < sizeof(config) - sizeof(config.crc); i++) {
        uint32_t data = c[i]; // use as 8 bits
        crc = HAL_CRC_Accumulate(&hcrc, &data, 1);
    }

    if (crc != config.crc) {
        config_load_default();
        return false;
    } else {
        return true;
    }
}


void config_save_eeprom(void)
{
    uint32_t crc;
    uint8_t *c = (uint8_t*)&config;

    HAL_CRC_Calculate(&hcrc, NULL, 0); // reset CRC
    for (uint16_t i = 0; i < sizeof(config) - sizeof(config.crc); i++) {
        uint32_t data = c[i]; // use as 8 bits
        config.crc = HAL_CRC_Accumulate(&hcrc, &data, 1);
    }

    // write only if CRC changed
    eeprom_read(ADDR_CONFIG + sizeof(config) - sizeof(crc), &crc, sizeof(crc));
    if (crc != config.crc) eeprom_write(ADDR_CONFIG, &config, sizeof(config));
}


void syslog_read_nvinfo(char *str)
{
    char *end = str + SYSLOG_MAX_LENGTH - 8;

    str += snprintf(str, end-str, "%s NVinfo at %u\rbuild " __DATE__ "\r", config.callsign, (unsigned int)HAL_GetTick());
    if (str > end) return;

    EVENT_COUNTER ec;
    for (uint8_t i = 0; i < LOG_EVENT_LAST; i++) {
        eeprom_read(ADDR_EVENTS + i*EEPROM_PAGESIZE, &ec, sizeof(ec));
        if (ec.counter != 0xFFFFFFFF) {
            str += snprintf(str, end-str, "%s %d at %u\r", counter_name[i], (int)ec.counter+1, (unsigned int)ec.last_tick);
            if (str > end) return;
        }
    }

    SYSLOG_HARDFAULT sh;
    eeprom_read(ADDR_HARDFAULT, &sh, sizeof(sh));
    if (sh.fault != 0xFFFFFFFF) {
        str += snprintf(str, end-str, "hardfault tick %u, fault %#x at %#x, pc %#x, lr %#x\r",
            (unsigned int)sh.tick, (unsigned int)sh.fault, (unsigned int)sh.addr_fault, (unsigned int)sh.addr_pc, (unsigned int)sh.addr_lr);
        if (str > end) return;
    }

    extern void *_crc_end;
    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)FLASH_BASE, ((uint32_t)(&_crc_end) - FLASH_BASE) / 4);
    #define UID_BASE 0x1FFF7A10UL /*!< Unique device ID register base address */
    str += snprintf(str, end-str, "flash crc %#08x, unique id %#08x\r",
            (unsigned int)crc, *((unsigned int *)UID_BASE) ^ *((unsigned int *)UID_BASE+4) ^ *((unsigned int *)UID_BASE+8));
    if (str > end) return;
}


void syslog_read_config(char *str)
{
    char *end = str + SYSLOG_MAX_LENGTH - 8;

    str += snprintf(str, end-str, "%s config at %u\r", config.callsign, (unsigned int)HAL_GetTick());
    if (str > end) return;

    str += snprintf(str, end-str, "save page %u, count %u, delay %u, llo %u, lhi %u\r",
        config.save_page, config.save_count, config.save_delay, config.save_light_lo, config.save_light_hi
    );
    if (str > end) return;

    str += snprintf(str, end-str, "sampling light %u, volt %u, temp %u\r",
        config.spl_light_delay, config.spl_volt_delay, config.spl_temp_delay
    );
    if (str > end) return;

    str += snprintf(str, end-str, "ov2640 delay %u, qs %u, agc %u, aec %u, agc-ceiling %u, agc-manual %u, aec-manual %u, awb %u, rotate %u\r",
        config.cam_delay, config.cam_qs, config.cam_agc, config.cam_aec, config.cam_agc_ceiling,
        config.cam_agc_manual, config.cam_aec_manual, config.cam_awb, config.cam_rotate
    );
    if (str > end) return;

    str += snprintf(str, end-str, "psk speed %u, freq %u, append %u\rcw wpm %u, freq %u\r",
        config.psk_speed, config.psk_freq, config.psk_append, config.cw_wpm, config.cw_freq
    );
    if (str > end) return;

    str += snprintf(str, end-str, "sys i2c-watchdog %u, autoreboot %u\r",
        config.sys_i2c_watchdog, config.sys_autoreboot
    );
    if (str > end) return;

    uint16_t light = adc_read_light();
    str += snprintf(str, end-str, "camera temp %d'C, light %02ulx%u, voltage %umV\r",
        adc_read_temperature(), light % 100, light / 100, adc_read_voltage()
    );
    if (str > end) return;

    str += snprintf(str, end-str, "config dump ");
    for (uint16_t i = 0; i < sizeof(CONFIG_SYSTEM)/2; i++) {
        str += snprintf(str, end-str, "%x ", ((uint16_t*)(&config))[i]);
        if (str > end) return;
    }
    str += snprintf(str, end-str, "\r");
    if (str > end) return;
}


void syslog_read_telemetry(char *str)
{
    char *end = str + SYSLOG_MAX_LENGTH - 8;

    str += snprintf(str, end-str, "%s telemetry at %u\rbuild " __DATE__ "\r", config.callsign, (unsigned int)HAL_GetTick());
    if (str > end) return;

    EVENT_COUNTER ec;
    for (uint8_t i = 0; i < LOG_EVENT_LAST; i++) {
        eeprom_read(ADDR_EVENTS + i*EEPROM_PAGESIZE, &ec, sizeof(ec));
        if (ec.counter != 0xFFFFFFFF) {
            str += snprintf(str, end-str, "%s %d\r", counter_name[i], (int)ec.counter+1);
            if (str > end) return;
        }
    }

    SYSLOG_HARDFAULT sh;
    eeprom_read(ADDR_HARDFAULT, &sh, sizeof(sh));
    if (sh.fault != 0xFFFFFFFF) {
        str += snprintf(str, end-str, "hardfault tick %u, fault %#x at %#x, pc %#x, lr %#x\r",
            (unsigned int)sh.tick, (unsigned int)sh.fault, (unsigned int)sh.addr_fault, (unsigned int)sh.addr_pc, (unsigned int)sh.addr_lr);
        if (str > end) return;
    }

    str += snprintf(str, end-str, "sstv page %u, count %u, delay %u, llo %u, lhi %u\r",
        config.save_page, config.save_count, config.save_delay, config.save_light_lo, config.save_light_hi
    );
    if (str > end) return;

    uint16_t light = adc_read_light();
    str += snprintf(str, end-str, "camera temp %d'C, light %02ulx%u, voltage %umV\r",
        adc_read_temperature(), light % 100, light / 100, adc_read_voltage()
    );
    if (str > end) return;

    extern void *_crc_end;
    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)FLASH_BASE, ((uint32_t)(&_crc_end) - FLASH_BASE) / 4);
    #define UID_BASE 0x1FFF7A10UL /*!< Unique device ID register base address */
    str += snprintf(str, end-str, "flash crc %#08x, unique id %#08x\r",
            (unsigned int)crc, *((unsigned int *)UID_BASE) ^ *((unsigned int *)UID_BASE+4) ^ *((unsigned int *)UID_BASE+8));
    if (str > end) return;
}


void syslog_read_samples(char *str, uint8_t what, uint16_t count)
{
    char *end = str + SYSLOG_MAX_LENGTH - 8;

    if (what == SAMPLE_LIGHT) {
        str += snprintf(str, end-str, "%s light lux per %u.%us at %u\r", config.callsign, config.spl_light_delay/10, config.spl_light_delay%10, (unsigned int)HAL_GetTick());
        if (str > end) return;

        uint16_t idx = sample_light_wr;
        for (uint16_t i = 0; i < count; i++) {
            if (idx > 0) idx--; else idx = MAX_SAMPLES-1;
            str += snprintf(str, end-str, "%ue%u ", sample_light[idx] % 100, sample_light[idx] / 100);
            if (str > end) return;
        }
    }
    else if (what == SAMPLE_VOLTAGE) {
        str += snprintf(str, end-str, "%s voltage mV per %u.%us at %u\r", config.callsign, config.spl_volt_delay/10, config.spl_volt_delay%10, (unsigned int)HAL_GetTick());
        if (str > end) return;

        uint16_t idx = sample_volt_wr;
        for (uint16_t i = 0; i < count; i++) {
            if (idx > 0) idx--; else idx = MAX_SAMPLES-1;
            str += snprintf(str, end-str, "%u ", sample_volt[idx]);
            if (str > end) return;
        }
    }
    else if (what == SAMPLE_TEMP) {
        str += snprintf(str, end-str, "%s temperature 'C per %u.%us at %u\r", config.callsign, config.spl_temp_delay/10, config.spl_temp_delay%10, (unsigned int)HAL_GetTick());
        if (str > end) return;

        uint16_t idx = sample_temp_wr;
        for (uint16_t i = 0; i < count; i++) {
            if (idx > 0) idx--; else idx = MAX_SAMPLES-1;
            str += snprintf(str, end-str, "%d ", sample_temp[idx]);
            if (str > end) return;
        }
    }

    str += snprintf(str, end-str, "\r");
    if (str > end) return;
}


void syslog_event(LOG_EVENT event)
{
    EVENT_COUNTER ec;
    uint16_t addr = ADDR_EVENTS + event*EEPROM_PAGESIZE;

    /* test for valid event ID */
    if (event >= LOG_EVENT_LAST) return;

    /* read EEPROM counter, increment and save back */
    ec.counter = syslog_get_counter(event) + 1;
    ec.last_tick = HAL_GetTick();
    eeprom_write(addr, &ec, sizeof(ec));
}


uint32_t syslog_get_counter(LOG_EVENT event)
{
    EVENT_COUNTER ec;
    if (!eeprom_read(ADDR_EVENTS + event*EEPROM_PAGESIZE, &ec, sizeof(ec))) ec.counter = 0;

    return ec.counter;
}


void sampling_task(void)
{
    static uint32_t sample_light_last, sample_volt_last, sample_temp_last;

    if (config.spl_light_delay && (HAL_GetTick() >= sample_light_last + (config.spl_light_delay*100))) {
        uint16_t value = adc_read_light(); /* measure light here */
        sample_light[sample_light_wr++] = value;
        if (sample_light_wr >= MAX_SAMPLES) sample_light_wr = 0;
        config.value_light = value;
        sample_light_last = HAL_GetTick();
    }

    if (config.spl_volt_delay && (HAL_GetTick() >= sample_volt_last + (config.spl_volt_delay*100))) {
        uint16_t value = adc_read_voltage(); /* measure voltage here */
        sample_volt[sample_volt_wr++] = value;
        if (sample_volt_wr >= MAX_SAMPLES) sample_volt_wr = 0;
        config.value_volt = value;
        sample_volt_last = HAL_GetTick();
    }

    if (config.spl_temp_delay && (HAL_GetTick() >= sample_temp_last + (config.spl_temp_delay*100))) {
        int16_t value = adc_read_temperature(); /* measure temperature here */
        sample_temp[sample_temp_wr++] = value;
        if (sample_temp_wr >= MAX_SAMPLES) sample_temp_wr = 0;
        config.value_temp = value;
        sample_temp_last = HAL_GetTick();
    }
}


