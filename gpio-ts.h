/*
 * gpio-ts.h
 *
 *  Created on: Mar 5, 2017
 *      Author: Alex Konshin
 */

#ifndef GPIO_TS_H_
#define GPIO_TS_H_

#define DEV_NAME      "gpio-ts"
#define GPIO_TS_CLASS_NAME             "gpio-ts"
#define GPIO_TS_ENTRIES_NAME           "gpiots%d"
#define MAX_GPIO_ENTRIES     17

#ifdef __KERNEL__
extern int gpios[MAX_GPIO_ENTRIES];
extern int n_of_gpios;
#endif

/* RPi 3
gpio readall
 +-----+-----+---------+------+---+---Pi 3---+---+------+---------+-----+-----+
 | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |
 +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
 |     |     |    3.3v |      |   |  1 || 2  |   |      | 5v      |     |     |
 |   2 |   8 |   SDA.1 | ALT0 | 1 |  3 || 4  |   |      | 5v      |     |     |
 |   3 |   9 |   SCL.1 | ALT0 | 1 |  5 || 6  |   |      | 0v      |     |     |
 |   4 |   7 | GPIO. 7 |   IN | 0 |  7 || 8  | 0 | IN   | TxD     | 15  | 14  |
 |     |     |      0v |      |   |  9 || 10 | 1 | IN   | RxD     | 16  | 15  |
 |  17 |   0 | GPIO. 0 |   IN | 0 | 11 || 12 | 0 | IN   | GPIO. 1 | 1   | 18  |
 |  27 |   2 | GPIO. 2 |   IN | 1 | 13 || 14 |   |      | 0v      |     |     |
 |  22 |   3 | GPIO. 3 |   IN | 0 | 15 || 16 | 0 | IN   | GPIO. 4 | 4   | 23  |
 |     |     |    3.3v |      |   | 17 || 18 | 0 | IN   | GPIO. 5 | 5   | 24  |
 |  10 |  12 |    MOSI | ALT0 | 0 | 19 || 20 |   |      | 0v      |     |     |
 |   9 |  13 |    MISO | ALT0 | 0 | 21 || 22 | 0 | IN   | GPIO. 6 | 6   | 25  |
 |  11 |  14 |    SCLK | ALT0 | 0 | 23 || 24 | 1 | OUT  | CE0     | 10  | 8   |
 |     |     |      0v |      |   | 25 || 26 | 1 | OUT  | CE1     | 11  | 7   |
 |   0 |  30 |   SDA.0 |   IN | 1 | 27 || 28 | 1 | IN   | SCL.0   | 31  | 1   |
 |   5 |  21 | GPIO.21 |   IN | 1 | 29 || 30 |   |      | 0v      |     |     |
 |   6 |  22 | GPIO.22 |   IN | 1 | 31 || 32 | 0 | IN   | GPIO.26 | 26  | 12  |
 |  13 |  23 | GPIO.23 |   IN | 0 | 33 || 34 |   |      | 0v      |     |     |
 |  19 |  24 | GPIO.24 |   IN | 0 | 35 || 36 | 0 | IN   | GPIO.27 | 27  | 16  |
 |  26 |  25 | GPIO.25 |   IN | 0 | 37 || 38 | 0 | IN   | GPIO.28 | 28  | 20  |
 |     |     |      0v |      |   | 39 || 40 | 0 | IN   | GPIO.29 | 29  | 21  |
 +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
 | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |
 +-----+-----+---------+------+---+---Pi 3---+---+------+---------+-----+-----+
*/

/* BPi M3
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
*/

#define STATUS_LEVEL_0     0
#define STATUS_LEVEL_1     1
#define STATUS_NOISE       2
#define STATUS_LOST_DATA   3

#define GPIO_TS_MAX_DURATION ((1L<<30)-1)
#define GPIO_TS_MAX_DURATION_NS (1000LL*GPIO_TS_MAX_DURATION)

#define make_item(status, duration) (((uint32_t)status<<30) | duration)
#define item_to_duration(item) ((item) & GPIO_TS_MAX_DURATION)
#define item_to_status(item) (((item)>>30)&3L)

#define DEFAULT_MIN_DURATION 100
#define DEFAULT_MAX_DURATION 5000
#define DEFAULT_MIN_SEQ_LEN 30

// ------------------ IOCTL commands ------------------------------

#ifndef __KERNEL__
#include <sys/ioctl.h>
#endif

typedef struct gpio_ts_statistics {
  unsigned isr_counter;
  unsigned irq_data_overflow_counter;
  unsigned buffer_overflow_counter;
} gpio_ts_statistics_t;


#define GPIOTS_IOCTL_MAGIC_NUMBER 'G'

#define GPIOTS_IOCTL_START            _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x1)
#define GPIOTS_IOCTL_SUSPEND          _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x2)
#define GPIOTS_IOCTL_SET_MIN_DURATION _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x3)
#define GPIOTS_IOCTL_SET_MAX_DURATION _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x4)
#define GPIOTS_IOCTL_SET_MIN_SEQ_LEN  _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x5)
//#define GPIOTS_IOCTL_GET_STAT         _IOR(GPIOTS_IOCTL_MAGIC_NUMBER, 0x6, gpio_ts_statistics_t) TODO How???

#ifdef __KERNEL__
// ------------------ Driver private data ------------------------------

#ifdef KERNEL_SIMULATION
#define N_BUFFER_ITEMS 16
#else
#define N_BUFFER_ITEMS 2048
#endif
#define MAX_SEQ_LEN_IN_BUFFER (N_BUFFER_ITEMS-4)

#define BUFFER_SIZE    (N_BUFFER_ITEMS*sizeof(uint32_t))
#define BUFFER_INDEX_MASK (N_BUFFER_ITEMS-1)

struct gpio_ts_data {
    volatile s64 last_timestamp;
    volatile s64 last_recorded_timestamp;
    volatile s64 current_seq_timestamp;

    spinlock_t spinlock;
    wait_queue_head_t read_queue;
    uint32_t min_duration;
    uint32_t max_duration;
    unsigned gpio;
    unsigned gpio_index;
    volatile unsigned read_index;
    volatile unsigned write_index;
    volatile unsigned current_seq_index;
    volatile unsigned buffer_seq_len;
    volatile unsigned current_seq_len;
    unsigned min_seq_len;
    volatile unsigned last_status;
    volatile unsigned delayed_status;
    volatile bool enabled;
    volatile bool signal;
    uint32_t buffer[N_BUFFER_ITEMS];
};

typedef struct gpio_ts_irq_data {
    volatile ktime_t kt;
    volatile struct file* pfile;
    volatile unsigned gpio_level;
} gpio_ts_irq_data_t;

#define N_IRQ_DATA_SLOTS (1<<5) // = 32

typedef struct gpio_ts_tasklet_data {
    struct tasklet_struct tasklet;
    gpio_ts_irq_data_t irq_data[N_IRQ_DATA_SLOTS];
    volatile unsigned write_index;
    volatile unsigned read_index;
} gpio_ts_tasklet_data_t;

#endif // __KERNEL__

#endif /* GPIO_TS_H_ */
