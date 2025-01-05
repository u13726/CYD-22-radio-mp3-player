# CYD-22-radio-mp3-player

using thelimited features of a CYD 2.2 display to build another web radio.

uses:
- the internal DAC of the esp32
- the sd card for wifi  config, radio stations and mp3 files
- on board  speaker on gpio 26
- touch button conn3cted to gpio3
- display to  shoz time an station / mp3

time
- when online time synced with NTP
- offline time set manually

wifi credentials
- set in source
- or given in config

stations
- fixed in source
- specified un configg file ex Station=http://....../.....mp3

- 
