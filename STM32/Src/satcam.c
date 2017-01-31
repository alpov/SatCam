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
#include "m25p16.h"
#include "sstv.h"
#include "eeprom.h"

static uint8_t jpeg[IMG_BUFFER_SIZE];
static struct {
    uint32_t length;
    char overlay[2][TEXT_LEN];
} img;

static bool startup_done = false;
static uint32_t last_cmd_tick = 0;

IMPORT_BIN("Inc/sstv_monoscope.jpg", uint8_t, img_monoscope);

uint8_t *images[] = {
    img_monoscope,
};

/* global configuration variables, linked from eeprom.c */
CONFIG_SYSTEM config;
CONFIG_PLAN plan;
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
    },
    .sstv_keep_rx = true,
    .auth_req = AUTH_AUTH_SET + AUTH_CAMCFG + AUTH_CAMCFG_STARTUP + AUTH_CAMCFG_SAVE + AUTH_DEBUG + AUTH_MULTI_HIGH_DUTY + AUTH_TCMD,
    .idle_time = 60,
    .startup_cmd = "SSTV.SAVE.0.8.30",
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

    uint32_t img_counter = syslog_get_counter(LOG_CAM_SNAPSHOT) % 1000;
    uint16_t light = adc_read_light();
    snprintf(img.overlay[OVERLAY_HEADER], sizeof(img.overlay[OVERLAY_HEADER]), CALLSIGN_SSTV_PSK " +%05u %d\037C #%03u %02ulx%u",
        (unsigned int)(HAL_GetTick() / 60000), adc_read_temperature(), (unsigned int)img_counter, light % 100, light / 100
    );

    snprintf(img.overlay[OVERLAY_IMG], sizeof(img.overlay[OVERLAY_IMG]), "%uB g%u e%u", (int)img.length, agc, aec);

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
        case R_TX_DENIED:
            text = "TX denied";
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
    if (cw && psk_request(PSK_CMD_TX_KEEP_RX)) {
        audio_start();
        audio_morse(CW_WPM, CW_FREQ, cw);
        audio_stop();
        psk_request(PSK_CMD_STOP_TX);
    }
}


