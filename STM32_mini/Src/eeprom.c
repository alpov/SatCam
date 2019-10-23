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


static uint8_t i2c_delay_value = 255; // I2C half bit delay derived from HCLK
static uint32_t auth_tick_end = 0; // end tick of authorization

static const char *counter_name[] = {
    [LOG_RST_IWDG] = "rst-iwdg",
    [LOG_RST_WWDG] = "rst-wwdg",
    [LOG_RST_BOR] = "rst-bor",
    [LOG_RST_SFTR] = "rst-sftr",
    [LOG_RST_PIN] = "rst-pin",
    [LOG_HARDFAULT] = "hardfault",
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
    [LOG_CMD_IGNORED] = "cmd-ignored",
    [LOG_BOOT] = "boot",
};

#define i2c_delay()     for (uint8_t __delay = 0; __delay < i2c_delay_value; __delay++) __NOP();
#define i2c_wscl(__x)   EEP_SCL_GPIO_Port->BSRR = EEP_SCL_Pin << ((__x) ? 0 : 16)
#define i2c_wsda(__x)   EEP_SDA_GPIO_Port->BSRR = EEP_SDA_Pin << ((__x) ? 0 : 16)
#define i2c_rsda()      (EEP_SDA_GPIO_Port->IDR & EEP_SDA_Pin)


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


/*
static uint8_t GetCharHi5(uint16_t Number)
{
    Number &= 1023;
    uint8_t temp = (Number >> 5);    // bits 9..5
    if (temp < 26) return temp + 97; // a..z for 0..25
    else return temp + 39;           // A..F for 26..31
}


static uint8_t GetCharLo5(uint16_t Number)
{
    Number &= 1023;
    uint8_t temp = (Number & 31);    // bits 4..0
    if (temp < 26) return temp + 97; // a..z for 0..25
    else return temp + 39;           // A..F for 26..31
}


static void EncCharFull(uint16_t Number, char **p)
{
#if 1
    (*p)[0] = GetCharHi5(Number);
    (*p)[1] = GetCharLo5(Number);
    *p += 2;
#else
    *p += sprintf_P(*p, PSTR("%u "), Number & 0x03FF);
#endif
}
*/


/* Initialization of the I2C bus interface. Need to be called only once. */
static void i2c_init(void)
{
    i2c_wsda(1);
    i2c_wscl(1);
    i2c_delay();
}


/* Send one byte to I2C device. Returns 0=OK, 1=failed. */
static uint8_t i2c_write(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        i2c_wscl(0); i2c_delay();
        i2c_wsda(data & 0x80); i2c_delay();
        i2c_wscl(1); i2c_delay();
        data <<= 1;
    }
    i2c_wscl(0); i2c_wsda(1); i2c_delay();
    i2c_wscl(1); i2c_delay();
    uint8_t result = i2c_rsda() ? 1 : 0;
    return result;
}


/* Read one byte from the I2C device, ACK as required. */
static uint8_t i2c_read(bool ack)
{
    uint8_t data = 0;
    for (uint8_t i = 0; i < 8; i++) {
        data <<= 1;
        i2c_wscl(0); i2c_wsda(1); i2c_delay();
        i2c_wscl(1); i2c_delay();
        data |= i2c_rsda() ? 1 : 0;
    }
    i2c_wscl(0); i2c_wsda(ack ? 0 : 1); i2c_delay();
    i2c_wscl(1); i2c_delay();
    return data;
}


/* Issues a start condition and sends address. Returns 0=OK, 1=failed. */
static uint8_t i2c_start(uint8_t address)
{
    i2c_delay();
    i2c_wsda(0); i2c_delay();
    return i2c_write(address);
}


/* Issues a repeated start condition and sends address. Returns 0=OK, 1=failed. */
static uint8_t i2c_rep_start(uint8_t address)
{
    i2c_delay();
    i2c_wscl(0); i2c_delay();
    i2c_wsda(1); i2c_delay();
    i2c_wscl(1); i2c_delay();
    i2c_wsda(0); i2c_delay();
    return i2c_write(address);
}


