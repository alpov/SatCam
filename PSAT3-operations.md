# SatCam - SSTV camera on PSAT3

SatCam is an [SSTV encoder with OV2640 camera](SatCam.md). It is based on STM32F446RET6 microcontroller. Supported SSTV modes are Robot36, Robot72, MP73 and MP115. SatCam was originally developed as a part of PSAT2 satellite project and later redesigned as a stand-alone module.

SatCam can be commanded from e.g. an APRS link, the UART works at 9600 8N1. It looks for `SATCAMERA:` keyword, followed by a command, case insensitive. End of line is `<CR>`, `<LF>` or `{`. Empty lines and messages without proper keyword are ignored. All commands including the keyword are case insensitive. Example: `SatCamera:sstv.live.73<CR>`.

Another way of using SatCam is to setup its initial configuration with UART and then use its input signals only.

## Overview
* EN low or tristated - camera is completely off, PTT tristated, RXD not monitored
* EN goes high, ST is high - camera starts 36sec SSTV (M0 low) or 73sec SSTV (M1 high)
* after SSTV, camera ignores anything on ST for next 10 minutes
* after 10 minutes, camera starts to monitor ST again and waits for rising edge, which starts new SSTV
* RXD monitor - commands addressed to SATCAMERA are executed immediately

## SatCam commands

The module monitors RXD line when idle and looks for `SATCAMERA:` identifier. Expected commands are for SSTV, PSK and CW systems. Other commands (CAMCFG and DEBUG) are protected by a PIN. User PIN can be set to a custom number, master PIN is fixed for the particular module (computed from the MCU unique ID). It is possible to send protected commands for the next 15 minutes after the AUTH command with a correct PIN. Master PIN can be read using `psk.nvinfo` and `debug.status` commands in case when the user PIN is not set (i.e. is zero).

| Command syntax                        | Example       | Parameters |
| ------------------------------------- | ------------- | ---------- |
| `sstv.live.MODE.OVERLAY`              | `sstv.live.36` (send picture as Robot36) | MODE is `36` for Robot36, `72` for Robot72, `73` for MP73, `115` for MP115 (default: 36)
| `sstv.rom.MODE.PAGE_NUMBER.OVERLAY`   | `sstv.rom.115.3.hello` (send hardcoded image 3 as MP115, with overlay) | PAGE_NUMBER is ROM memory page (default: 0), OVERLAY is user-defined text, `.` and `{` not allowed, up to 13 chars (default: none)
| `psk.MESSAGE.SPEED.FREQ`              | `psk.hello.31.1000` (send message as PSK-31 at 1kHz) | MESSAGE is either user-defined text (`.` and `{` not allowed) or `nvinfo` or `config`
| `psk.config.SPEED.FREQ`               | `psk.config.125.1000` (send configuration as PSK-125 at 1kHz) | SPEED is `31`, `63`, `125`, `250`, `500`, `1000` (default: 31, for config/nvinfo: 125)
| `psk.nvinfo.SPEED.FREQ`               | `psk.nvinfo.125.1000` (send NVinfo log as PSK-125 at 1kHz) | FREQ is 100-7000Hz (default: 800)
| `cw.MESSAGE.WPM.FREQ`                 | `cw.hello.25.1000` (send message as 25WPM CW at 1kHz) | WPM is word per minute speed (5 to 40)

