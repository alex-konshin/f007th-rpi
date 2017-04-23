# f007th-rpi
## Raspberry Pi: Receiving data from temperature/humidity sensors Ambient Weather F007TH with cheap RF 433MHz receiver
###### Project source can be downloaded from [https://github.com/alex-konshin/f007th-rpi.git](https://github.com/alex-konshin/f007th-rpi.git)

### Author and Contributors
Alex Konshin <akonshin@gmail.com>

### Overview
This project currently contains source code of 2 executables for Raspberry Pi to receive data from temperature/humidity sensors "Ambient Weather F007TH" 
with cheap RF 433MHz receivers like [RXB6](http://www.jmrth.com/en/images/proimages/RXB6_en_v3.pdf), RX-RM-5V, etc.
- **f007th-rpi** is a demo/test program that receive data from sensors and prints it in plain text or JSON format.
- **f007th-rpi_send** is more advanced program that sends received and decoded data to a remote InfluxDB or REST server. 

### Files
| File | Description |
| :--- | :--- |
| `Bits.hpp` | Operations with long set of bits. It is used for storing decoded bits in Manchester decoder.|
| `F007TH.cpp` | Source code of **f007th-rpi** executable.|
| `f007th_send.cpp` | Source code of **f007th-rpi_send** executable. |
| `ReceivedMessage.hpp` | Container for received and decoded data. |
| `RFReceiver.cpp` | Implementation part of main class `RFReceiver` that actually receives and decodes data. |
| `RFReceiver.hpp` | Declaration part of class `RFReceiver`. |
| `SensorsData.hpp` | Contains the current list of sensors with the latest data. It is used for filtering duplicated data. |
| `makefile,*.mk` | Makefiles generated by Eclipse. They can be used for building executables outside Eclipse as well.
| `README.md` | This file. |

### How to build
There are several ways to do it. I actually cross-build this project in Eclipse on my Windows machine. 
You can look at [this good instruction about setting up Eclipse for cross-compilation](http://www.cososo.co.uk/2015/12/cross-development-using-eclipse-and-gcc-for-the-rpi/).  

##### Building on Raspberry Pi
- You need to install [libcurl](https://curl.haxx.se/libcurl/) and [pigpio](http://abyz.co.uk/rpi/pigpio/index.html) libraries.
```
sudo apt-get install libcurl4-openssl-dev
sudo apt-get install pigpio
```
- Clone sources from GitHub. The following command will create new sub-directory f007th-rpi in the current directory and download sources
```
git clone https://github.com/alex-konshin/f007th-rpi.git
```
- Build
```
/bin/sh f007th-rpi/build.sh
```
- Executables are created in directory `f007th-rpi/bin`. Note that you must run them with root privileges (for example with `sudo`). Use Ctrl-C to terminate the program.
 
### Running f007th-rpi_send
The command can send data to InfluxDB server or virtually any REST server that supports PUT requests.
How to setup these servers? It is out of the scope of this instruction because there are many possible solutions. For REST server I personally use [LoopBack](https://loopback.io/) with [PostgreSQL](https://www.postgresql.org/) that are run on QNAP NAS server.
 
The command sends JSON to REST server with following fields:  
`"time", "valid", "channel", "rolling_code", "temperature", "humidity","battery_ok"`.  
The value of field `temperature` is integer number of dF ("deciFahrenheit" = 1/10 of Fahrenheit). For example, if the value is 724 then the temperature is 72.4&deg;F.  

Instructions for InfluxDB can be found on site [https://docs.influxdata.com/influxdb/v1.2/introduction/installation/](https://docs.influxdata.com/influxdb/v1.2/introduction/installation/). The command sends 3 types of metrics: "temperature", "humidity" and "sensor_battery_status" with tags "type" (always "F007TH"), "channel" and "rolling_code". The value of "temperature" is in Fahrenheit.

#### Command line arguments of utility f007th-rpi_send:
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
##### --all, -A
Send all data. Only changed and valid data is sent by default.
##### --log-file, -l
Parameter is a path to log file.
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
`sudo f007th-rpi_send -t InfluxDB http://server.dom:8086/write?db=smarthome &`  
or  
`sudo f007th-rpi_send --type=InfluxDB -s http://server.dom:8086/write?db=smarthome &`  
or  
`sudo f007th-rpi_send --type=InfluxDB --send-to http://server.dom:8086/write?db=smarthome &`  
 
Send data to Loopback (REST server) on server `qnap.dom`:  
`sudo f007th-rpi_send http://qnap.dom:3000/api/roomtemps`  
or  
`sudo f007th-rpi_send -t REST -s http://qnap.dom:3000/api/roomtemps`  