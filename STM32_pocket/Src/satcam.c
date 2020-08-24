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

IMPORT_BIN("Inc/sstv_monoscope.jpg", uint8_t, img_monoscope);

uint8_t *images[] = {
    img_monoscope,
};

bool service_enable = false;

/* global configuration variables, linked from eeprom.c */
CONFIG_SYSTEM config;
const CONFIG_SYSTEM config_default = {
    .command = 0,
    .save_page = 0,
    .save_count = 0,
    .save_delay = 0,
    .save_light_lo = 0,
    .save_light_hi = 0,
    .spl_light_delay = 0,
    .spl_volt_delay = 0,
    .spl_temp_delay = 0,
    .psk_append = 0,
    .cam_agc = true,
    .cam_aec = true,
    .cam_agc_ceiling = 16,
    .cam_agc_manual = 0,
    .cam_aec_manual = 0,
    .cam_awb = AWB_SUNNY,
    .cam_rotate = false,
    .psk_speed = PSK_SPEED2,
    .psk_freq = PSK_FREQ,
    .cw_wpm = CW_WPM,
    .cw_freq = CW_FREQ,
    .callsign = "SatCam",
    .sys_i2c_watchdog = 0,
    .sys_autoreboot = 43200,
    .cam_delay = 1000,
    .cam_qs = 5,
    .sstv_ampl = (int16_t)(0.9 * 32767),
    .psk_ampl = (int16_t)(0.4 * 32767),
    .cw_ampl = (int16_t)(0.9 * 32767),
    .debug_enable = 1,
    .light_cal = 512,
};


