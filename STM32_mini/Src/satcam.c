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
#include "eeprom.h"
#include "ov2640.h"
#include "audio.h"
#include "comm.h"
#include "sstv.h"
#include "eeprom.h"

static uint8_t jpeg[IMG_BUFFER_SIZE];
static struct {
    uint32_t length;
    char overlay[2][TEXT_LEN];
} img;

IMPORT_BIN("Inc/sstv_monoscope.jpg", uint8_t, img_monoscope);

uint8_t *images[] = {
    img_monoscope,
};

/* global configuration variables, linked from eeprom.c */
CONFIG_SYSTEM config;
const CONFIG_SYSTEM config_default = {
    .cam = {
        .delay = 1000,
        .qs = 5,
        .agc = true,
        .aec = true,
        .agc_ceiling = 16,
        .agc_manual = 0,
        .aec_manual = 0,
        .awb = AWB_SUNNY,
        .rotate = false,
    },
    .callsign = "SatCam",
    .mode_cmd = {
        "SSTV.LIVE.36",
        "SSTV.LIVE.73",
        "PSK.NVINFO.125.1000",
        "SSTV.ROM.115.0",
    },
    .start_edge = EDGE_BOTH,
    .autoreboot = 0,
    .user_pin = 0,
};


static bool camera_snapshot(void)
{
    set_led_red(true);
    if (!ov2640_hilevel_init(config.cam)) {
        set_led_red(false);
        return false;
    }

    img.length = ov2640_snapshot(jpeg, sizeof(jpeg)); // requires enabled turbo
    uint16_t agc = ov2640_get_current_agc();
    uint16_t aec = ov2640_get_current_aec();
    set_led_red(false);
    ov2640_enable(false);

    uint32_t img_counter = syslog_get_counter(LOG_CAM_SNAPSHOT) % 10000;
    uint16_t voltage = adc_read_voltage();
    snprintf(img.overlay[OVERLAY_HEADER], sizeof(img.overlay[OVERLAY_HEADER]), "%s +%05u %d\037C #%04u %umV", config.callsign,
        (unsigned int)(HAL_GetTick() / 60000), adc_read_temperature(), (unsigned int)img_counter, voltage
    );

    snprintf(img.overlay[OVERLAY_IMG], sizeof(img.overlay[OVERLAY_IMG]), "#%04u %uB g%u e%u", (unsigned int)img_counter, (int)img.length, agc, aec);

    return true;
}


static void send_downlink(CMD_RESULT what)
{
    const char *text = NULL;
    const char *cw = NULL;

    switch (what) {
        case R_OK:
            text = "OK";
            cw = CALLSIGN_CW " RRR";
            break;
        case R_OK_SILENT:
            text = "OK";
            break;
        case R_ERR_SYNTAX:
            text = "Syntax error";
            cw = CALLSIGN_CW " EEEEE";
            break;
        case R_ERR_AUTH:
            text = "Not authorized";
            cw = CALLSIGN_CW " EEE AUTH";
            break;
        case R_BOOT_OK:
            text = "Booted OK";
            // cw = CALLSIGN_CW " BOOT";
            break;
        case R_BOOT_ERR:
            text = "Booted, EEPROM checksum error";
            // cw = CALLSIGN_CW " BOOT EEEEE";
            break;
        case R_QUEUED:
            text = "Plan done";
            break;
    }

    if (text) {
        cmd_response(text);
    }
    if (cw) {
        audio_start();
        audio_morse(CW_WPM, CW_FREQ, cw);
        audio_stop();
    }
}


