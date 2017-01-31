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
#include "audio.h"
#include "comm.h"
#include "m25p16.h"
#include "tjpgd.h"
#include "eeprom.h"
#include "sstv.h"

// JPEG decompression engine variables
static uint8_t workspace[3100] __attribute__ ((aligned(4)));
static uint8_t *jpeg_data;
static uint16_t jpeg_pos;

// for JPEG decompression: 320*16*3 = 15360 bytes
// for complete thumbnail: 80*60*3 = 14400 bytes
static uint8_t image_buffer[IMG_WIDTH*IMG_HEIGHT*3];

// overlay text buffer: 4 * up to 39 chars + trailing zero
static char text_buffer[4][TEXT_LEN];

static uint8_t sstv_mode;

IMPORT_BIN("Inc/8x13B.fnt", uint8_t, Font8x13B);

static bool sstv_audio_callback(uint8_t *buffer, uint8_t line);


/* User defined call-back function to input JPEG data */
static UINT tjd_input(JDEC* jd, uint8_t* buff, UINT nd)
{
    if (buff) memcpy(buff, &jpeg_data[jpeg_pos], nd);
    jpeg_pos += nd;
    return nd;
}


/* User defined call-back function to output RGB bitmap */
static UINT tjd_thumbnail_output(JDEC* jd, void* bitmap, JRECT* rect)
{
    uint8_t *src, *dst;
    uint16_t y, bws, bwd;

    /* Copy the decompressed RGB rectangular to the frame buffer (assuming RGB888 cfg) */
    src = (uint8_t*)bitmap;
    dst = image_buffer + 3 * (rect->top * (IMG_WIDTH/4) + rect->left);  /* Left-top of destination rectangular */
    bws = 3 * (rect->right - rect->left + 1);     /* Width of source rectangular [byte] */
    bwd = 3 * (IMG_WIDTH/4);                      /* Width of frame buffer [byte] */
    for (y = rect->top; y <= rect->bottom; y++) {
        memcpy(dst, src, bws);   /* Copy a line */
        src += bws; dst += bwd;  /* Next line */
    }

    return 1;    /* Continue to decompress */
}


/* User defined call-back function to output RGB bitmap */
static UINT tjd_full_output(JDEC* jd, void* bitmap, JRECT* rect)
{
    uint8_t *src, *dst;
    uint16_t y, bws, bwd;

    /* Copy the decompressed RGB rectangular to the frame buffer (assuming RGB888 cfg) */
    src = (uint8_t*)bitmap;
    dst = image_buffer + 3 * ((rect->top % IMG_HEIGHT) * IMG_WIDTH + rect->left);  /* Left-top of destination rectangular */
    bws = 3 * (rect->right - rect->left + 1);     /* Width of source rectangular [byte] */
    bwd = 3 * IMG_WIDTH;                          /* Width of frame buffer [byte] */
    for (y = rect->top; y <= rect->bottom; y++) {
        memcpy(dst, src, bws);   /* Copy a line */
        src += bws; dst += bwd;  /* Next line */
    }

    /* execute callback when line block finished */
    if ((rect->bottom % IMG_HEIGHT) == (IMG_HEIGHT - 1) && rect->right == (IMG_WIDTH - 1)) {
        return sstv_audio_callback(image_buffer, (rect->bottom / IMG_HEIGHT) + 1) ? 1 : 0;
    }

    return 1;    /* Continue to decompress */
}


bool jpeg_thumbnail(uint8_t *jpeg, uint8_t **thumbnail)
{
    /* prepare variables */
    bool ok = true;
    JDEC jdec;
    jpeg_data = jpeg;
    jpeg_pos = 0;

    /* decompression */
    if (ok && jd_prepare(&jdec, tjd_input, workspace, sizeof(workspace), NULL) != JDR_OK) ok = false;
    if (ok && jdec.width != IMG_WIDTH) ok = false;
    if (ok && jd_decomp(&jdec, tjd_thumbnail_output, 2) != JDR_OK) ok = false;

    if (thumbnail != NULL)
        *thumbnail = image_buffer;

    if (!ok) syslog_event(LOG_JPEG_ERROR);

    return ok;
}


