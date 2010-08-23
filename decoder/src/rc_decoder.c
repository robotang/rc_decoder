/*
 * "rc_decoder, world!" minimal kernel module - /dev version
 *
 * Valerie Henson <val@nmt.edu>
 *
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>

#include <asm/uaccess.h>

#include "timestamp.h"
#include "decoder.h"

/*
 * rc_decoder_read is the function called when a process calls read() on
 * /dev/rc_decoder.  It writes "rc_decoder, world!" to the buffer passed in the
 * read() call.
 */

static ssize_t rc_decoder_read(struct file * file, char * buf, 
			  size_t count, loff_t *ppos)
{
	char *rc_decoder_str = "rc_decoder, world!\n";
	int len = strlen(rc_decoder_str); /* Don't include the null byte. */
	/*
	 * We only support reading the whole string at once.
	 */
	if (count < len)
		return -EINVAL;
	/*
	 * If file position is non-zero, then assume the string has
	 * been read and indicate there is no more data to be read.
	 */
	if (*ppos != 0)
		return 0;
	/*
	 * Besides copying the string to the user provided buffer,
	 * this function also checks that the user has permission to
	 * write to the buffer, that it is mapped, etc.
	 */
	if (copy_to_user(buf, rc_decoder_str, len))
		return -EINVAL;
	/*
	 * Tell the user how much data we wrote.
	 */
	*ppos = len;

	return len;
}

/*
 * The only file operation we care about is read.
 */

static const struct file_operations rc_decoder_fops = {
	.owner		= THIS_MODULE,
	.read		= rc_decoder_read,
};

static struct miscdevice rc_decoder_dev = {
	/*
	 * We don't care what minor number we end up with, so tell the
	 * kernel to just pick one.
	 */
	MISC_DYNAMIC_MINOR,
	/*
	 * Name ourselves /dev/rc_decoder.
	 */
	"rc_decoder",
	/*
	 * What functions to call when a program performs file
	 * operations on the device.
	 */
	&rc_decoder_fops
};

static int __init
rc_decoder_init(void)
{
	int ret;

	/*
	 * Create the "rc_decoder" device in the /sys/class/misc directory.
	 * Udev will automatically create the /dev/rc_decoder device using
	 * the default rules.
	 */
	ret = misc_register(&rc_decoder_dev);
	if (ret)
		printk(KERN_ERR
		       "Unable to register \"rc_decoder, world!\" misc device\n");

	return ret;
}

module_init(rc_decoder_init);

static void __exit
rc_decoder_exit(void)
{
	misc_deregister(&rc_decoder_dev);
}

module_exit(rc_decoder_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Valerie Henson <val@nmt.edu>");
MODULE_DESCRIPTION("\"rc_decoder, world!\" minimal module");
MODULE_VERSION("dev");
