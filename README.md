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

The utility can send data to InfluxDB server or virtually any REST server that supports PUT requests.
How to setup these servers? It is out of the scope of this instruction because there are many possible solutions.
For REST server I personally used [LoopBack](https://loopback.io/) with [PostgreSQL](https://www.postgresql.org/) that are run on QNAP NAS server. But now I prefer to send data to InfluxDB server.  
The utility sends JSON to REST server with following fields:  
`"time", "valid", "type", "channel", "rolling_code", "temperature", "humidity","battery_ok"`.  
The value of field `temperature` is integer number of dF ("deciFahrenheit" = 1/10 of Fahrenheit). For example, if the value is 724 then the temperature is 72.4&deg;F. Note that not all fields are always present in each report.  

Instructions for InfluxDB can be found on site [https://www.influxdata.com/products/influxdb/](https://www.influxdata.com/products/influxdb/).
The command sends 3 types of metrics: "temperature", "humidity" and "sensor_battery_status" with tags "type" (one of "F007TH", "00592TXR", "TX7", "HG02832", "WH2", "FT007TH"), "channel" and "rolling_code".
Note that rolling code is changed when you replace batteries.

You can assign action to some events. Actions may be changed accordingly to specified schedule.

### Supported receivers
The data is received with cheap RF 433.92MHz receivers like [RXB6](http://www.jmrth.com/en/images/proimages/RXB6_en_v3.pdf), [SeeedStudio RF-R-ASK](https://www.seeedstudio.com/433MHz-ASK%26amp%3BOOK-Super-heterodyne-Receiver-module-p-2205.html), RX-RM-5V, etc.
It is tested with RXB6 and SeeedStudio RF-R-ASK.
* [Example of wiring RXB6 to Raspberry Pi](https://github.com/alex-konshin/f007th-rpi/wiki/Example-of-wiring-RXB6-to-Raspberry-Pi)

### Supported sensors
This project currently supports and tested with following sensors:    
- [Ambient Weather F007TH](http://www.ambientweather.com/amf007th.html)   
- [AcuRite 00592TXR/06002RM](https://www.acurite.com/kbase/592TXR.html)  
- [LaCrosse TX7U](https://www.lacrossetechnology.com/tx7u) (probably TX3/TX6 may also work)  
- [Auriol HG02832 (IAN 283582)](https://manuall.co.uk/auriol-ian-283582-weather-station/)    
- Fine Offset Electronics WH2 / Telldus FT007TH / Renkforce FT007TH and other clones    

### [Supported platforms](https://github.com/alex-konshin/f007th-rpi/wiki/Home#supported-platforms)
Following platforms are supported and tested:
- Raspberry Pi 3, 4
- [Banana Pi M3](https://bananapi.gitbooks.io/bpi-m3/content/en/)
- [ODROID C2](http://www.hardkernel.com/main/products/prdt_info.php?g_code=G145457216438&tab_idx=1)
- [MinnowBoard MAX/Turbot](https://www.minnowboard.org/) (tested with [MinnowBoard Turbot QUAD Core Board](https://store.netgate.com/Turbot4.aspx))

### [How to build](https://github.com/alex-konshin/f007th-rpi/wiki/How-to-build)
* [Building on Raspberry Pi](https://github.com/alex-konshin/f007th-rpi/wiki/Building-on-Raspberry-Pi)
* [Building on MinnowBoard](https://github.com/alex-konshin/f007th-rpi/wiki/Building-on-MinnowBoard)

### [Running the utility](https://github.com/alex-konshin/f007th-rpi/wiki/Running-the-utility)
* [Command line arguments](https://github.com/alex-konshin/f007th-rpi/wiki/Command-line-arguments)
* [Configuration file](https://github.com/alex-konshin/f007th-rpi/wiki/Configuration-file)