void plan_task(void)
{
    static uint32_t tick_last = 0;
    uint32_t tick_curr = HAL_GetTick();

    // this check need to test the clock rollover
    if ((tick_curr < (tick_last + 1000)) && (tick_curr >= tick_last)) return;
    tick_last += 1000; // check on each elapsed 1 second

    set_led_yellow(true);
    // printf_debug("Tick! %u %u", tick_curr, tick_last);

    if (plan.sstv_live.count > 0) {
        if (plan.sstv_live.delay_curr > 0) plan.sstv_live.delay_curr--;
        else {
            uint32_t task_start = HAL_GetTick();
            plan.sstv_live.delay_curr = plan.sstv_live.delay_next;
            plan.sstv_live.count--;

            /* start SSTV here */
            enable_turbo(true); // peak 18% CPU
            bool ok = camera_snapshot();
            if (ok) ok = jpeg_test(jpeg, img.length);
            if (ok) {
                sstv_set_overlay(OVERLAY_HEADER, img.overlay[OVERLAY_HEADER]);
                sstv_set_overlay(OVERLAY_IMG, img.overlay[OVERLAY_IMG]);
                sstv_set_overlay(OVERLAY_LARGE, NULL);
                sstv_set_overlay(OVERLAY_FROM, CALLSIGN_SSTV_PSK);
                if (psk_request(config.sstv_keep_rx ? PSK_CMD_TX_KEEP_RX : PSK_CMD_TX_NO_RX)) {
                    sstv_play_jpeg(jpeg, plan.sstv_live.mode);
                    psk_request(PSK_CMD_STOP_TX);
                }
            }
            enable_turbo(false);
            plan.sstv_live.delay_curr += (HAL_GetTick() - task_start) / 1000 + 1; // add elapsed time to delay
            send_downlink(R_QUEUED);
        }
    }
    if (plan.sstv_save.count > 0) {
        if (plan.sstv_save.delay_curr > 0) plan.sstv_save.delay_curr--;
        else {
            uint32_t task_start = HAL_GetTick();
            plan.sstv_save.delay_curr = plan.sstv_save.delay_next;

            /* check light */
            uint16_t light = adc_read_light();
            if (plan.sstv_save.light_low && light < plan.sstv_save.light_low) return;
            if (plan.sstv_save.light_high && light > plan.sstv_save.light_high) return;

            plan.sstv_save.count--;

            /* do CAM snapshot here */
            uint8_t sector = plan.sstv_save.page;
            uint8_t *thumbnail;
            enable_turbo(true);
            bool ok = camera_snapshot();
            if (ok) ok = jpeg_thumbnail(jpeg, &thumbnail); // 4060ms without turbo, 205ms with turbo
            if (ok) {
                if (flash_erase_sector(ADDR_JPEGIMAGE(sector))) {
                    flash_program(ADDR_JPEGIMAGE(sector), jpeg, img.length);
                }
                if (flash_erase_sector(ADDR_THUMBNAIL(sector))) {
                    flash_program(ADDR_THUMBNAIL(sector), thumbnail, 80*60*3);
                    flash_program(ADDR_FLASHINFO(sector), (uint8_t*)(&img), sizeof(img));
                }
            }
            enable_turbo(false);

            if (++plan.sstv_save.page >= 16) plan.sstv_save.page = 0;
            if (plan.sstv_save.delay_curr < 30) last_cmd_tick = HAL_GetTick(); // ignore auto PSK commands for short measurement intervals
            plan.sstv_save.delay_curr += (HAL_GetTick() - task_start) / 1000 + 1; // add elapsed time to delay
            send_downlink(R_QUEUED);
        }
    }
    if (plan.psk.count > 0) {
        if (plan.psk.delay_curr > 0) plan.psk.delay_curr--;
        else {
            uint32_t task_start = HAL_GetTick();
            plan.psk.delay_curr = plan.psk.delay_next;
            plan.psk.count--;
            switch (plan.psk.what) {
                case PSK_MESSAGE:
                    // message already in buffer
                    break;
                case PSK_CONFIG:
                    syslog_read_config(plan.psk.buffer);
                    break;
                case PSK_NVINFO:
                    syslog_read_nvinfo(plan.psk.buffer);
                    break;
                case PSK_TLM:
                    syslog_read_telemetry(plan.psk.buffer);
                    break;
                case PSK_LIGHT:
                    syslog_read_light(plan.psk.buffer);
                    break;
            }

            /* start PSK here */
            if (psk_request((plan.psk.what == PSK_TLM) ? PSK_CMD_TX_IDLE : PSK_CMD_TX_KEEP_RX)) {
                enable_turbo(true); // peak 18% CPU
                audio_start();
                audio_psk(plan.psk.speed, plan.psk.freq, plan.psk.buffer); // 76% CPU without turbo, 4% CPU with turbo
                audio_stop();
                enable_turbo(false);
                psk_request(PSK_CMD_STOP_TX);
            }
            plan.psk.delay_curr += (HAL_GetTick() - task_start) / 1000 + 1; // add elapsed time to delay
            send_downlink(R_QUEUED);
        }
    }
    if (plan.cw.count > 0) {
        if (plan.cw.delay_curr > 0) plan.cw.delay_curr--;
        else {
            uint32_t task_start = HAL_GetTick();
            plan.cw.delay_curr = plan.cw.delay_next;
            plan.cw.count--;

            /* start CW here */
            if (psk_request(PSK_CMD_TX_KEEP_RX)) {
                audio_start();
                audio_morse(plan.cw.wpm, plan.cw.freq, plan.cw.buffer);
                audio_stop();
                psk_request(PSK_CMD_STOP_TX);
            }
            plan.cw.delay_curr += (HAL_GetTick() - task_start) / 1000 + 1; // add elapsed time to delay
            send_downlink(R_QUEUED);
        }
    }
    if (plan.light.count > 0) {
        if (plan.light.delay_curr > 0) plan.light.delay_curr--;
        else {
            plan.light.delay_curr = plan.light.delay_next;
            plan.light.count--;

            /* measure light here */
            plan.light.samples[plan.light.idx++] = adc_read_light();
            if (plan.light.delay_curr < 30) last_cmd_tick = HAL_GetTick(); // ignore auto PSK commands for short measurement intervals
            send_downlink(R_QUEUED);
        }
    }
    if (plan.auth) plan.auth--;

    HAL_Delay(1);
    set_led_yellow(false);
}