static CMD_RESULT cmd_sstv(char **saveptr)
{
    char *token = strtok_r(NULL, CMD_SEPARATOR, saveptr);
    if (token == NULL) return R_ERR_SYNTAX;
    else if (streq(token, "live")) {
        uint8_t mode = 36;
        char *overlay = NULL;
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) != NULL) mode = atol(token);
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) != NULL) overlay = token;

        /* start SSTV here */
        enable_turbo(true); // peak 18% CPU
        bool ok = camera_snapshot();
        if (ok) ok = jpeg_test(jpeg, img.length);
        if (ok) {
            sstv_set_overlay(OVERLAY_HEADER, img.overlay[OVERLAY_HEADER]);
            sstv_set_overlay(OVERLAY_IMG, img.overlay[OVERLAY_IMG]);
            sstv_set_overlay(OVERLAY_LARGE, overlay);
            sstv_set_overlay(OVERLAY_FROM, config.callsign);
            sstv_play_jpeg(jpeg, mode);
        }
        enable_turbo(false);

        return R_OK_SILENT;
    }
    else if (streq(token, "rom")) {
        uint8_t mode = 36;
        uint8_t sector = 0;
        char *overlay = NULL;
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) != NULL) mode = atol(token);
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) != NULL) sector = atol(token);
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) != NULL) overlay = token;

        /* load image from ROM: check sector number */
        if (sector >= sizeof(images)/sizeof(images[0])) return R_ERR_SYNTAX;

        /* send image: mode, overlay */
        snprintf(img.overlay[OVERLAY_HEADER], sizeof(img.overlay[OVERLAY_HEADER]), "%s +%05u %d\037C ROM #%u %umV", config.callsign,
            (unsigned int)(HAL_GetTick() / 60000), adc_read_temperature(), sector, adc_read_voltage()
        );
        sstv_set_overlay(OVERLAY_HEADER, img.overlay[OVERLAY_HEADER]);
        sstv_set_overlay(OVERLAY_IMG, NULL);
        sstv_set_overlay(OVERLAY_LARGE, overlay);
        sstv_set_overlay(OVERLAY_FROM, config.callsign);

        enable_turbo(true); // peak 18% CPU
        sstv_play_jpeg(images[sector], mode);
        enable_turbo(false);

        return R_OK_SILENT;
    }
    else return R_ERR_SYNTAX;
}


static CMD_RESULT cmd_psk(char **saveptr)
{
    char *token = strtok_r(NULL, CMD_SEPARATOR, saveptr);
    char buffer[SYSLOG_MAX_LENGTH];
    uint16_t speed = PSK_SPEED;
    uint16_t freq = PSK_FREQ;

    if (streq(token, "nvinfo")) {
        syslog_read_nvinfo(buffer);
        speed = PSK_SPEED2;
    }
    else if (streq(token, "config")) {
        syslog_read_config(buffer);
        speed = PSK_SPEED2;
    }
    else if (token == NULL) {
        snprintf(buffer, sizeof(buffer), "Greetings from %s, up %umin", config.callsign, (unsigned int)(HAL_GetTick() / 60000));
    }
    else {
        snprintf(buffer, sizeof(buffer), "%s %s", config.callsign, token);
    }

    if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) != NULL) speed = atol(token);
    if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) != NULL) freq = atol(token);

    /* start PSK here */
    enable_turbo(true); // peak 18% CPU
    audio_start();
    audio_psk(speed, freq, buffer); // 76% CPU without turbo, 4% CPU with turbo
    audio_stop();
    enable_turbo(false);

    return R_OK_SILENT;
}


static CMD_RESULT cmd_cw(char **saveptr)
{
    char *token = strtok_r(NULL, CMD_SEPARATOR, saveptr);
    char buffer[CW_MAX_LENGTH];

    if (token == NULL) {
        snprintf(buffer, sizeof(buffer), "73 DE %s %s %s K", config.callsign, config.callsign, config.callsign);
    }
    else {
        snprintf(buffer, sizeof(buffer), "%s %s", config.callsign, token);
    }

    uint16_t wpm = CW_WPM;
    uint16_t freq = CW_FREQ;
    if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) != NULL) wpm = atol(token);
    if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) != NULL) freq = atol(token);

    /* start CW here */
    audio_start();
    audio_morse(wpm, freq, buffer);
    audio_stop();

    return R_OK_SILENT;
}


static CMD_RESULT cmd_auth(char **saveptr)
{
    char *token = strtok_r(NULL, CMD_SEPARATOR, saveptr);
    if (token == NULL) return R_ERR_SYNTAX;
    if (!auth_check_token(atol(token))) return R_ERR_AUTH;
    return R_OK;
}


