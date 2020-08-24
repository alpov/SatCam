# SSTV camera on PSAT3

SatCam is an [SSTV encoder with OV2640 camera](SatCam.md). It is based on STM32F446RET6 microcontroller. Supported SSTV modes are Robot36, Robot72, MP73 and MP115. SatCam was originally developed as a part of PSAT2 satellite project and later redesigned as a stand-alone module.

When enabled, camera **sends a live picture** in Robot36 or MP73 **every 10 minutes**. The SSTV transmission always come after APRS.

## Current status

* Current PSAT3 status: **waiting for deploy**

* Gallery of SSTV images - TBD. SatNOGS for PSAT3 - TBD. Please send your SSTV images and received runtime status (NVinfo) to alpov@alpov.net.

## SatCam commanding

SatCam can be commanded from the APRS link. It looks for `SATCAMERA:` keyword, followed by a command, case insensitive. Every HAM is allowed to use the following camera commands when the system is enabled.

| Command syntax                        | Parameters |
| ------------------------------------- | ---------- |
| `satcamera:sstv.live.MODE.OVERLAY`              | MODE is `36` for Robot36, `72` for Robot72, `73` for MP73, `115` for MP115 (default: 36), OVERLAY is user-defined text, `.` and `{` not allowed, up to 13 chars (default: none)
| `satcamera:psk.MESSAGE.SPEED.FREQ`              | MESSAGE is either user-defined text (`.` and `{` not allowed) or `nvinfo` or `config`, SPEED is `31`, `63`, `125`, `250`, `500`, `1000` (default: 31, for config/nvinfo: 125), FREQ is 100-7000Hz (default: 800)
| `satcamera:cw.MESSAGE.WPM.FREQ`                 | MESSAGE is either user-defined text (`.` and `{` not allowed), WPM is word per minute speed (5 to 40), FREQ is 100-7000Hz (default: 800)

| Command examples                      | Description |
| ------------------------------------- | ----------- |
| `satcamera:sstv.live`                 | Send live picture in Robot36
| `satcamera:sstv.live.73`              | Send live picture in MP73
| `satcamera:sstv.live.115.de OK2ALP`   | Send live picture in MP115, add overlay to image
| `satcamera:psk.hello from space`      | Send message `PSAT3 hello from space` in PSK-31 at 800Hz carrier
| `satcamera:psk.config`                | Send camera configuration in PSK-125 @ 800Hz
| `satcamera:psk.nvinfo`                | Send camera runtime status in PSK-125 @ 800Hz
| `satcamera:cw`                        | Send a welcome message `73 DE PSAT3 PSAT3 PSAT3 K` at 25WPM @ 800Hz
| `satcamera:cw.morse forever.18`       | Send message `PSAT3 morse forever` at 18WPM @ 800Hz