static CMD_RESULT cmd_sstv(char **saveptr)
{
    char *token = strtok_r(NULL, ".", saveptr);
    if (!auth_check_req(AUTH_SSTV)) return R_ERR_AUTH;
    if (token == NULL) return R_ERR_SYNTAX;
    else if (streq(token, "live")) {
        if (!auth_check_req(AUTH_SSTV_LIVE)) return R_ERR_AUTH;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        plan.sstv_live.mode = atol(token);
        plan.sstv_live.count = 1;
        plan.sstv_live.delay_curr = 0;
        plan.sstv_live.delay_next = 0;
        /* queued SSTV transmission */

        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK_SILENT;
        if (!auth_check_req(AUTH_SSTV_LIVE_MULTI)) return R_ERR_AUTH;
        plan.sstv_live.count = atol(token);
        plan.sstv_live.delay_next = MIN_MULTI_DELAY;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK_SILENT;
        uint16_t delay = atol(token);
        if (delay < MIN_MULTI_DELAY && !auth_check_req(AUTH_MULTI_HIGH_DUTY)) return R_ERR_AUTH;
        plan.sstv_live.delay_next = delay;
        return R_OK_SILENT;
    }
    else if (streq(token, "save")) {
        if (!auth_check_req(AUTH_SSTV_SAVE)) return R_ERR_AUTH;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        plan.sstv_save.page = atol(token);
        plan.sstv_save.count = 1;
        plan.sstv_save.delay_curr = 0;
        plan.sstv_save.delay_next = 0;
        plan.sstv_save.light_low = 0;
        plan.sstv_save.light_high = 0;
        /* queued camera snapshot */

        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK;
        if (!auth_check_req(AUTH_SSTV_SAVE_MULTI)) return R_ERR_AUTH;
        plan.sstv_save.count = atol(token);
        plan.sstv_save.delay_next = MIN_MULTI_DELAY;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK;
        uint16_t delay = atol(token);
        // allow high duty with authorization OR for less than 16 images
        if (delay < MIN_MULTI_DELAY && plan.sstv_save.count > 16 && !auth_check_req(AUTH_MULTI_HIGH_DUTY)) return R_ERR_AUTH;
        plan.sstv_save.delay_next = delay;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK;
        plan.sstv_save.light_low = atol(token);
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK;
        plan.sstv_save.light_high = atol(token);
        return R_OK;
    }
    else if (streq(token, "load")) {
        if (!auth_check_req(AUTH_SSTV_LOAD)) return R_ERR_AUTH;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        uint8_t mode = atol(token);
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        uint8_t sector = atol(token);
        char *overlay = NULL;
        if ((token = strtok_r(NULL, ".", saveptr)) != NULL) overlay = token;

        /* load image from FLASH: sector */
        flash_read(ADDR_JPEGIMAGE(sector), jpeg, 0xFFFF);
        flash_read(ADDR_FLASHINFO(sector), (uint8_t*)(&img), sizeof(img));

        /* send image: mode, overlay */
        if (img.length != 0 && img.length != 0xFFFFFFFF) {
            // add memory number
            snprintf(img.overlay[OVERLAY_HEADER], sizeof(img.overlay[OVERLAY_HEADER]), "%s F#%u",
                img.overlay[OVERLAY_HEADER], sector
            );
            sstv_set_overlay(OVERLAY_HEADER, img.overlay[OVERLAY_HEADER]);
            sstv_set_overlay(OVERLAY_IMG, img.overlay[OVERLAY_IMG]);
            sstv_set_overlay(OVERLAY_LARGE, overlay);
            sstv_set_overlay(OVERLAY_FROM, CALLSIGN_SSTV_PSK);
            if (!psk_request(config.sstv_keep_rx ? PSK_CMD_TX_KEEP_RX : PSK_CMD_TX_NO_RX)) return R_TX_DENIED;
            enable_turbo(true); // peak 18% CPU
            sstv_play_jpeg(jpeg, mode);
            enable_turbo(false);
            psk_request(PSK_CMD_STOP_TX);
        }
        return R_OK_SILENT;
    }
    else if (streq(token, "rom")) {
        if (!auth_check_req(AUTH_SSTV_ROM)) return R_ERR_AUTH;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        uint8_t mode = atol(token);
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        uint8_t sector = atol(token);
        char *overlay = NULL;
        if ((token = strtok_r(NULL, ".", saveptr)) != NULL) overlay = token;

        /* load image from ROM: check sector number */
        if (sector >= sizeof(images)/sizeof(images[0])) return R_ERR_SYNTAX;

        /* send image: mode, overlay */
        snprintf(img.overlay[OVERLAY_HEADER], sizeof(img.overlay[OVERLAY_HEADER]), CALLSIGN_SSTV_PSK " +%05u %d\037C ROM #%u",
            (unsigned int)(HAL_GetTick() / 60000), adc_read_temperature(), sector
        );
        sstv_set_overlay(OVERLAY_HEADER, img.overlay[OVERLAY_HEADER]);
        sstv_set_overlay(OVERLAY_IMG, NULL);
        sstv_set_overlay(OVERLAY_LARGE, overlay);
        sstv_set_overlay(OVERLAY_FROM, CALLSIGN_SSTV_PSK);
        if (!psk_request(config.sstv_keep_rx ? PSK_CMD_TX_KEEP_RX : PSK_CMD_TX_NO_RX)) return R_TX_DENIED;
        enable_turbo(true); // peak 18% CPU
        sstv_play_jpeg(images[sector], mode);
        enable_turbo(false);
        psk_request(PSK_CMD_STOP_TX);
        return R_OK_SILENT;
    }
    else if (streq(token, "thumbnails")) {
        if (!auth_check_req(AUTH_SSTV_THUMBS)) return R_ERR_AUTH;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        uint8_t mode = atol(token);

        /* send thumbnails: mode, no overlay */
        snprintf(img.overlay[OVERLAY_HEADER], sizeof(img.overlay[OVERLAY_HEADER]), CALLSIGN_SSTV_PSK " +%05u %d\037C thumbnails",
            (unsigned int)(HAL_GetTick() / 60000), adc_read_temperature()
        );
        sstv_set_overlay(OVERLAY_HEADER, img.overlay[OVERLAY_HEADER]);
        sstv_set_overlay(OVERLAY_IMG, NULL);
        sstv_set_overlay(OVERLAY_LARGE, NULL);
        sstv_set_overlay(OVERLAY_FROM, NULL);
        if (!psk_request(config.sstv_keep_rx ? PSK_CMD_TX_KEEP_RX : PSK_CMD_TX_NO_RX)) return R_TX_DENIED;
        enable_turbo(true); // peak 31% CPU
        sstv_play_thumbnail(mode);
        enable_turbo(false);
        psk_request(PSK_CMD_STOP_TX);
        return R_OK_SILENT;
    }
    else if (streq(token, "killplan")) {
        plan.sstv_live.count = 0;
        plan.sstv_save.count = 0;
        return R_OK;
    }
    else return R_ERR_SYNTAX;
}


