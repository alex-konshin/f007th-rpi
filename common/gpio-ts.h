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


#define GPIOTS_IOCTL_MAGIC_NUMBER 'G'

#define GPIOTS_IOCTL_START                _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x1)
#define GPIOTS_IOCTL_SUSPEND              _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x2)
#define GPIOTS_IOCTL_SET_MIN_DURATION     _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x3)
#define GPIOTS_IOCTL_SET_MAX_DURATION     _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x4)
#define GPIOTS_IOCTL_SET_MIN_SEQ_LEN      _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x5)
#define GPIOTS_IOCTL_GET_IRQ_OVERFLOW_CNT _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x6)
#define GPIOTS_IOCTL_GET_BUF_OVERFLOW_CNT _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x7)
#define GPIOTS_IOCTL_GET_ISR_CNT          _IO(GPIOTS_IOCTL_MAGIC_NUMBER, 0x8)

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
#if defined(CONFIG_ARCH_MESON64_ODROIDC2)
    int irq0_bank;
    int irq1_bank;
#endif
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
