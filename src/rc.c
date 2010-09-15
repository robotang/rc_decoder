/*
	ENEL675 - Advanced Embedded Systems
	File: 		rc.c
	Author: 	rct42 jrh97
	Date:  		11 September 2010
	Description:	Creates a kernel module (/dev/rc) which decodes a PPM 
			signal. The hardware is of an omap board, such as a 
			gumstix or a beagleboard SBC. It currently uses 
			GPIO_144.
			
			The module works by using interrupts and timestamping 
			with an internal timer. Firstly, it automatically 
			detects the number of channels in the PPM signal, and 
			then decodes the signal. Each time the module is read, 
			it will output the value of each channel, separated with
			 a space, and null terminated. e.g. (using a test 
			 signal): "cat /dev/rc returns" 
			 102 199 295 392 488 585 681 777 

			TODO: Explicitly set the clock speed into the timer using PRCM			
			TODO: Incorporate a ring buffer into the system
			TODO: Test on a proper PPM signal
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
#include <linux/miscdevice.h>
#include <mach/gpio.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <plat/mux.h>
#include "rc.h"

#define RC_DEV_NAME				"rc"
#define RC_PAD_ADDR				(0x2174 + 0x48000000 - OMAP34XX_PADCONF_START) /* This is GPIO_144 */
#define RC_PAD_BIT				16 /* 144 - 128 = 16 */
#define RC_PAD_NUM				144
#define RC_PAD_BASE				OMAP34XX_GPIO5_REG_BASE /* Note: RC_PAD is GPIO_144 -> GPIO group 5 */
#define RC_GPTIMER_NUM				8
#define RC_GPTIMER_BASE				GPTIMER8

#define PPM_START_MIN_10US			1000 /* i.e. 10ms. TODO: This value may need to be adjusted */			
#define PPM_START_MAX_10US			4000 /* i.e. 40ms, TODO: This value may need to be adjusted */			

#define USER_BUFF_SIZE				128

typedef enum {B_FALSE = 0, B_TRUE} bool_t;

typedef enum {DETECT_CHANNELS = 0, DECODE_PPM } rc_mode_t;

typedef struct 
{
	unsigned int val;
} rc_channel_t;

typedef struct 
{
	unsigned int padconf_reg; /* Store the value of this reg so it can later be returned */
	unsigned int gpio_oe_reg; /* Store the value of this reg so it can later be returned */
	unsigned int gpt_tclr_reg; /* Store the value of this reg so it can later be returned */
	unsigned int irq;
	unsigned int num_channels;
	rc_channel_t *channel; /* Dynamically allocated */
	rc_mode_t mode;
	char user_buff[USER_BUFF_SIZE];
} rc_dev_t;

/* local variables */
static rc_dev_t rc_dev;

static ssize_t rc_read(struct file * file, char * buf, size_t count, loff_t *ppos)
{	 
	int len, i;
	
	rc_dev.user_buff[0] = '\0';
	for(i = 0; i < rc_dev.num_channels; i++)
	{
		int j = strlen(rc_dev.user_buff);
		snprintf(rc_dev.user_buff + j, USER_BUFF_SIZE, "%d ", rc_dev.channel[i].val); 
	}	
	
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
	void __iomem *base;
	unsigned int reg;

	base = ioremap(RC_GPTIMER_BASE, GPT_REGS_PAGE_SIZE);
	if(base == NULL)
	{
		printk(KERN_ERR "ioremap(PADCONF) failed\n");
		return -1;
	}
	/* Read timer */
	reg = ioread32(base + GPT_TCRR);
	/* Zero the timer */
	iowrite32(0x00000000, base + GPT_TCRR);	
	iounmap(base);
	
	return reg >> 2; /* Approx (actually should be divided by 4.0625 rather than 4! Note 13MHz/DIV32 = 406250Hz */
}

static irqreturn_t interrupt_handler(void)
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
	
	if(rc_dev.mode == DETECT_CHANNELS)
	{
		if(dt > PPM_START_MIN_10US && dt < PPM_START_MAX_10US) /* Have received a start pulse */
		{
			if(pulse > 0) /* Have received a second start pulse -> change mode */
			{
				rc_dev.num_channels = pulse - 1;
				rc_dev.channel = kmalloc(rc_dev.num_channels * sizeof(rc_channel_t), GFP_KERNEL); /* Allocate memory for channels */
				if(rc_dev.channel == NULL)
				{
					printk(KERN_ERR "kmalloc failed\n");
					return -1;
				}
				rc_dev.mode = DECODE_PPM;
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
		}
		else if(pulse < rc_dev.num_channels)
		{
			rc_dev.channel[pulse++].val = dt;
		}
	}
	
	return IRQ_HANDLED;
}

static int rc_hardware_init(bool_t enable)
{
	void __iomem *base;
	
	/* Configure mode of RC_PAD */
	base = ioremap(OMAP34XX_PADCONF_START, OMAP34XX_PADCONF_SIZE);
	if(base == NULL)
	{
		printk(KERN_ERR "ioremap(PADCONF) failed\n");
		return -1;
	}
	if(enable == B_TRUE)
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
	if(enable == B_TRUE) 
	{
		rc_dev.gpio_oe_reg = ioread32(base + GPIO_OE_REG_OFFSET);
		iowrite32(rc_dev.gpio_oe_reg & (~(0 << RC_PAD_BIT)), base + GPIO_OE_REG_OFFSET); 
		
	}
	else 
	{
		iowrite32(rc_dev.gpio_oe_reg, base + GPIO_OE_REG_OFFSET);		
	}
	iounmap(base);
	
	/* Configure timer. TODO: Setup clock into timer via PRCM */
	base = ioremap(RC_GPTIMER_BASE, GPT_REGS_PAGE_SIZE);
	if(base == NULL)
	{
		printk(KERN_ERR "ioremap(GPT) failed\n");
		return -1;
	}
	if(enable == B_TRUE)
	{
		rc_dev.gpt_tclr_reg = ioread32(base + GPT_TCLR);
		iowrite32(GPT_TCLR_ST_START | GPT_TCLR_PRESCALE_EN | GPT_TCLR_PS_DIV32, base + GPT_TCLR);
	}
	else
	{
		iowrite32(rc_dev.gpt_tclr_reg, base + GPT_TCLR);
	}
	iounmap(base);

	/* Configure interrupt */
	if(enable == B_TRUE)
	{
		if(request_irq(rc_dev.irq, interrupt_handler, IRQF_TRIGGER_FALLING, RC_DEV_NAME, &rc_dev))
		{
			printk(KERN_ERR "request_irq failed\n");
			return -1;
		}
	}
	else
	{
		free_irq(rc_dev.irq, &rc_dev); 
	}
	
	return 0;
}

static int __init rc_init(void)
{		
	unsigned int ret = misc_register(&rc_misc_dev);
	if (ret)
	{
		printk(KERN_ERR "Unable to register \"rc\" misc device\n");
		return -1;
	}
	
	/* Setup rc_dev structure */
	rc_dev.irq = gpio_to_irq(RC_PAD_NUM);
	rc_dev.num_channels = 0;
	rc_dev.channel = NULL;
	rc_dev.mode = DETECT_CHANNELS;

	/* Setup hardware */
	ret = rc_hardware_init(B_TRUE);
	
	return ret;
}

static void __exit rc_exit(void)
{
	misc_deregister(&rc_misc_dev);	
	/* Return RC_PAD to its original state */
	rc_hardware_init(B_FALSE);
	
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