/* Terminates the data transfer and releases the I2C bus. */
static void i2c_stop(void)
{
    i2c_delay();
    i2c_wscl(0); i2c_wsda(0); i2c_delay();
    i2c_wscl(1); i2c_delay();
    i2c_wsda(1); i2c_delay();
}


/* Issues a start condition and sends address. Use ACK polling until timeout. Returns 0=OK, 1=failed. */
static uint8_t i2c_start_wait(uint8_t address)
{
    uint32_t tickstart = HAL_GetTick();
    bool wip;
    do {
        wip = i2c_start(address);
        if (wip) i2c_stop();
    } while (wip && (HAL_GetTick() - tickstart < EEPROM_TIMEOUT));
    return wip;
}


bool eeprom_init(void)
{
    /* enable BKUP SRAM */
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_BKPSRAM_CLK_ENABLE();

    /* check EEPROM connectivity */
    i2c_init();
    if (i2c_start_wait(EEPROM_ADDR+I2C_READ) != 0) return false;
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


void eeprom_set_freq(uint32_t hclk)
{
    /* calculate ticks count for half bit delay based on HCLK */
    i2c_delay_value = (hclk / 5) / (EEPROM_SPEED * 2);
    if (i2c_delay_value < 2) i2c_delay_value = 1;
    else i2c_delay_value--;
}


bool eeprom_read(uint16_t addr_eeprom, void *addr_ram, uint16_t length)
{
    uint8_t *buffer = (uint8_t *)(addr_ram);

    if (i2c_start_wait(EEPROM_ADDR+I2C_WRITE) != 0) return false;
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
    if (i2c_start_wait(EEPROM_ADDR+I2C_WRITE) != 0) return false;
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
    uint16_t addr = erase_config ? ADDR_CONFIG : ADDR_HARDFAULT;
    uint8_t buffer[128]; // min. 2 pages to reset watchdog

    memset(buffer, 0xFF, sizeof(buffer));
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
    str += snprintf(str, end-str, "flash crc %#08x\r", (unsigned int)crc);
    if (str > end) return;

    if (config.user_pin == 0) {
        str += snprintf(str, end-str, "master-pin %lu\r", auth_get_master_pin());
        if (str > end) return;
    }
}


void syslog_read_config(char *str)
{
    char *end = str + SYSLOG_MAX_LENGTH - 8;

    str += snprintf(str, end-str, "%s config at %u\r", config.callsign, (unsigned int)HAL_GetTick());
    if (str > end) return;

    str += snprintf(str, end-str, "ov2640 delay %u, qs %u, agc %u, aec %u, agc-ceiling %u, agc-manual %u, aec-manual %u, awb %u, rotate %u\r",
        config.cam.delay, config.cam.qs, config.cam.agc, config.cam.aec, config.cam.agc_ceiling,
        config.cam.agc_manual, config.cam.aec_manual, config.cam.awb, config.cam.rotate
    );
    if (str > end) return;

    str += snprintf(str, end-str, "camera temp %d'C, voltage %umV, autoreboot %lu\r",
        adc_read_temperature(), adc_read_voltage(), config.autoreboot
    );
    if (str > end) return;

    for (uint8_t i = 0; i < 4; i++) {
        str += snprintf(str, end-str, "mode %d '%s' on %d\r",
            i, config.mode_cmd[i], config.start_edge
        );
        if (str > end) return;
    }
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


uint32_t auth_get_master_pin(void)
{
    #define UID_BASE 0x1FFF7A10UL /*!< Unique device ID register base address */
    return ((*((uint32_t *)UID_BASE) ^ *((uint32_t *)UID_BASE+4) ^ *((uint32_t *)UID_BASE+8) ^ 0x4438f5a1) % 1000000) | 0x20000;
}


bool auth_check_token(uint32_t token)
{
    if (token == config.user_pin || token == auth_get_master_pin()) {
        auth_tick_end = HAL_GetTick() + AUTH_TIME*1000;
        return true;
    } else {
        return false;
    }
}


bool auth_check_req(void)
{
    if (HAL_GetTick() < auth_tick_end || config.user_pin == 0) return true;
    auth_tick_end = 0;
    return false;
}

