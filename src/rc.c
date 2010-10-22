/*
   ENEL675 - Advanced Embedded Systems
File: 		rc.c
Authors: 	Robert Tang, John Howe
Date:  		11 September 2010

Creates a kernel module (/dev/rc) which decodes a PPM signal.
The hardware is of an omap board, such as a gumstix or a
beagleboard SBC. It currently uses GPIO_144.

The module works by using interrupts and timestamping with an
internal timer. Firstly, it automatically detects the number of
channels in the PPM signal, and then decodes the signal. Each
time the module is read, It puts the status ("OK", LOST", or
"REALLY_LOST"), followed by the value of each channel, separated
with a comma, and null terminated. e.g. (using a test signal):
"cat /dev/rc returns" RC_OK,102,199,295,392,488,585,681,777\n

TODO: Right now it assumes SYS_CLK = 13MHz. Fix this assumption!
*/

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h> /* kmalloc, kfree etc */
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/clk.h>	
#include <linux/miscdevice.h>
#include <linux/jiffies.h>
#include <mach/gpio.h>
#include <plat/dmtimer.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <plat/mux.h>
#include <linux/sched.h>
#include "rc.h"
#include "ring.h"

#define JIFFIES_TO_MILLISECONDS(x)		(((x) * 1000) / HZ)
#define MAX_CHANNELS				20 

#define RC_DEV_NAME				"rc"
#define RC_PAD_ADDR				(0x2174 + 0x48000000 - OMAP34XX_PADCONF_START) /* This is GPIO_144 */
#define RC_PAD_BIT				16 /* 144 - 128 = 16 */
#define RC_PAD_NUM				144
#define RC_PAD_BASE				OMAP34XX_GPIO5_REG_BASE /* Note: RC_PAD is GPIO_144 -> GPIO group 5 */

#define PPM_START_MIN_10US			600 /* i.e. 6ms */			
#define PPM_START_MAX_10US			1500 /* i.e. 15ms */			
#define PRESCALE_DIV32				32
#define TIMER_PRESCALE_DIV32		4 

#define USER_BUFF_SIZE				128

#define REALLY_LOST				20 /* i.e. 20 x 100ms = 2s */
#define REALLY_LOST_MS          2000 /* i.e. 2s */

typedef enum {DETECT_CHANNELS = 0, DECODE_PPM } rc_mode_t;

typedef struct
{
    char buffer[128];
    ring_t ring;
} rc_channel_t;

typedef struct
{
    unsigned int padconf_reg; /* Store the value of this reg so it can later be returned */
    unsigned int gpio_oe_reg; /* Store the value of this reg so it can later be returned */
    unsigned int gpt_tclr_reg; /* Store the value of this reg so it can later be returned */
    unsigned int ppm_irq;
    unsigned int timer_irq;
    unsigned int timer_zero_val;
    unsigned int lost_counter;
    struct omap_dm_timer *timer_ptr;
    unsigned int num_channels;
    rc_channel_t *channel; /* Dynamically allocated */
    rc_mode_t mode;
    char user_buff[USER_BUFF_SIZE];
    unsigned int last_jiffies;
} rc_dev_t;

/* local variables */
static rc_dev_t rc_dev;

static ssize_t rc_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{	 
    int len, i, j;

    rc_dev.user_buff[0] = '\0';

    /* Status */
    j = strlen(rc_dev.user_buff);
    if(rc_dev.lost_counter == 0 && rc_dev.num_channels && rc_dev.mode != DETECT_CHANNELS)
    {
        snprintf(rc_dev.user_buff + j, USER_BUFF_SIZE, "RC_OK");
    }
    else if(rc_dev.lost_counter < REALLY_LOST && rc_dev.num_channels && (rc_dev.mode != DETECT_CHANNELS || JIFFIES_TO_MILLISECONDS(jiffies - rc_dev.last_jiffies) < REALLY_LOST_MS))
    {
        snprintf(rc_dev.user_buff + j, USER_BUFF_SIZE, "RC_LOST");
    }
    else /* REALLY_LOST */
    {
        snprintf(rc_dev.user_buff + j, USER_BUFF_SIZE, "RC_REALLY_LOST");
    }

    /* Values */
    if(rc_dev.num_channels && rc_dev.lost_counter == 0)
    {
        if(!ring_empty_p(&rc_dev.channel[0].ring) && !ring_empty_p(&rc_dev.channel[rc_dev.num_channels-1].ring))
        {
            for(i = 0; i < rc_dev.num_channels; i++)
            {
                int val;
                j = strlen(rc_dev.user_buff);
                ring_read(&rc_dev.channel[i].ring, &val, sizeof(val));
                snprintf(rc_dev.user_buff + j, USER_BUFF_SIZE, ",%d", val);
            }
        }
    }

    /* New line character */
    j = strlen(rc_dev.user_buff);
    snprintf(rc_dev.user_buff + j, USER_BUFF_SIZE, "\n");

    len = strlen(rc_dev.user_buff);

    if(count < len)
        return -EINVAL;
    if(*ppos != 0)
        return 0;
    if(copy_to_user(buf, rc_dev.user_buff, len))
        return -EINVAL;

    *ppos = len;

    return len;
}