static CMD_RESULT cmd_psk(char **saveptr)
{
    char *token = strtok_r(NULL, ".", saveptr);
    if (!auth_check_req(AUTH_PSK)) return R_ERR_AUTH;
    if (streq(token, "nvinfo") && auth_check_req(AUTH_PSK_LOG)) {
        plan.psk.what = PSK_NVINFO;
    }
    else if (streq(token, "config") && auth_check_req(AUTH_PSK_LOG)) {
        plan.psk.what = PSK_CONFIG;
    }
    else if (streq(token, "tlm")) {
        plan.psk.what = PSK_TLM;
    }
    else if (streq(token, "light") && auth_check_req(AUTH_PSK_LOG)) {
        plan.psk.what = PSK_LIGHT;
    }
    else if (streq(token, "sample")) {
        if (!auth_check_req(AUTH_PSK_LIGHT)) return R_ERR_AUTH;
        plan.light.idx = 0;
        plan.light.count = LIGHT_MAX_SAMPLES;
        plan.light.delay_curr = 0;
        plan.light.delay_next = 0;
        memset(plan.light.samples, 0, sizeof(plan.light.samples));

        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK;
        if (atol(token) > LIGHT_MAX_SAMPLES) return R_ERR_SYNTAX;
        plan.light.count = atol(token);
        plan.light.delay_next = MIN_MULTI_DELAY;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK;
        plan.light.delay_next = atol(token);
        return R_OK;
    }
    else if (streq(token, "killplan")) {
        plan.psk.count = 0;
        plan.light.count = 0;
        return R_OK;
    }
    else if (token == NULL) {
        plan.psk.what = PSK_MESSAGE;
        snprintf(plan.psk.buffer, sizeof(plan.psk.buffer), "Greetings from " CALLSIGN_SSTV_PSK ", up %umin", (unsigned int)(HAL_GetTick() / 60000));
    }
    else {
        plan.psk.what = PSK_MESSAGE;
        snprintf(plan.psk.buffer, sizeof(plan.psk.buffer), CALLSIGN_SSTV_PSK " %s", token);
    }

    plan.psk.speed = PSK_SPEED;
    plan.psk.freq = PSK_FREQ;
    plan.psk.count = 1;
    plan.psk.delay_curr = 0;
    plan.psk.delay_next = 0;
    /* queued PSK transmission */

    if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK_SILENT;
    plan.psk.speed = atol(token);
    if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK_SILENT;
    plan.psk.freq = atol(token);

    if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK_SILENT;
    if (!auth_check_req(AUTH_PSK_MULTI)) return R_ERR_AUTH;
    plan.psk.count = atol(token);
    plan.psk.delay_next = MIN_MULTI_DELAY;
    if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK_SILENT;
    uint16_t delay = atol(token);
    if (delay < MIN_MULTI_DELAY && !auth_check_req(AUTH_MULTI_HIGH_DUTY)) return R_ERR_AUTH;
    plan.psk.delay_next = delay;
    return R_OK_SILENT;
}