bool jpeg_decompress(uint8_t *jpeg)
{
    /* prepare variables */
    bool ok = true;
    JDEC jdec;
    jpeg_data = jpeg;
    jpeg_pos = 0;

    /* decompression */
    if (ok && jd_prepare(&jdec, tjd_input, workspace, sizeof(workspace), NULL) != JDR_OK) ok = false;
    if (ok && jdec.width != IMG_WIDTH) ok = false;
    if (ok && jd_decomp(&jdec, tjd_full_output, 0) != JDR_OK) ok = false;

    if (!ok) syslog_event(LOG_JPEG_ERROR);

    return true;
}


bool jpeg_test(uint8_t *jpeg, uint32_t length)
{
    return jpeg_thumbnail(jpeg, NULL);
}


static bool sstv_thumbnails(void)
{
    uint16_t ram_offset = 0;
    uint16_t flash_offset;

    for (uint8_t row = 0; row < 4; row++) {
        flash_offset = 0; // thumbnail has a zero offset
        for (uint8_t y = 0; y < 60; y++) {
            for (uint8_t col = 0; col < 4; col++) {
                flash_read(ADDR_THUMBNAIL(row*4 + col) + flash_offset, &image_buffer[ram_offset], 80*3);
                ram_offset += 80*3;
            }
            flash_offset += 80*3;

            uint16_t line = row*60+y;
            if (line % IMG_HEIGHT == IMG_HEIGHT-1) {
                /* decompression block buffer is full */
                if (!sstv_audio_callback(image_buffer, line / IMG_HEIGHT + 1)) return false;
                ram_offset = 0;
            }
        }
    }
    return true;
}


static void sstv_do_overlay(uint8_t *s, uint32_t color, uint8_t zoom, uint8_t part)
{
    uint16_t h = Font8x13B[15]; /* Font size: height */
    uint16_t w = Font8x13B[14]; /* Font size: width */
    uint16_t x = 3; /* X offset */

    while (x <= IMG_WIDTH - w*zoom) {
        uint8_t chr = *s++; /* Load character */

        if (chr >= 31 && chr <= 127) {
            const uint8_t *fnt = Font8x13B; /* Load font */
            fnt += 17 + chr * h; /* Goto start of the bitmap */

            for (uint16_t i = 0; i < IMG_HEIGHT; i++) { /* Go through Y axis */
                uint8_t chr_line = i/zoom + part*(IMG_HEIGHT/zoom);
                if (chr_line < h) { /* Is current line mapped in font face? */
                    uint8_t d = fnt[chr_line]; /* Get next 8 horizontal dots */
                    for (uint16_t j = 0; j < w*zoom; j++) { /* Go through X axis */
                        uint16_t idx = i*IMG_WIDTH + (x+j);
                        if (zoom == 1) idx += IMG_WIDTH; /* No zoom -> shift line by 1px down */
                        if (d & 0x80) { /* color */
                            image_buffer[idx*3+0] = (color >> 16) & 0xFF;
                            image_buffer[idx*3+1] = (color >> 8) & 0xFF;
                            image_buffer[idx*3+2] = (color >> 0) & 0xFF;
                        } else { /* lower brightness */
                            image_buffer[idx*3+0] >>= 1;
                            image_buffer[idx*3+1] >>= 1;
                            image_buffer[idx*3+2] >>= 1;
                        }
                        if (j % zoom == zoom - 1) d <<= 1; /* Next horizontal bit */
                    }
                }
            }
        }
        x += w*zoom; /* Next character */
    }
}