static const struct file_operations rc_fops = 
{
    .owner = THIS_MODULE,
    .read = rc_read,
};

static struct miscdevice rc_misc_dev = 
{
    MISC_DYNAMIC_MINOR,
    RC_DEV_NAME,
    &rc_fops
};

static unsigned int delta_10us(void)
{
    unsigned int reg = omap_dm_timer_read_counter(rc_dev.timer_ptr) - rc_dev.timer_zero_val;
    omap_dm_timer_write_counter(rc_dev.timer_ptr, rc_dev.timer_zero_val);

    return reg >> 2; /* Approx (actually should be divided by 4.0625 rather than 4! Note 13MHz/DIV32 = 406250Hz */
}

/* This isr is designed to overflow at 10Hz */
static irqreturn_t timer_interrupt_handler(int irq, void *dev_id)
{
    /* Reset the timer interrupt status */
    omap_dm_timer_write_status(rc_dev.timer_ptr, OMAP_TIMER_INT_OVERFLOW);
    omap_dm_timer_read_status(rc_dev.timer_ptr);
    /* Increment the lost count, in seconds */
    rc_dev.lost_counter++;
    return IRQ_HANDLED;
}

static irqreturn_t ppm_interrupt_handler(int irq, void *dev_id)
{
    static int pulse = 0;
    unsigned int dt = delta_10us();

    if(dt > PPM_START_MAX_10US) /* Have encountered rather long frame. Need to re-detect channels */
    {
        pulse = 0;
        rc_dev.num_channels = 0;
        if(rc_dev.channel != NULL) /* De-allocate memory for channels... only if it has been allocated before! */
        {
            kfree(rc_dev.channel);
            rc_dev.channel = NULL;
        }
        rc_dev.mode = DETECT_CHANNELS;
    }

    //printk(KERN_ERR "pulse: %d\n", pulse);
    if(rc_dev.mode == DETECT_CHANNELS)
    {
        if(dt > PPM_START_MIN_10US && dt < PPM_START_MAX_10US) /* Have received a start pulse */
        {		
            if(pulse > 0) /* Have received a second start pulse -> change mode */
            {
                int i;
                rc_dev.num_channels = pulse - 1;
                rc_dev.channel = kmalloc(rc_dev.num_channels * sizeof(rc_channel_t), GFP_KERNEL); /* Allocate memory for channels */
                if(rc_dev.channel == NULL)
                {
                    printk(KERN_ERR "kmalloc failed\n");
                    return -1;
                }
                rc_dev.mode = DECODE_PPM;
                rc_dev.lost_counter = 0;
                for(i = 0; i < rc_dev.num_channels; i++)
                {
                    ring_init(&rc_dev.channel[i].ring, rc_dev.channel[i].buffer, sizeof(rc_dev.channel[i].buffer));
                }

                pulse = 0;
            }
            else
            {
                pulse = 1;
            }
        }
        else if(dt < PPM_START_MIN_10US && pulse > 0) /* Have to first receive a start pulse */
        {
            pulse++;
        }
    }
    else
    {
        if(dt > PPM_START_MIN_10US && dt < PPM_START_MAX_10US) /* Have received a start pulse */
        {
            pulse = 0;
            rc_dev.last_jiffies = jiffies;
        }
        else if(pulse < rc_dev.num_channels)
        {
            ring_write_safe(&rc_dev.channel[pulse++].ring, &dt, sizeof(dt));
        }
        else
        {
            rc_dev.mode = DETECT_CHANNELS;
            rc_dev.last_jiffies = jiffies;
        }
    }

    return IRQ_HANDLED;
}

