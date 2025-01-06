# CYD-22-radio-mp3-player

using the limited features of a CYD 2.2 display to build another web radio.

uses:
- the internal DAC of the esp32
- the sd card for wifi  config, radio stations and mp3 files
- on board  speaker on gpio 26
- touch button connected to gpio3
- display to  show time and station / mp3

time
- when online time synced with NTP
- offline time set manually

wifi credentials
- set in source
- or given in config

stations
- fixed in source
- specified in config file ex Station=http://....../.....mp3

uses libraries 
- esp32IZS audios
- bb_spi_lcd
- fileconfig

use of gpio3 as input  requires to be disconnected when uploading a new version
- 