static CMD_RESULT cmd_cw(char **saveptr)
{
    char *token = strtok_r(NULL, ".", saveptr);
    if (!auth_check_req(AUTH_CW)) return R_ERR_AUTH;
    if (token == NULL) {
        snprintf(plan.cw.buffer, sizeof(plan.cw.buffer), "73 DE " CALLSIGN_CW " " CALLSIGN_CW " " CALLSIGN_CW " K");
    }
    else if (streq(token, "killplan")) {
        plan.cw.count = 0;
        return R_OK;
    }
    else {
        snprintf(plan.cw.buffer, sizeof(plan.cw.buffer), CALLSIGN_CW " %s", token);
    }

    plan.cw.wpm = CW_WPM;
    plan.cw.freq = CW_FREQ;
    plan.cw.count = 1;
    plan.cw.delay_curr = 0;
    plan.cw.delay_next = 0;
    /* queued CW transmission */

    if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK_SILENT;
    plan.cw.wpm = atol(token);
    if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK_SILENT;
    plan.cw.freq = atol(token);

    if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK_SILENT;
    if (!auth_check_req(AUTH_CW_MULTI)) return R_ERR_AUTH;
    plan.cw.count = atol(token);
    plan.cw.delay_next = MIN_MULTI_DELAY;
    if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_OK_SILENT;
    uint16_t delay = atol(token);
    if (delay < MIN_MULTI_DELAY && !auth_check_req(AUTH_MULTI_HIGH_DUTY)) return R_ERR_AUTH;
    plan.cw.delay_next = delay;
    return R_OK_SILENT;
}


