# f007th-rpi
## Raspberry Pi: Receiving data from temperature/humidity sensors with cheap RF receiver
###### Project source can be downloaded from [https://github.com/alex-konshin/f007th-rpi.git](https://github.com/alex-konshin/f007th-rpi.git)

### Author and Contributors
Alex Konshin <akonshin@gmail.com>

### Overview
The main goal of this project is to intercept and decode radio signals from temperature/humidity sensors and show on console or send this received data to REST/[InfluxDB](https://www.influxdata.com/products/influxdb-overview/) servers and/or [MQTT](http://mqtt.org/) broker.
It is also possible to retrieve the current values via HTTP. 
Support for MQTT is currently experimental and instructions are under development. If you want to help to test this new feature then please contact the developer.

The data is received with cheap RF 433.92MHz receivers like [RXB6](http://www.jmrth.com/en/images/proimages/RXB6_en_v3.pdf), [SeeedStudio RF-R-ASK](https://www.seeedstudio.com/433MHz-ASK%26amp%3BOOK-Super-heterodyne-Receiver-module-p-2205.html), RX-RM-5V, etc. It is tested with RXB6 and SeeedStudio RF-R-ASK.

This project currently supports and tested with following sensors:    
- [Ambient Weather F007TH](http://www.ambientweather.com/amf007th.html)   
- [AcuRite 00592TXR/06002RM](https://www.acurite.com/kbase/592TXR.html)  
- [LaCrosse TX7U](https://www.lacrossetechnology.com/tx7u) (probably TX3/TX6 may also work)  
- [Auriol HG02832 (IAN 283582)](https://manuall.co.uk/auriol-ian-283582-weather-station/)    
- Fine Offset Electronics WH2 / Telldus FT007TH / Renkforce FT007TH and other clones    

### Supported platforms
Following platforms are supported and tested:
- Raspberry Pi 3, 4
- [Banana Pi M3](https://bananapi.gitbooks.io/bpi-m3/content/en/)
- [ODROID C2](http://www.hardkernel.com/main/products/prdt_info.php?g_code=G145457216438&tab_idx=1)
- [MinnowBoard MAX/Turbot](https://www.minnowboard.org/) (tested with [MinnowBoard Turbot QUAD Core Board](https://store.netgate.com/Turbot4.aspx))

##### Raspberry Pi
There are 2 executables on this platform:
- **f007th-rpi_send** sends received and decoded data to a remote InfluxDB or REST server. This executable requires pigpio library to be installed and should be run as root (via sudo).
- **f007th-send** is the same as above but uses [gpio-ts driver](https://github.com/alex-konshin/gpio-ts). This executable does not require root privileges but [gpio-ts module](https://github.com/alex-konshin/gpio-ts) must be already loaded.

##### Banana Pi M3, ODROID C2, MinnowBoard.
On these platforms only **f007th-send** is supported and tested. This utility sends received and decoded data to a remote InfluxDB or REST server. It does not require root privileges but [gpio-ts module](https://github.com/alex-konshin/gpio-ts) must be already loaded.


### Files
| File | Description |
| :--- | :--- |
| `mach/rpi3.h` | Contains macros that are specific for platform Raspberry Pi 3.|
| `mach/bpi-m3.h` | Contains macros that are specific for platform Banana Pi M3.|
| `mach/odroid-c2.h` | Contains macros that are specific for platform ODROID C2.|
| `mach/x86_64.h` | Contains macros that are specific for platform x86_64 (ex: MinnowBoard).|
| `Bits.hpp` | Operations with long set of bits. It is used for storing decoded bits in Manchester decoder.|
| `F007TH.cpp` | Simple example of code that receives and prints messages from sensors.|
| `f007th_send.cpp` | Source code of **f007th-rpi_send** executable. |
| `ReceivedMessage.hpp` | Container for received and decoded data. |
| `RFReceiver.cpp` | Implementation part of main class `RFReceiver` that actually receives and decodes data. |
| `RFReceiver.hpp` | Declaration part of class `RFReceiver`. |
| `SensorsData.hpp` | Contains the current list of sensors with the latest data. It is used for filtering duplicated data. |
| `Logger.hpp` | Implements functions for logging messages. |
| `makefile,*.mk` | Makefiles generated by Eclipse. They can be used for building executables outside Eclipse as well.
| `README.md` | This file. |

### How to build
There are several ways to do it. I actually cross-build this project in Eclipse on my Windows machine. 
You can look at [this good instruction about setting up Eclipse for cross-compilation](http://www.cososo.co.uk/2015/12/cross-development-using-eclipse-and-gcc-for-the-rpi/).  

##### Building on Raspberry Pi
- You need to install [libcurl](https://curl.haxx.se/libcurl/) and [microhttpd](https://www.gnu.org/software/libmicrohttpd/) libraries.
```
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev libmicrohttpd-dev
```
- If you want to use [pigpio library](http://abyz.co.uk/rpi/pigpio/index.html) then you need to install it.
Note: It is recommended do not use pigpio but use [gpio-ts driver](https://github.com/alex-konshin/gpio-ts). The driver works better and creates less load to Raspberry Pi CPU.   
```
sudo apt-get install pigpio
```
- If you want to use [MQTT](http://mqtt.org/) (this is not usual) then you need to install [Mosquitto](https://github.com/eclipse/mosquitto) libraries:
```
sudo apt-get install libmosquitto-dev libmosquittopp-dev libssl-dev
```  
- Clone sources from GitHub. The following command will create new sub-directory f007th-rpi in the current directory and download sources
```
git clone https://github.com/alex-konshin/f007th-rpi.git
```
- Build
```
/bin/sh f007th-rpi/build.sh
```
- Executables are created in directory `f007th-rpi/bin`. Note that some of them must be run with root privileges (for example with `sudo`). Use Ctrl-C or command kill to terminate utilities.

##### Building on MinnowBoard
- Build [gpio-ts module](https://github.com/alex-konshin/gpio-ts) first.
- Install [libcurl](https://curl.haxx.se/libcurl/) and [microhttpd](https://www.gnu.org/software/libmicrohttpd/) libraries.
```
sudo apt-get install libcurl4-openssl-dev libmicrohttpd-dev
```
- Clone sources from GitHub. The following command will create new sub-directory f007th-rpi in the current directory and download sources
```

git clone https://github.com/alex-konshin/f007th-rpi.git
```
- Build
```
/bin/bash f007th-rpi/f007th-ts-x86_64/build.sh
```
- Executable file `f007th-send` is created in directory `~/bin`.

### Running f007th-rpi_send or f007th-send
The command can send data to InfluxDB server or virtually any REST server that supports PUT requests.
How to setup these servers? It is out of the scope of this instruction because there are many possible solutions. For REST server I personally use [LoopBack](https://loopback.io/) with [PostgreSQL](https://www.postgresql.org/) that are run on QNAP NAS server.
 
The command sends JSON to REST server with following fields:  
`"time", "valid", "type", "channel", "rolling_code", "temperature", "humidity","battery_ok"`.  
The value of field `temperature` is integer number of dF ("deciFahrenheit" = 1/10 of Fahrenheit). For example, if the value is 724 then the temperature is 72.4&deg;F. Note that not all fields are always present in each report. 

Instructions for InfluxDB can be found on site [https://docs.influxdata.com/influxdb/v1.2/introduction/installation/](https://docs.influxdata.com/influxdb/v1.2/introduction/installation/). The command sends 3 types of metrics: "temperature", "humidity" and "sensor_battery_status" with tags "type" (one of "F007TH", "00592TXR", "TX7", "HG02832", "WH2", "FT007TH"), "channel" and "rolling_code". The value of "temperature" is in Fahrenheit. Note that rolling code is changed when you replace batteries.

#### Command line arguments of utility f007th-rpi_send:
##### --config, -c
Argument of this option specifies configuration file. 
##### --gpio, -g
Value is GPIO pin number (default is 27) as defined on page [http://abyz.co.uk/rpi/pigpio/index.html](http://abyz.co.uk/rpi/pigpio/index.html)
##### --celsius, -C
Output temperature in degrees Celsius.
##### --utc, -U
Timestamps are printed/sent in ISO 8601 format.
##### --local-time, -L
Timestamps are printed/sent in format "YYYY-mm-dd HH:MM:SS TZ".
##### --send-to, -s
Parameter value is server URL.
##### --server-type, -t
Parameter value is server type. Possible values are REST (default) or InfluxDB.
##### --stdout, -o
Print data to stdout. This option is not compatible with --server-type and --no-server.
##### --no-server, -n
Do not print data on console or send it to servers. This option is not compatible with --server-type and --stdout.
##### --all-changes, -A
Send all data. Only changed and valid data is sent by default.
##### --max-gap, -G
Argument of this option specifies the max time in minutes when the utility does not send data if it is not changed.
If option --all-changes|-A is not specified then by default the utility does not send repeating values to the server. You can specify this option to prevent big gaps in server's data when values are changed very slowly.    
##### --log-file, -l
Parameter is a path to log file.
##### --httpd, -H
Run HTTPD server on the specified port. If this option is specified then the utility will serve HTTP requests on this port. Currently it just sends JSON with current values from all sensors.    
##### --verbose, -v
Verbose output.
##### --more_verbose, -V
More verbose output.

#### Examples:
Print received data on console:  
`sudo f007th-rpi_send -A`  
 
Print received data on console with temperature in degrees Celsius:  
`sudo f007th-rpi_send -C -A`  
 
Run the command in background and send data to InfluxDB database `smarthome` on server `server.dom`:  
`f007th-send -t InfluxDB http://server.dom:8086/write?db=smarthome &`  
or  
`f007th-send --type=InfluxDB -s http://server.dom:8086/write?db=smarthome &`  
or  
`f007th-send --type=InfluxDB --send-to http://server.dom:8086/write?db=smarthome &`  
 
Send data to Loopback (REST server) on server `qnap.dom`:  
`f007th-send http://qnap.dom:3000/api/roomtemps`  
or  
`f007th-send -t REST -s http://qnap.dom:3000/api/roomtemps`  

### Configuration file
You can specify configuration file with command line option -c|--config. Each line of configuration file may contain a single command line option (long form only!) or command. Symbol '#' starts a comment at any line of configuration file. 

#### Configuration file commands:
##### sensor
```
sensor <type> [<channel>] <rolling_code> <name>
```
Defines a sensor and gives a name to it. This name is printed or sent to server with sensor's data. An argument `<name>` can be any word without blanks or quoted string.
Argument `<type>` could be one of (case insensitive): `f007th`, `f007tp`, `00592txr`, `hg02832`, `tx6`, `wh2`, `ft007th`.
Argument `<rolling_code>` may be decimal number or hex number started with `0x`. 
Warning: Argument `<channel>` is not optional: You must specify it if this particular sensor type has channels. For sensors of type 00592txr the value of channel is A, B or C. For sensors f007th and f007tp it should be a number 1..8. For sensor hg02832 it should be number 1..3.

#### Example of configuration file without MQTT:
```
# Any comments are started with hash. Empty lines are ignored.

gpio 27
#all-changes
#verbose
httpd 8888

server-type InfluxDB
send-to http://m700.dom:8086/write?db=smarthome
max-gap 5

log-file "f007th-send.log"

sensor f007th   1  13 "Server room"
sensor f007th   2  19 "Main bedroom"
sensor f007th   4 120 "Alex office"
sensor f007th   5 174 "Kitchen"
sensor f007th   6  48 "TV room"
sensor f007th   7  88 "Garage"

sensor f007th   1  191 "Neighbor 2"
sensor 00592txr A 0x5c "Neighbor 1"
sensor 00592txr A  146 "Backyard"

sensor tx6  104 "LaCrosse TX7U 1" # note that channel is missing
sensor tx6   92 "LaCrosse TX7U 2"
``` 

#### Example of configuration file with MQTT:
```
# Any comments are started with hash. Empty lines are ignored.

gpio 27
#all-changes
#verbose
httpd 8888

server-type InfluxDB
send-to http://m700.dom:8086/write?db=smarthome
max-gap 5

log-file "f007th-send.log"

sensor f007th   1  13 "Server room"
sensor f007th   2  19 "Main bedroom"
sensor f007th   3  85 "Roman room"
sensor f007th   4 120 "Alex office"
sensor f007th   5 174 "Kitchen"
sensor f007th   6  48 "TV room"
sensor f007th   7  88 "Garage"
sensor f007th   8  83 "Dirty room"

sensor f007th   1 191 "Neighbor 2"
sensor 00592txr A  92 "Neighbor 1"
sensor 00592txr A 146 "Backyard"

sensor tx6        104 "LaCrosse TX7U 1"
sensor tx6         92 "LaCrosse TX7U 2"

# MQTT broker connection information
mqtt_broker host=m700.dom port=1883 client_id=RPi4 user=pi password=censored

# A rule that always sends temperature in Fahrenheit to broker
mqtt_rule id=alex_office      sensor="Alex office" metric=F topic=sensors/temperature/alex_office msg=%F

# Rules that potentially may controls HVAC unit
mqtt_bounds_rule id=cool sensor="Alex office" metric=F topic=hvac/cooling msg_hi=on msg_lo=off bounds=72..77[24:00]70..77[8:00]
mqtt_bounds_rule id=heat sensor="Kitchen" metric=F topic=hvac/heating msg_hi=off msg_lo=on bounds=72..77[24:00]70..77[8:00]
``` 

#### Example of wiring RXB6 to Raspberry Pi 3 ####

Raspberry Pi is on the left:  
3.3v (pin 1) <==> +5v/VDD (pin 5)  
GPIO27 (pin 13) <==> DATA (pin 7)  
Ground (pin 25) <==> GND (pin 8)  
![Picture of wiring RXB6 to Raspberry Pi 3](https://github.com/alex-konshin/f007th-rpi/blob/master/RXB6_wiring.JPG)
