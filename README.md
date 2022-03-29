# f007th-rpi
## Raspberry Pi: Receiving data from temperature/humidity sensors with cheap RF receiver
###### Project source can be downloaded from [https://github.com/alex-konshin/f007th-rpi.git](https://github.com/alex-konshin/f007th-rpi.git)

## [More information is available in the project Wiki](https://github.com/alex-konshin/f007th-rpi/wiki/) 

### Author and Contributors
Alex Konshin <akonshin@gmail.com>

### Overview
The main goal of this project is to intercept and decode radio signals from temperature/humidity sensors and show on console or send this received data to REST/[InfluxDB](https://www.influxdata.com/products/influxdb-overview/) servers and/or [MQTT](http://mqtt.org/) broker.
It is also possible to retrieve the current values via HTTP and even get some HTML pages. 
[Support of MQTT](https://github.com/alex-konshin/f007th-rpi/wiki/Support-of-MQTT) is still experimental. If you want to help to test this new feature then please contact the developer.

The utility can send data to InfluxDB server or virtually any REST server that supports PUT requests.\
For example, REST server can be [LoopBack](https://loopback.io/) with [PostgreSQL](https://www.postgresql.org/) for storing data.
But I prefer to send data to [InfluxDB](https://www.influxdata.com/products/influxdb/) server and visualize it with [Grafana](http://grafana.org/).  

You can view in any web browser the latest data received from sensors and graph of temperatures for the last 24 hours.\
This functionality does not require installing any servers because it uses built-in HTTP server.
![Example of web page](https://github.com/alex-konshin/f007th-rpi/wiki/images/rpi-www-screenshot.png)

You can assign action to some events. Actions may be changed accordingly to the specified schedule and can be enabled/disabled by other actions.

### Supported receivers
The data is received with cheap RF 433.92MHz receivers like [RXB6](https://cdn.instructables.com/ORIG/FM6/PYJR/JUMXMMGB/FM6PYJRJUMXMMGB.pdf), [SeeedStudio RF-R-ASK](https://www.seeedstudio.com/433MHz-ASK%26amp%3BOOK-Super-heterodyne-Receiver-module-p-2205.html), RX-RM-5V, etc.
It is tested with RXB6 and SeeedStudio RF-R-ASK.
* [Example of wiring RXB6 to Raspberry Pi](https://github.com/alex-konshin/f007th-rpi/wiki/Example-of-wiring-RXB6-to-Raspberry-Pi)

### Supported sensors
This project currently supports and tested with following sensors:    
- [Ambient Weather F007TH](http://www.ambientweather.com/amf007th.html)   
- [AcuRite 00592TXR/06002RM](https://www.acurite.com/kbase/592TXR.html)  
- [LaCrosse TX7U](https://www.lacrossetechnology.com/tx7u) (probably TX3/TX6 may also work)
- [LaCrosse TX141TH-BV3](https://www.lacrossetechnology.com/products/tx141th-bv3) (TX141TH-BCH is also tested)  
- [Auriol HG02832 (IAN 283582)](https://manuall.co.uk/auriol-ian-283582-weather-station/)    
- Fine Offset Electronics WH2 / Telldus FT007TH / Renkforce FT007TH and other clones
- TFA Twin Plus 30.3049 / Conrad KW9010 / Ea2 BL999
- 1-wire sensor (not RF) DS18B20

### [Supported platforms](https://github.com/alex-konshin/f007th-rpi/wiki/Home#supported-platforms)
Following platforms are supported and tested:
- Raspberry Pi 3, 4
- [ODROID C2](http://www.hardkernel.com/main/products/prdt_info.php?g_code=G145457216438&tab_idx=1)
- [MinnowBoard MAX/Turbot](https://www.minnowboard.org/) (tested with [MinnowBoard Turbot QUAD Core Board](https://store.netgate.com/Turbot4.aspx))
- [Banana Pi M3](https://bananapi.gitbooks.io/bpi-m3/content/en/) (limited support)

### [Getting started](https://github.com/alex-konshin/f007th-rpi/wiki/Getting-Started)
First start with instruction [Getting started](https://github.com/alex-konshin/f007th-rpi/wiki/Getting-Started) on Wiki.
When you get your setup working you can change configuration to enable other features.  

### [How to build](https://github.com/alex-konshin/f007th-rpi/wiki/How-to-build)
* [Building on Raspberry Pi](https://github.com/alex-konshin/f007th-rpi/wiki/Building-on-Raspberry-Pi)
* [Building on MinnowBoard](https://github.com/alex-konshin/f007th-rpi/wiki/Building-on-MinnowBoard)

### [Running the utility](https://github.com/alex-konshin/f007th-rpi/wiki/Running-the-utility)
* [Command line arguments](https://github.com/alex-konshin/f007th-rpi/wiki/Command-line-arguments)
* [Configuration file](https://github.com/alex-konshin/f007th-rpi/wiki/Configuration-file)

