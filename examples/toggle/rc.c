#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <mach/gpio.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <plat/mux.h>

/* Definitions relating to configuration of the System Control Module */
#define PADCONF_IEN				(1 << 8) /* Input enable */
#define PADCONF_IDIS				(0 << 8) /* Input disable */
#define PADCONF_PULL_UP				(1 << 4) /* Pull type up */
#define PADCONF_PULL_DOWN			(0 << 4) /* Pull type down */
#define PADCONF_PULL_EN				(1 << 3)
#define PADCONF_PULL_DIS			(0 << 3)
#define PADCONF_GPIO_MODE			4

#define OMAP34XX_PADCONF_START			0x48002030
#define OMAP34XX_PADCONF_SIZE			0x05CC

#define RC_PAD_ADDR				(0x2174 + 0x48000000 - OMAP34XX_PADCONF_START) //This is GPIO_144
#define RC_PAD_BIT				16 /* 144 - 128 = 16 */

/* Definitions relating to configuration of the General Purpose Interface */
#define OMAP34XX_GPIO1_REG_BASE			0x48310000 /* GPIO[  0: 31] */
#define OMAP34XX_GPIO2_REG_BASE			0x49050000 /* GPIO[ 32: 63] */
#define OMAP34XX_GPIO3_REG_BASE			0x49052000 /* GPIO[ 64: 95] */
#define OMAP34XX_GPIO4_REG_BASE			0x49054000 /* GPIO[ 96:127] */
#define OMAP34XX_GPIO5_REG_BASE			0x49056000 /* GPIO[128:159] */
#define OMAP34XX_GPIO6_REG_BASE			0x49058000 /* GPIO[160:191] */
#define OMAP34XX_GPIO_REG_SIZE			1024

#define RC_PAD_REG_BASE				OMAP34XX_GPIO5_REG_BASE /* Note: RC_PAD is GPIO_144 -> GPIO group 5 */

#define GPIO_IRQENABLE1_REG_OFFSET		0x0000001C
#define GPIO_IRQENABLE2_REG_OFFSET		0x0000002C
#define GPIO_OE_REG_OFFSET			0x00000034
#define GPIO_DATAIN_REG_OFFSET			0x00000038
#define GPIO_DATAOUT_REG_OFFSET			0x0000003C
#define GPIO_RISINGDETECT_REG_OFFSET		0x00000048
#define GPIO_FALLINGDETECT_REG_OFFSET		0x0000004C
#define GPIO_CLEARIRQENABLE1_REG_OFFSET		0x00000060
#define GPIO_CLEARIRQENABLE2_REG_OFFSET		0x00000070
#define GPIO_SETIRQENABLE1_REG_OFFSET		0x00000064
#define GPIO_SETIRQENABLE2_REG_OFFSET		0x00000074
#define GPIO_CLEARDATAOUT_REG_OFFSET		0x00000090
#define GPIO_SETDATAOUT_REG_OFFSET		0x00000094

/* local variables */
static unsigned int padconf_reg, gpio_oe_reg;

static unsigned int toggle_output(void)
{
	static unsigned int val = 0;
	void __iomem *base;
	
	base = ioremap(RC_PAD_REG_BASE, OMAP34XX_GPIO_REG_SIZE);
	if(base == NULL)
	{
		printk(KERN_ERR "ioremap(GPIO_DATAOUT) failed\n");
		return -1;
	}

	printk(KERN_ALERT "val: %u\n", val);	
	if(val == 1)
		iowrite32(1 << RC_PAD_BIT, base + GPIO_SETDATAOUT_REG_OFFSET);
	else
		iowrite32(1 << RC_PAD_BIT, base + GPIO_CLEARDATAOUT_REG_OFFSET);
	val = (val > 0) ? 0 : 1;
	iounmap(base);
	
	return 0;
}

static ssize_t rc_read(struct file * file, char * buf, size_t count, loff_t *ppos)
{
	char *rc_str = "Hello, world!\n";
	int len = strlen(rc_str);
		
	if (count < len)
		return -EINVAL;
	if (*ppos != 0)
		return 0;
	if (copy_to_user(buf, rc_str, len))
		return -EINVAL;
	*ppos = len;
	/* Toggle output */
	toggle_output();
	return len;
}

static const struct file_operations rc_fops = 
{
	.owner		= THIS_MODULE,
	.read		= rc_read,
};

static struct miscdevice rc_dev = 
{
	MISC_DYNAMIC_MINOR,
	"rc",
	&rc_fops
};

static int rc_hardware_init(int en)
{
	void __iomem *base;
	base = ioremap(OMAP34XX_PADCONF_START, OMAP34XX_PADCONF_SIZE);
	if(base == NULL)
	{
		printk(KERN_ERR "ioremap(PADCONF) failed\n");
		return -1;
	}
	if(en == 1) /* Configure RC_PAD as mode 4 (GPIO) */
	{
		padconf_reg = ioread16(base + RC_PAD_ADDR);
		iowrite16(PADCONF_IEN | PADCONF_PULL_DIS | PADCONF_GPIO_MODE, base + RC_PAD_ADDR);
	}
	else /* Return RC_PAD to its initial state */
	{
		iowrite16(padconf_reg, base + RC_PAD_ADDR);
	}
	iounmap(base);
	
	base = ioremap(RC_PAD_REG_BASE, OMAP34XX_GPIO_REG_SIZE);
	if(base == NULL)
	{
		printk(KERN_ERR "ioremap(GPIO_OE) failed\n");
		return -1;
	}
	if(en == 1) /* Configure RC_PAD as output */
	{
		gpio_oe_reg = ioread32(base + GPIO_OE_REG_OFFSET);
		iowrite32(gpio_oe_reg & (~(1 << RC_PAD_BIT)), base + GPIO_OE_REG_OFFSET);
	}
	else /* Return RC_PAD to its initial state */
	{
		iowrite32(gpio_oe_reg, base + GPIO_OE_REG_OFFSET);
	}
	iounmap(base);
	return 0;
}

static int __init rc_init(void)
{		
	unsigned int ret = misc_register(&rc_dev);
	if (ret)
	{
		printk(KERN_ERR "Unable to register \"rc\" misc device\n");
		return -1;
	}
	/* Setup RC_PAD */
	ret = rc_hardware_init(1);
	
	return ret;
}

static void __exit rc_exit(void)
{
	misc_deregister(&rc_dev);	
	/* Return RC_PAD to its original state */
	rc_hardware_init(0);
}

module_init(rc_init);
module_exit(rc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rct42 jrh97");
MODULE_DESCRIPTION("Decodes standard radio control PPM signals");
MODULE_VERSION("dev");
