/*
 * bpi-m3.h
 *
 *  Created on: Apr 22, 2017
 *      Author: Alex Konshin
 */

#ifndef ARCH_BPI_M3_H_
#define ARCH_BPI_M3_H_

/* BPi M3: See column CPU
gpio readall
 +-----+-----+---------+------+---+---BPi ---+---+------+---------+-----+-----+
 | CPU | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | CPU |
 +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
 |     |     |    3.3v |      |   |  1 || 2  |   |      | 5v      |     |     |
 | 229 |   8 |   SDA.1 | ALT5 | 0 |  3 || 4  |   |      | 5V      |     |     |
 | 228 |   9 |   SCL.1 | ALT5 | 0 |  5 || 6  |   |      | GND     |     |     |
 | 362 |   7 |    GCLK | ALT5 | 0 |  7 || 8  | 0 | ALT5 | TxD0    | 15  | 32  |
 |     |     |     GND |      |   |  9 || 10 | 0 | ALT5 | RxD0    | 16  | 33  |
 |  68 |   0 |    GEN0 | ALT3 | 0 | 11 || 12 | 0 | ALT5 | GEN1    | 1   | 35  |
 |  71 |   2 |    GEN2 | ALT3 | 0 | 13 || 14 |   |      | GND     |     |     |
 |  81 |   3 |    GEN3 | ALT3 | 0 | 15 || 16 | 0 | ALT5 | GEN4    | 4   | 34  |
 |     |     |    3.3v |      |   | 17 || 18 | 0 | ALT3 | GEN5    | 5   | 360 |
 |  64 |  12 |    MOSI | ALT4 | 0 | 19 || 20 |   |      | GND     |     |     |
 |  65 |  13 |    MISO | ALT4 | 0 | 21 || 22 | 0 | OUT  | GEN6    | 6   | 361 |
 |  66 |  14 |    SCLK | ALT4 | 0 | 23 || 24 | 0 | ALT4 | CE0     | 10  | 67  |
 |     |     |     GND |      |   | 25 || 26 | 0 | ALT3 | CE1     | 11  | 234 |
 | 227 |  30 |   SDA.0 | ALT5 | 0 | 27 || 28 | 0 | ALT5 | SCL.0   | 31  | 226 |
 |  82 |  21 | GPIO.21 | ALT3 | 0 | 29 || 30 |   |      | GND     |     |     |
 | 202 |  22 | GPIO.22 | ALT3 | 0 | 31 || 32 | 0 | ALT3 | GPIO.26 | 26  | 205 |
 | 203 |  23 | GPIO.23 | ALT3 | 0 | 33 || 34 |   |      | GND     |     |     |
 | 204 |  24 | GPIO.24 | ALT3 | 0 | 35 || 36 | 0 | ALT3 | GPIO.27 | 27  | 133 |
 | 132 |  25 | GPIO.25 | ALT3 | 0 | 37 || 38 | 0 | ALT3 | GPIO.28 | 28  | 146 |
 |     |     |     GND |      |   | 39 || 40 | 0 | ALT3 | GPIO.29 | 29  | 147 |
 +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
 | CPU | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | CPU |
 +-----+-----+---------+------+---+---BPi ---+---+------+---------+-----+-----+

*** WARNING ***
Not all GPIOs allow to set interruptions handler. This is a hardware limitation.
The following GPIO numbers are supported on BPI-M3:
  32, 33, 34, 35, 202, 203, 204, 205, 226, 227, 228, 229, 234, 360, 361, 362.
*/

#define MAX_GPIO 362
#define DEFAULT_PIN 35

#endif /* ARCH_BPI_M3_H_ */