static CMD_RESULT cmd_camcfg(char **saveptr)
{
    char *token = strtok_r(NULL, CMD_SEPARATOR, saveptr);
    if (!auth_check_req()) return R_ERR_AUTH;
    if (token == NULL) return R_ERR_SYNTAX;
    else if (streq(token, "delay")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        config.cam.delay = atol(token);
        return R_OK;
    }
    else if (streq(token, "qs")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        config.cam.qs = atol(token);
        return R_OK;
    }
    else if (streq(token, "agc")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "ceiling")) {
            if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
            config.cam.agc = true;
            config.cam.agc_ceiling = atol(token);
            return R_OK;
        }
        else if (streq(token, "manual")) {
            if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
            config.cam.agc = false;
            config.cam.agc_manual = atol(token);
            return R_OK;
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "aec")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "auto")) {
            config.cam.aec = true;
            return R_OK;
        }
        else if (streq(token, "manual")) {
            if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
            config.cam.aec = false;
            config.cam.aec_manual = atol(token);
            return R_OK;
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "awb")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "auto")) {
            config.cam.awb = AWB_AUTO;
            return R_OK;
        }
        else if (streq(token, "sunny")) {
            config.cam.awb = AWB_SUNNY;
            return R_OK;
        }
        else if (streq(token, "cloudy")) {
            config.cam.awb = AWB_CLOUDY;
            return R_OK;
        }
        else if (streq(token, "office")) {
            config.cam.awb = AWB_OFFICE;
            return R_OK;
        }
        else if (streq(token, "home")) {
            config.cam.awb = AWB_HOME;
            return R_OK;
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "rotate")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "off")) {
            config.cam.rotate = 0;
            return R_OK;
        }
        else if (streq(token, "on")) {
            config.cam.rotate = 1;
            return R_OK;
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "start")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        uint8_t mode = atol(token);
        if (strlen(*saveptr) >= STARTUP_CMD_LENGTH-1) return R_ERR_SYNTAX;
        strncpy(config.mode_cmd[mode], *saveptr, STARTUP_CMD_LENGTH);
        return R_OK;
    }
    else if (streq(token, "startedge")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "rising")) {
            config.start_edge = EDGE_RISING;
            return R_OK;
        }
        else if (streq(token, "falling")) {
            config.start_edge = EDGE_FALLING;
            return R_OK;
        }
        else if (streq(token, "any")) {
            config.start_edge = EDGE_BOTH;
            return R_OK;
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "callsign")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        if (strlen(token) >= CALLSIGN_LENGTH-1) return R_ERR_SYNTAX;
        strncpy(config.callsign, token, CALLSIGN_LENGTH);
        return R_OK;
    }
    else if (streq(token, "autoreboot")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        config.autoreboot = atol(token);
        return R_OK;
    }
    else if (streq(token, "userpin")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        config.user_pin = atol(token);
        auth_check_token(config.user_pin); // authorize with new pin
        return R_OK;
    }
    else if (streq(token, "clearlog")) {
        eeprom_erase_full(false);
        return R_OK;
    }
    else if (streq(token, "reboot")) {
        NVIC_SystemReset(); // trigger NVIC system reset
        return R_OK_SILENT; // to suppress warning
    }
    else if (streq(token, "load")) {
        config_load_eeprom();
        return R_OK;
    }
    else if (streq(token, "save")) {
        config_save_eeprom();
        return R_OK;
    }
    else if (streq(token, "default")) {
        config_load_default();
        return R_OK;
    }
    else return R_ERR_SYNTAX;
}


