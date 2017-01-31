# PSAT-2 PSK/SSTV transponder
This repository hosts public parts of two boards from [PSAT-2](http://www.aprs.org/psat-2.html), a 2U CubeSat by US Naval Academy. These boards were developed at Dept. of Radio Electronics, Brno University of Technology, Czech Republic.

### PSK transponder
First board is an updated version of the PSK transponder with beacon and telemetry, used on [PSAT](http://www.aprs.org/psat.html) and [BRICSAT-1](http://www.aprs.org/bricsat-1.html). Transponder documentation available for [PSAT](http://www.urel.feec.vutbr.cz/esl/files/Projects/PSAT/P%20sat%20transponder%20WEB%20spec02.htm) and [BRICSAT-1](http://www.urel.feec.vutbr.cz/esl/files/Projects/BRICsat/Bricsat%20transponder%20WEB%20spec02.htm).

### SSTV transmitter
Second board provides a Slow-scan Television (SSTV) signal generator with PSK/CW telemetry, APRS uplink, and camera module. It is based on STM32F446RET6 microcontroller. Supported SSTV modes are Robot36, Robot72, MP73 and MP115.

The source files do not include authorization codes for SSTV and uplink commanding for PSK transponder. More documentation and APRS uplink description will be added in next months.