static bool camera_snapshot(void)
{
    CONFIG_CAMERA cam = {
        .delay = config.cam_delay,
        .qs = config.cam_qs,
        .agc = config.cam_agc ? true : false,
        .aec = config.cam_aec ? true : false,
        .agc_ceiling = config.cam_agc_ceiling,
        .agc_manual = config.cam_agc_manual,
        .aec_manual = config.cam_aec_manual,
        .awb = config.cam_awb,
        .rotate = config.cam_rotate ? true : false,
    };

    set_led_red(true);
    if (!ov2640_hilevel_init(&cam)) {
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
    uint16_t light = adc_read_light();
    snprintf(img.overlay[OVERLAY_HEADER], sizeof(img.overlay[OVERLAY_HEADER]), "%s +%05u %d\037C #%04u %02ulx%u %umV", config.callsign,
        (unsigned int)(HAL_GetTick() / 60000), adc_read_temperature(), (unsigned int)img_counter, light % 100, light / 100, voltage
    );

    snprintf(img.overlay[OVERLAY_IMG], sizeof(img.overlay[OVERLAY_IMG]), "#%04u %uB g%u e%u", (unsigned int)img_counter, (int)img.length, agc, aec);

    return true;
}


static void send_roger(void)
{
    if (config.debug_enable) {
        audio_start();
        audio_morse(CW_WPM, CW_FREQ, "R");
        audio_stop();
    }
}


static void cmd_sstv(uint8_t param, uint8_t mode)
{
    if (param == 0) { // snapshot camera (live)
        /* start SSTV here */
        enable_turbo(true); // peak 18% CPU
        bool ok = camera_snapshot();
        if (ok) ok = jpeg_test(jpeg, img.length);
        if (ok) {
            sstv_set_overlay(OVERLAY_HEADER, img.overlay[OVERLAY_HEADER]);
            sstv_set_overlay(OVERLAY_IMG, img.overlay[OVERLAY_IMG]);
            sstv_set_overlay(OVERLAY_LARGE, NULL);
            sstv_set_overlay(OVERLAY_FROM, config.callsign);
            sstv_play_jpeg(jpeg, mode);
        }
        enable_turbo(false);
    }
    else if (param >= 1 && param <= 16) { // Flash images
        uint8_t sector = param - 1;

        /* load image from FLASH: sector */
        flash_read(ADDR_JPEGIMAGE(sector), jpeg, 0xFFFF);
        flash_read(ADDR_FLASHINFO(sector), (uint8_t*)(&img), sizeof(img));

        /* send image: mode, overlay */
        if (img.length != 0 && img.length != 0xFFFFFFFF) {
            // change last parameter (voltage) to flash memory number
            sprintf(strrchr(img.overlay[OVERLAY_HEADER], ' '), " F#%u", sector);
            sstv_set_overlay(OVERLAY_HEADER, img.overlay[OVERLAY_HEADER]);
            sstv_set_overlay(OVERLAY_IMG, img.overlay[OVERLAY_IMG]);
            sstv_set_overlay(OVERLAY_LARGE, NULL);
            sstv_set_overlay(OVERLAY_FROM, config.callsign);
            enable_turbo(true); // peak 18% CPU
            sstv_play_jpeg(jpeg, mode);
            enable_turbo(false);
        }
    }
    else if (param == 17) { // Flash thumbnails
        /* send thumbnails: mode, no overlay */
        snprintf(img.overlay[OVERLAY_HEADER], sizeof(img.overlay[OVERLAY_HEADER]), "%s +%05u %d\037C thmbs %umV", config.callsign,
            (unsigned int)(HAL_GetTick() / 60000), adc_read_temperature(), adc_read_voltage()
        );
        sstv_set_overlay(OVERLAY_HEADER, img.overlay[OVERLAY_HEADER]);
        sstv_set_overlay(OVERLAY_IMG, NULL);
        sstv_set_overlay(OVERLAY_LARGE, NULL);
        sstv_set_overlay(OVERLAY_FROM, NULL);
        enable_turbo(true); // peak 31% CPU
        sstv_play_thumbnail(mode);
        enable_turbo(false);
    }
    else if (param >= 18 && param <= (18+(sizeof(images)/sizeof(images[0])))) { // ROM images
        uint8_t sector = param - 18;

        /* send image: mode, overlay */
        snprintf(img.overlay[OVERLAY_HEADER], sizeof(img.overlay[OVERLAY_HEADER]), "%s +%05u %d\037C %umV ROM #%u", config.callsign,
            (unsigned int)(HAL_GetTick() / 60000), adc_read_temperature(), adc_read_voltage(), sector
        );
        sstv_set_overlay(OVERLAY_HEADER, img.overlay[OVERLAY_HEADER]);
        sstv_set_overlay(OVERLAY_IMG, NULL);
        sstv_set_overlay(OVERLAY_LARGE, NULL);
        sstv_set_overlay(OVERLAY_FROM, config.callsign);

        enable_turbo(true); // peak 18% CPU
        sstv_play_jpeg(images[sector], mode);
        enable_turbo(false);
    }

    if (config.psk_append) {
        char buffer[SYSLOG_MAX_LENGTH];
        syslog_read_telemetry(buffer);

        /* start PSK here */
        enable_turbo(true); // peak 18% CPU
        audio_start();
        audio_psk(config.psk_append, config.psk_freq, buffer); // 76% CPU without turbo, 4% CPU with turbo
        audio_stop();
        enable_turbo(false);
    }
}


static void cmd_psk(uint8_t param)
{
    char buffer[SYSLOG_MAX_LENGTH];

    switch (param) {
        case 0: // hello
            snprintf(buffer, sizeof(buffer), "Greetings from %s, up %umin", config.callsign, (unsigned int)(HAL_GetTick() / 60000));
            break;

        case 1: // config
            syslog_read_config(buffer);
            break;

        case 2: // nvinfo
            syslog_read_nvinfo(buffer);
            break;

        case 3: // telemetry
            syslog_read_telemetry(buffer);
            break;

        case 4 ... 6: // samples all
            syslog_read_samples(buffer, param-4, MAX_SAMPLES);
            break;

        case 7 ... 9: // samples 32
            syslog_read_samples(buffer, param-7, 32);
            break;

        default:
            return;
    }

    /* start PSK here */
    enable_turbo(true); // peak 18% CPU
    audio_start();
    audio_psk(config.psk_speed, config.psk_freq, buffer); // 76% CPU without turbo, 4% CPU with turbo
    audio_stop();
    enable_turbo(false);
}


static void cmd_cw(uint8_t param)
{
    char buffer[CW_MAX_LENGTH];

    if (param == 0) { // hello
        snprintf(buffer, sizeof(buffer), "73 DE %s %s %s K", config.callsign, config.callsign, config.callsign);
    }
    else return;

    /* start CW here */
    audio_start();
    audio_morse(config.cw_wpm, config.cw_freq, buffer);
    audio_stop();
}


static void cmd_service(uint8_t param)
{
    if (!service_enable) return;

    switch (param) {
        case 0: // eeprom clearlog
            eeprom_erase_full(false);
            send_roger();
            break;

        case 1: // reboot by nvic
            NVIC_SystemReset(); // trigger NVIC system reset
            break;

        case 2: // eeprom load
            config_load_eeprom();
            send_roger();
            break;

        case 3: // eeprom save
            config_save_eeprom();
            send_roger();
            break;

        case 4: // eeprom default
            config_load_default();
            send_roger();
            break;

        case 200: // reboot by watchdog
            while (1) {} // wait for watchdog reset, system halted
            break;

        case 201: // reboot by fault
            ((void (*)())(0x0700000))(); // trigger HardFault with invalid jump
            break;

        case 210: // eeprom fullerase
            eeprom_erase_full(true);
            send_roger();
            break;

        case 220: // flash erase all
            flash_erase_bulk();
            send_roger();
            break;

        case 221 ... 236: // flash erase sector
            flash_erase_sector(ADDR_JPEGIMAGE(param - 221));
            flash_erase_sector(ADDR_THUMBNAIL(param - 221));
            send_roger();
            break;

        default: // service disable
            service_enable = false;
            send_roger();
            break;
    }
}


void cmd_handler(uint16_t command)
{
    uint8_t cmd = (command) & 0xFF;
    uint8_t param = (command >> 8) & 0xFF;


    switch (cmd) {
        case 1: cmd_sstv(param, 36); break;
        case 2: cmd_sstv(param, 72); break;
        case 3: cmd_sstv(param, 73); break;
        case 4: cmd_sstv(param, 115); break;
        case 5: cmd_psk(param); break;
        case 6: cmd_cw(param); break;
        case 254: cmd_service(param); break;
    }
}


void save_task(void)
{
    static uint32_t last;

    if (config.save_count == 0) return;

    if (HAL_GetTick() >= last + (config.save_delay*1000)) {
        last = HAL_GetTick();

        /* check light */
        uint16_t light = adc_read_light();
        if (config.save_light_lo && light < config.save_light_lo) return;
        if (config.save_light_hi && light > config.save_light_hi) return;

        /* do CAM snapshot here */
        uint8_t *thumbnail;
        enable_turbo(true);
        bool ok = camera_snapshot();
        if (ok) ok = jpeg_thumbnail(jpeg, &thumbnail); // 4060ms without turbo, 205ms with turbo
        if (ok) {
            if (flash_erase_sector(ADDR_JPEGIMAGE(config.save_page))) {
                flash_program(ADDR_JPEGIMAGE(config.save_page), jpeg, img.length);
            }
            if (flash_erase_sector(ADDR_THUMBNAIL(config.save_page))) {
                flash_program(ADDR_THUMBNAIL(config.save_page), thumbnail, 80*60*3);
                flash_program(ADDR_FLASHINFO(config.save_page), (uint8_t*)(&img), sizeof(img));
            }
        }
        enable_turbo(false);

        if (++config.save_page >= 16) config.save_page = 0;
        config.save_count--;
        last = HAL_GetTick(); // update delay with elapsed time
    }
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

    config_load_eeprom();

    syslog_event(LOG_BOOT);
    send_roger();
    set_led_red(false);

    /* main loop */
    while (1) {
        /* tasks */
        comm_cmd_task();
        save_task();
        sampling_task();

        /* watchdog reset */
        HAL_IWDG_Refresh(&hiwdg);
    }
}