static CMD_RESULT cmd_auth(char **saveptr)
{
    char *token = strtok_r(NULL, ".", saveptr);
    if (token == NULL) return R_ERR_SYNTAX;
    else if (streq(token, "set")) {
        if (!auth_check_req(AUTH_AUTH_SET)) return R_ERR_AUTH;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        config.auth_req = atol(token);
        return R_OK;
    }
    else {
        /* either already authorized or token verified */
        if (!plan.auth && !auth_check_token(atol(token))) return R_ERR_AUTH;
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        plan.auth = atol(token);
        return R_OK;
    }
}


static CMD_RESULT cmd_camcfg(char **saveptr)
{
    char *token = strtok_r(NULL, ".", saveptr);
    if (!auth_check_req(AUTH_CAMCFG)) return R_ERR_AUTH;
    if (token == NULL) return R_ERR_SYNTAX;
    else if (streq(token, "delay")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        config.cam.delay = atol(token);
        return R_OK;
    }
    else if (streq(token, "qs")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        config.cam.qs = atol(token);
        return R_OK;
    }
    else if (streq(token, "agc")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "ceiling")) {
            if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
            config.cam.agc = true;
            config.cam.agc_ceiling = atol(token);
            return R_OK;
        }
        else if (streq(token, "manual")) {
            if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
            config.cam.agc = false;
            config.cam.agc_manual = atol(token);
            return R_OK;
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "aec")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "auto")) {
            config.cam.aec = true;
            return R_OK;
        }
        else if (streq(token, "manual")) {
            if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
            config.cam.aec = false;
            config.cam.aec_manual = atol(token);
            return R_OK;
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "awb")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
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
    else if (streq(token, "rx")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "disable")) {
            config.sstv_keep_rx = false;
            return R_OK;
        }
        else if (streq(token, "keep")) {
            config.sstv_keep_rx = true;
            return R_OK;
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "idle")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        config.idle_time = atol(token);
        return R_OK;
    }
    else if (streq(token, "startup")) {
        if (!auth_check_req(AUTH_CAMCFG_STARTUP)) return R_ERR_AUTH;
        if (strlen(*saveptr) >= STARTUP_CMD_LENGTH-1) return R_ERR_SYNTAX;
        strncpy(config.startup_cmd, *saveptr, STARTUP_CMD_LENGTH);
        return R_OK;
    }
    else if (streq(token, "load")) {
        config_load_eeprom();
        return R_OK;
    }
    else if (streq(token, "save")) {
        if (!auth_check_req(AUTH_CAMCFG_SAVE)) return R_ERR_AUTH;
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
    char *token = strtok_r(NULL, ".", saveptr);
    if (!auth_check_req(AUTH_DEBUG)) return R_ERR_AUTH;
    if (token == NULL) return R_ERR_SYNTAX;
    else if (streq(token, "status")) {
        syslog_read_config(plan.psk.buffer);
        printf_debug("Config status\r%s", plan.psk.buffer);
        syslog_read_nvinfo(plan.psk.buffer);
        printf_debug("NVinfo status\r%s", plan.psk.buffer);
        syslog_read_telemetry(plan.psk.buffer);
        printf_debug("Telemetry\r%s", plan.psk.buffer);
        return R_OK_SILENT;
    }
    else if (streq(token, "reset")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
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

        HAL_UART_Transmit(&huart3, (char*)(&img.length), sizeof(img.length), HAL_MAX_DELAY);
        uint8_t *ptr = jpeg;
        uint32_t len = img.length;
        while (len > 0) {
            uint32_t len_next = len > 4096 ? 4096 : len;
            HAL_UART_Transmit(&huart3, ptr, len_next, HAL_MAX_DELAY);
            ptr += len_next;
            len -= len_next;
            HAL_IWDG_Refresh(&hiwdg);
        }
        return R_OK_SILENT;
    }
    else if (streq(token, "eeprom")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "erase")) {
            eeprom_erase_full();
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
    else if (streq(token, "flash")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "erase")) {
            if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
            if (streq(token, "all")) {
                flash_erase_bulk();
                return R_OK;
            } else {
                uint8_t sector = atol(token);
                flash_erase_sector(ADDR_JPEGIMAGE(sector));
                flash_erase_sector(ADDR_THUMBNAIL(sector));
                return R_OK;
            }
        }
        else return R_ERR_SYNTAX;
    }
    else if (streq(token, "adc")) {
        if ((token = strtok_r(NULL, ".", saveptr)) == NULL) return R_ERR_SYNTAX;
        if (streq(token, "light")) {
            static const uint32_t base[] = { 1, 10, 100, 1000, 10000, 100000 };
            uint16_t light = adc_read_light();
            printf_debug("Light = %u lux", (light % 100) * base[light / 100]);
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


static CMD_RESULT cmd_tcmd(char **saveptr)
{
    char *token = strtok_r(NULL, ".", saveptr);
    if (!auth_check_req(AUTH_TCMD)) return R_ERR_AUTH;
    if (token == NULL) return R_ERR_SYNTAX;
    else {
        return R_OK_SILENT;
    }
}


void cmd_handler(char *cmd, CMD_SOURCE src)
{
    char *token, *saveptr;
    CMD_RESULT result = R_ERR_SYNTAX;
    token = strtok_r(cmd, ".", &saveptr);

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
    else if (streq(token, "tcmd")) {
        result = cmd_tcmd(&saveptr);
    }
    else if (*token == '\0') {
        result = R_OK_SILENT;
    }

    if (src == SRC_CMD || src == SRC_UPLINK) {
        send_downlink(result);
        last_cmd_tick = HAL_GetTick();
    }
    startup_done = true;
}


void psk_uplink_handler(char cmd)
{
    if (!plan.auth) plan.auth = 1; // all PSK uplink commands are always authorized
    printf_debug("TRX uplink cmd %c", cmd);

}


void psk_auto_handler(char cmd)
{
    static bool rom = true;
    static uint8_t page_load = 0;
    static uint8_t page_rom = 0;
    char s[CMD_MAX_LEN] = "";

    // ignore PSK auto commands if idle time not yet elapsed
    if (!startup_done || config.idle_time == 0 || HAL_GetTick() - last_cmd_tick < config.idle_time * 1000UL) return;
    printf_debug("TRX auto cmd %c", cmd);

    switch (cmd) {
        case PSK_RSP_SSTV_36:
            if (rom) {
                sprintf(s, "SSTV.ROM.36.%u", page_rom++);
                rom = false;
            } else {
                sprintf(s, "SSTV.LOAD.36.%u", page_load++);
                rom = true;
            }
            break;
        case PSK_RSP_SSTV_73:
            if (rom) {
                sprintf(s, "SSTV.ROM.73.%u", page_rom++);
                rom = false;
            } else {
                sprintf(s, "SSTV.LOAD.73.%u", page_load++);
                rom = true;
            }
            break;
        case PSK_RSP_TLM:
            sprintf(s, "PSK.TLM.31." PSK_TLM_FREQ);
            break;
        default:
            return;
    }
    cmd_handler(s, SRC_AUTO);
    if (page_load >= 16) page_load = 0;
    if (page_rom >= sizeof(images)/sizeof(images[0])) page_rom = 0;
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
    flash_init();
    comm_init(); // last

    if (config_load_eeprom()) send_downlink(R_BOOT_OK);
    else send_downlink(R_BOOT_ERR);

    syslog_event(LOG_BOOT);

    set_led_red(false);

    /* main loop */
    while (1) {
        /* execute startup command */
        if (!startup_done && HAL_GetTick() > STARTUP_CMD_DELAY*1000) {
            config.startup_cmd[STARTUP_CMD_LENGTH-1] = '\0';
            if (!plan.auth) plan.auth = 1; // startup command is always authorized
            cmd_handler_const(config.startup_cmd, SRC_STARTUP);
        }

        /* tasks */
        comm_cmd_task();
        comm_psk_task();
        plan_task();

        /* watchdog reset */
        HAL_IWDG_Refresh(&hiwdg);
    }
}