static CMD_RESULT cmd_debug(char **saveptr)
{
    char *token = strtok_r(NULL, CMD_SEPARATOR, saveptr);
    if (!auth_check_req()) return R_ERR_AUTH;
    if (token == NULL) return R_ERR_SYNTAX;
    else if (streq(token, "status")) {
        char buffer[SYSLOG_MAX_LENGTH];
        syslog_read_config(buffer);
        printf_debug("\r%s\r", buffer);
        syslog_read_nvinfo(buffer);
        printf_debug("\r%s\r", buffer);
        return R_OK_SILENT;
    }
    else if (streq(token, "reset")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "nvic")) {
            NVIC_SystemReset(); // trigger NVIC system reset
            return R_OK_SILENT; // to suppress warning
        }
        else if (streq(token, "watchdog")) {
            while (1) {} // wait for watchdog reset, system halted
        }
        else if (streq(token, "fault")) {
            void (*fn)() = (void*)0x0700000;
            fn(); // trigger HardFault with invalid jump
            return R_OK_SILENT; // to suppress warning
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "sendjpeg")) {
        enable_turbo(true);
        if (!camera_snapshot()) img.length = 0;
        enable_turbo(false);

        HAL_UART_Transmit(&huart2, (char*)(&img.length), sizeof(img.length), HAL_MAX_DELAY);
        uint8_t *ptr = jpeg;
        uint32_t len = img.length;
        while (len > 0) {
            uint32_t len_next = len > 4096 ? 4096 : len;
            HAL_UART_Transmit(&huart2, ptr, len_next, HAL_MAX_DELAY);
            ptr += len_next;
            len -= len_next;
            HAL_IWDG_Refresh(&hiwdg);
        }
        return R_OK_SILENT;
    }
    else if (streq(token, "eeprom")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "fullerase")) {
            eeprom_erase_full(true);
            return R_OK;
        }
        else if (streq(token, "dump")) {
            uint16_t addr = 0;
            while (addr < 0x0200) {
                uint8_t buffer[32];
                char s[200];
                eeprom_read(addr, buffer, sizeof(buffer));
                s[0] = '\0';
                for (uint8_t i = 0; i < sizeof(buffer); i++) {
                    sprintf(s, "%s%02X ", s, buffer[i]);
                }
                printf_debug(s);
                addr += sizeof(buffer);
                HAL_IWDG_Refresh(&hiwdg);
            }
            return R_OK_SILENT;
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "adc")) {
        if ((token = strtok_r(NULL, CMD_SEPARATOR, saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "voltage")) {
            printf_debug("Voltage = %u mV", adc_read_voltage());
            return R_OK_SILENT;
        }
        else if (streq(token, "temp")) {
            printf_debug("Temperature = %d'C", adc_read_temperature());
            return R_OK_SILENT;
        }
        else return R_ERR_SYNTAX;
    }
    else return R_ERR_SYNTAX;
}


void cmd_handler(char *cmd)
{
    char *token, *saveptr;
    CMD_RESULT result = R_ERR_SYNTAX;
    set_led_yellow(true);
    token = strtok_r(cmd, CMD_SEPARATOR, &saveptr);

    if (streq(token, "sstv")) {
        result = cmd_sstv(&saveptr);
    }
    else if (streq(token, "psk")) {
        result = cmd_psk(&saveptr);
    }
    else if (streq(token, "cw")) {
        result = cmd_cw(&saveptr);
    }
    else if (streq(token, "auth")) {
        result = cmd_auth(&saveptr);
    }
    else if (streq(token, "camcfg")) {
        result = cmd_camcfg(&saveptr);
    }
    else if (streq(token, "debug")) {
        result = cmd_debug(&saveptr);
    }
    else if (*token == '\0') {
        result = R_OK_SILENT;
    }

    set_led_yellow(false);
    send_downlink(result);
}


void start_in_task(void)
{
    bool start_new = HAL_GPIO_ReadPin(GPIO_START_GPIO_Port, GPIO_START_Pin) == GPIO_PIN_SET;
    static bool start_old;
    uint8_t mode = 0;
    bool start = false;

    mode |= (HAL_GPIO_ReadPin(GPIO_M0_GPIO_Port, GPIO_M0_Pin) == GPIO_PIN_SET) ? 1 : 0;
    mode |= (HAL_GPIO_ReadPin(GPIO_M1_GPIO_Port, GPIO_M1_Pin) == GPIO_PIN_SET) ? 2 : 0;

    if ((config.start_edge & EDGE_RISING) && (start_new && !start_old)) start = true;
    if ((config.start_edge & EDGE_FALLING) && (!start_new && start_old)) start = true;

    if (start) cmd_handler_const(config.mode_cmd[mode]);

    start_old = HAL_GPIO_ReadPin(GPIO_START_GPIO_Port, GPIO_START_Pin) == GPIO_PIN_SET;
}


void main_satcam()
{
#if ENABLE_SWD_DEBUG
    /* enable debug access */
    HAL_DBGMCU_EnableDBGSleepMode();
    __HAL_DBGMCU_FREEZE_IWDG();
#endif

    /* initialize peripherals */
    set_led_red(true);

    eeprom_init();
    comm_init(); // last

    if (config_load_eeprom()) send_downlink(R_BOOT_OK);
    else send_downlink(R_BOOT_ERR);

    syslog_event(LOG_BOOT);

    set_led_red(false);

    /* main loop */
    while (1) {
        /* tasks */
        comm_cmd_task();
        start_in_task();

        /* autoreboot */
        if ((config.autoreboot >= 120) && (HAL_GetTick()/1000 > config.autoreboot)) NVIC_SystemReset();

        /* watchdog reset */
        HAL_IWDG_Refresh(&hiwdg);

        /* fix bug with UART DMA error handler */
        comm_init();
    }
}