static int rc_hardware_init(bool enable)
{
    void __iomem *base;

    /* Configure mode of RC_PAD */
    base = ioremap(OMAP34XX_PADCONF_START, OMAP34XX_PADCONF_SIZE);
    if(base == NULL)
    {
        printk(KERN_ERR "ioremap(PADCONF) failed\n");
        return -1;
    }
    if(enable)
    {
        rc_dev.padconf_reg = ioread16(base + RC_PAD_ADDR);
        iowrite16(PADCONF_IEN | PADCONF_PULL_UP | PADCONF_PULL_EN | PADCONF_GPIO_MODE, base + RC_PAD_ADDR);
    }
    else 
    {
        iowrite16(rc_dev.padconf_reg, base + RC_PAD_ADDR);
    }
    iounmap(base);

    /* Configure RC_PAD as input */
    base = ioremap(RC_PAD_BASE, OMAP34XX_GPIO_REG_SIZE);
    if(base == NULL)
    {
        printk(KERN_ERR "ioremap(GPIO_OE) failed\n");
        return -1;
    }
    if(enable) 
    {
        rc_dev.gpio_oe_reg = ioread32(base + GPIO_OE_REG_OFFSET);
        iowrite32(rc_dev.gpio_oe_reg & (~(0 << RC_PAD_BIT)), base + GPIO_OE_REG_OFFSET); 

    }
    else 
    {
        iowrite32(rc_dev.gpio_oe_reg, base + GPIO_OE_REG_OFFSET);		
    }
    iounmap(base);

    /* Configure timer and its overflow interrupt */
    if(enable)
    {
        struct clk *gt_fclk;
        rc_dev.timer_ptr = omap_dm_timer_request();
        if(rc_dev.timer_ptr == NULL)
        {
            printk(KERN_ERR "omap_dm_timer_request failed\n");
            return -1;	
        }

        omap_dm_timer_set_source(rc_dev.timer_ptr, OMAP_TIMER_SRC_SYS_CLK);
        omap_dm_timer_set_prescaler(rc_dev.timer_ptr, TIMER_PRESCALE_DIV32);
        rc_dev.timer_irq = omap_dm_timer_get_irq(rc_dev.timer_ptr);

        if(request_irq(rc_dev.timer_irq, timer_interrupt_handler, IRQF_DISABLED | IRQF_TIMER , RC_DEV_NAME, timer_interrupt_handler))
        {
            printk(KERN_ERR "request_irq failed (timer)\n");
            return -1;
        }
        gt_fclk = omap_dm_timer_get_fclk(rc_dev.timer_ptr);
        rc_dev.timer_zero_val = 0xFFFFFFFF - (clk_get_rate(gt_fclk) / (PRESCALE_DIV32 * 10)); // divide by extra 10 to make ISR interrupt at 10Hz
        omap_dm_timer_set_load(rc_dev.timer_ptr, 1, rc_dev.timer_zero_val);
        omap_dm_timer_set_int_enable(rc_dev.timer_ptr, OMAP_TIMER_INT_OVERFLOW);
        omap_dm_timer_start(rc_dev.timer_ptr);
    }
    else
    {
        omap_dm_timer_stop(rc_dev.timer_ptr);
        free_irq(rc_dev.timer_irq, timer_interrupt_handler); 
        omap_dm_timer_free(rc_dev.timer_ptr);
    }

    /* Configure interrupt */
    if(enable)
    {
        if(request_irq(rc_dev.ppm_irq, ppm_interrupt_handler, IRQF_TRIGGER_FALLING, RC_DEV_NAME, &rc_dev))
        {
            printk(KERN_ERR "request_irq failed (io)\n");
            return -1;
        }
    }
    else
    {
        free_irq(rc_dev.ppm_irq, &rc_dev); 
    }

    return 0;
}

static int __init rc_init(void)
{
    unsigned int ret = misc_register(&rc_misc_dev);
    if(ret)
    {
        printk(KERN_ERR "Unable to register \"rc\" misc device\n");
        return -1;
    }

    /* Setup rc_dev structure */
    rc_dev.ppm_irq = gpio_to_irq(RC_PAD_NUM);
    rc_dev.num_channels = 0;
    rc_dev.channel = NULL;
    rc_dev.mode = DETECT_CHANNELS;
    rc_dev.lost_counter = 0;
    rc_dev.last_jiffies = 0;

    /* Setup hardware */
    ret = rc_hardware_init(true);

    return ret;
}

static void __exit rc_exit(void)
{
    misc_deregister(&rc_misc_dev);	
    /* Return RC_PAD to its original state */
    rc_hardware_init(false);

    if(rc_dev.channel != NULL)
    {
        kfree(rc_dev.channel);
    }
}

module_init(rc_init);
module_exit(rc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rct42 jrh97");
MODULE_DESCRIPTION("Decodes standard radio control PPM signals");
MODULE_VERSION("dev");