static bool sstv_audio_callback(uint8_t *buffer, uint8_t line)
{
    // modes with 15 text lines
    if (sstv_mode == 36 || sstv_mode == 72) {
        // overlay basic white chars, no zoom
        if (line == 1) sstv_do_overlay(text_buffer[OVERLAY_HEADER], 0xFFFFFF, 1, 0);
        if (line == 15) sstv_do_overlay(text_buffer[OVERLAY_IMG], 0xFFFFFF, 1, 0);

        // overlay up to 13 yellow chars on lines 3-5, zoom 3x
        if (line == 3 || line == 4 || line == 5) sstv_do_overlay(text_buffer[OVERLAY_LARGE], 0xFFFF00, 3, line-3);

        // overlay up to 19 red chars on lines 14-15, zoom 2x
        if (line == 14 || line == 15) sstv_do_overlay(text_buffer[OVERLAY_FROM], 0xFF4040, 2, line-14);
    }
    // modes with 16 text lines
    else if (sstv_mode == 73 || sstv_mode == 115) {
        // overlay basic white chars, no zoom
        if (line == 0) sstv_do_overlay(text_buffer[OVERLAY_HEADER], 0xFFFFFF, 1, 0);
        if (line == 15) sstv_do_overlay(text_buffer[OVERLAY_IMG], 0xFFFFFF, 1, 0);

        // overlay up to 13 yellow chars on lines 2-4, zoom 3x
        if (line == 2 || line == 3 || line == 4) sstv_do_overlay(text_buffer[OVERLAY_LARGE], 0xFFFF00, 3, line-2);

        // overlay up to 19 red chars on lines 14-15, zoom 2x
        if (line == 14 || line == 15) sstv_do_overlay(text_buffer[OVERLAY_FROM], 0xFF4040, 2, line-14);
    }

    // send audio block
    if (sstv_mode == 36) audio_robot36_color(buffer);
    else if (sstv_mode == 72) audio_robot72_color(buffer);
    else if (sstv_mode == 73) audio_mp73(buffer);
    else if (sstv_mode == 115) audio_mp115(buffer);

    return true; // continue
}


bool sstv_play_jpeg(uint8_t* jpeg, uint8_t mode)
{
    bool ok = true;
    
    /* check valid SSTV mode, default to Robot36 */
    if (mode == 72 || mode == 73 || mode == 115) sstv_mode = mode;
    else sstv_mode = 36;

    audio_start();
    audio_play_vox_start();
    if (sstv_mode == 36) audio_play_vis(0x88); // Robot36
    else if (sstv_mode == 72) audio_play_vis(0x0C); // Robot72
    else if (sstv_mode == 73) audio_play_vis16(0x2523); // MP73
    else if (sstv_mode == 115) audio_play_vis16(0x2923); // MP115

    if (sstv_mode == 73 || sstv_mode == 115) {
        // black header on line 0 for MP modes
        memset(image_buffer, 0, sizeof(image_buffer));
        ok = sstv_audio_callback(image_buffer, 0);
    }

    if (ok && jpeg != NULL) {
        ok = jpeg_decompress(jpeg);
    }
    else if (ok && jpeg == NULL) {
        ok = sstv_thumbnails();
    }
    audio_play_vox_stop();
    audio_stop();
    return ok;
}


bool sstv_play_thumbnail(uint8_t mode)
{
    return sstv_play_jpeg(NULL, mode);
}


void sstv_set_overlay(uint8_t line, const char *overlay)
{
    char s[TEXT_LEN];

    if (overlay == NULL) s[0] = '\0';
    else strncpy(s, overlay, TEXT_LEN);

    switch (line) {
        case OVERLAY_HEADER:
        case OVERLAY_IMG:
            if (strlen(overlay) > TEXT_Z1_WIDTH) s[TEXT_Z1_WIDTH] = '\0';
            memset(text_buffer[line], 0x00, TEXT_Z1_WIDTH);
            strcpy(text_buffer[line], s);
            break;
        case OVERLAY_LARGE:
            if (strlen(overlay) > TEXT_Z3_WIDTH) s[TEXT_Z3_WIDTH] = '\0';
            memset(text_buffer[line], 0x00, TEXT_Z3_WIDTH);
            strcpy(text_buffer[line], s);
            break;
        case OVERLAY_FROM:
            if (strlen(overlay) > TEXT_Z2_WIDTH) s[TEXT_Z2_WIDTH] = '\0';
            memset(text_buffer[line], 0xFF, TEXT_Z2_WIDTH); // blanks for right alignment
            strcpy(text_buffer[line] + (TEXT_Z2_WIDTH-strlen(s)), s);
            break;
    }
    printf_debug("Overlay %d = '%s'", line, s);
}

