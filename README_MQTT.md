# f007th. MQTT configuration
This article is about configuring parameters and rules of f007th utility related to interaction with MQTT broker.

For information about the utility see page [https://github.com/alex-konshin/f007th-rpi](https://github.com/alex-konshin/f007th-rpi)

## Overview
Support for MQTT is currently experimental and this instruction is still under development. If you want to help to test this new feature then please contact the developer.

The implementation of support of MQTT is based on [Mosquitto](https://github.com/eclipse/mosquitto) library. The exact supported versions of MQTT protocols are depends on that support in that library.

## How to build
There are several ways to do it. I actually cross-build this project in Eclipse on my Windows machine.
You can look at [this good instruction about setting up Eclipse for cross-compilation](http://www.cososo.co.uk/2015/12/cross-development-using-eclipse-and-gcc-for-the-rpi/).

### Building the utility
- Build [gpio-ts driver](https://github.com/alex-konshin/gpio-ts) first.
Theoretically it is possible to build the utility for Raspberry Pi with pigpio library but this configuration (with MQTT support) has never been tested and currently there is no plans to support this configuration. For other platforms it will not work anyway.
- Install [libcurl](https://curl.haxx.se/libcurl/) and [microhttpd](https://www.gnu.org/software/libmicrohttpd/) libraries.
Actually it is not absolutely necessary for the utility but this is how it is configured in build scripts.
```
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev libmicrohttpd-dev
```
- Install [Mosquitto](https://github.com/eclipse/mosquitto) libraries:
```
sudo apt-get install libmosquitto-dev libmosquittopp-dev libssl-dev
```
- Clone sources from GitHub. The following command will create new sub-directory f007th-rpi in the current directory and download sources
```
git clone https://github.com/alex-konshin/f007th-rpi.git
```
- Build
```
/bin/sh f007th-rpi/build_mqtt.sh
```
- Executables are created in directory `f007th-rpi/bin`. Use Ctrl-C or command `kill` to terminate the utility.

### Utility Configuration
The utility supports all options that are defined on page [https://github.com/alex-konshin/f007th-rpi](https://github.com/alex-konshin/f007th-rpi).

All settings and rules that are related to MQTT can be set in configuration file only. They cannot be specified on command line.

You have to specify configuration file with command line option -c|--config.\
Each line of configuration file may contain a single command line option (long form only!) or some other command.\
Symbol '#' starts a comment at any line of configuration file.\
String arguments with blanks and/or other special characters must be quoted.

To use the utility with MQTT you must:
- Define sensors with commands `sensor`.
- Add command `mqtt_broker`.
- Add one or more commands `mqtt_rule` and/or `mqtt_bounds_rule`.

These commands are described below. For other commands see page [https://github.com/alex-konshin/f007th-rpi](https://github.com/alex-konshin/f007th-rpi).

If you do not specify MQTT-related settings/rules in configuration files then the utility will not initialize MQTT and it will work the same way as the utility built without MQTT support.

#### Command `sensor`
```
sensor <type> [<channel>] <rolling_code> <name>
```
Defines a sensor and gives a name to it. This name is printed or sent to server with sensor's data. An argument `<name>` can be any word without blanks or quoted string.\
Argument `<type>` could be one of (case insensitive): `f007th`, `f007tp`, `00592txr`, `hg02832`, `tx6`, `wh2`, `ft007th`.\
Argument `<rolling_code>` may be decimal number or hex number started with `0x`.\
Warning: Argument `<channel>` is not optional: You must specify it if this particular sensor type has channels:\
- For sensors of type 00592txr the value of channel is A, B or C.
- For sensors f007th and f007tp it should be a number 1..8.
- For sensors hg02832 it should be number 1..3.

#### Command `mqtt_broker`
```
mqtt_broker [host=<host>] [port=<port>] [client_id=<client_id>] [user=<user> password=<password>] [keepalive=<keepalive>]
```
The command defines the MQTT broker. The utility will send messages to this broker only. There is nor way to change the broker dynamically but probably you don't need it anyway.\
Arguments `host` and `port` specifies MQTT broker host and port. The default value of `host` is `localhost`. The dafault value of `port` is 1883.\
Argument `client_id` should specify any valid unique identifier that will distinguish this MQTT client from any other clients registered on the broker.\
Arguments `user` and `password` specify credentials for connection to MQTT broker. Sorry, currently there is no way to make it more secure.\
Argument `keepalive` specifies the number of seconds after which the broker should send a PING message to the client if no other messages have been exchanged in that time. The default value is 60.\
Arguments "protocol", "certificate", "tls_insecure", "tls_version" are not implemented yet but they are planned.

#### Command `mqtt_rule`


TODO


#### Command `mqtt_bounds_rule`


TODO


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

