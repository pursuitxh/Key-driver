/*
 * @file: gpio.c
 * 
 * Created on: Nov 6, 2012
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/sched.h>
#include "mvApb.h"

#undef IRQ_POWER
#undef IRQ_DC_DETECT
#undef IRQ_DEFAULT_SET

#define IRQ_DEFAULT_SET		(32+5)
#define IRQ_DC_DETECT		(32+6)
#define IRQ_POWER		(32+8)

#define DEFAULT_SET_KEY		5
#define	DC_DETECT_KEY		6
#define POWER_KEY		8

#define NUM_KEY			3
#define DRIVER_NAME 		"KEY_LB"
#define KEY_MAJOR 		0

static int key_major = KEY_MAJOR;
struct class *key_class;

typedef enum _key_status {
	KEY_STATUS_UP = 0,
	KEY_STATUS_DOWN,
	KEY_STATUS_REPEAT,
	KEY_STATUS_DB_REPEAT
}key_status_e;

typedef enum _key_mode {
	KEY_RESULT_SHORT = 0,
	KEY_RESULT_LONG,
	KEY_RESULT_FACTORY
}key_mode_e;

typedef struct _key_result {
	int 		value;
	key_mode_e 	mode;
}key_result_t;

typedef struct _key_irq_desc {
	unsigned int 	irq;
	unsigned long 	flags;
	unsigned int 	number;
	const char 	*name;
}key_irq_desc_t;

typedef struct _key {
	key_status_e		status;
	key_mode_e		mode;
	struct timer_list	timer;
	key_irq_desc_t		irq_desc;
}ffxav_key_t;

struct key_dev {
	struct cdev 		cdev;
	ffxav_key_t		ffxav_key[NUM_KEY];
	key_result_t		result;
	atomic_t 		key_open;
	struct fasync_struct 	*key_fasync;
};

static void key_timer_handle_powerkey(unsigned long value);
static void key_timer_handle_common_key(unsigned long value);

struct key_attr_id {
	unsigned int 	irq;
	unsigned int 	number;
	const char 	*name;
	unsigned long 	data;
	void (*function)(unsigned long);
	unsigned long 	flags;
	key_status_e  	status;
};

static struct key_attr_id key_attr_table[] = {
	[0] = {
		.irq 	  = IRQ_POWER,
		.number   = POWER_KEY,
		.name 	  = "POWER_KEY",
		.data 	  = POWER_KEY,
		.function = &key_timer_handle_powerkey,
		.flags 	  = IRQ_TYPE_EDGE_RISING|IRQ_TYPE_EDGE_FALLING,
		.status   = KEY_STATUS_UP,
	},

	[1] = {
		.irq 	  = IRQ_DEFAULT_SET,
		.number   = DEFAULT_SET_KEY,
		.name 	  = "DC_DEFAULT_KEY",
		.data     = DEFAULT_SET_KEY,
		.function = &key_timer_handle_common_key,
		.flags    = IRQ_TYPE_EDGE_RISING|IRQ_TYPE_EDGE_FALLING,
		.status   = KEY_STATUS_UP,
	},

	[2] = {
		.irq 	  = IRQ_DC_DETECT,
		.number   = DC_DETECT_KEY,
		.name     = "DC_DETECT_KEY",
		.data     = DC_DETECT_KEY,
		.function = &key_timer_handle_common_key,
		.flags    = IRQ_TYPE_EDGE_RISING|IRQ_TYPE_EDGE_FALLING,
		.status   = KEY_STATUS_UP,
	}
};

struct key_dev *key_devp;

static int get_value_from_index(int index)
{
	int value;
	
	if (index == 0) {
		value = POWER_KEY;
	} else if (index == 1) {
		value = DEFAULT_SET_KEY;
	} else if (index == 2) {
		value = DC_DETECT_KEY;
	} else {
		value = -1;
	}
	
	return value;	
}

static int get_index_from_value(unsigned long value)
{
	int index;
	
	if (value == POWER_KEY) {
		index = 0;
	} else if (value == DEFAULT_SET_KEY) {
		index = 1;
	} else if (value == DC_DETECT_KEY) {
		index = 2;
	} else {
		index = -1;
	}
	
	return index;
}

static void report_key(int index, int mode)
{
	key_devp->result.value = get_value_from_index(index);
	key_devp->result.mode = mode;
	pr_debug("Key%d press%d\n", key_devp->result.value,
				    key_devp->result.mode);

	kill_fasync(&key_devp->key_fasync, SIGIO, POLL_IN);
}

static void key_timer_handle_powerkey(unsigned long value)
{
	volatile int key_down = gpio_get_value(value);
	int index = get_index_from_value(value);

	if (key_devp->ffxav_key[index].status == KEY_STATUS_UP) {
		if (key_down) {
			key_devp->ffxav_key[index].status = KEY_STATUS_DOWN;
			mod_timer(&(key_devp->ffxav_key[index].timer), jiffies+3*HZ);
		} else {
			/* key shake, we need do nothing */
		}
	} else if (key_devp->ffxav_key[index].status == KEY_STATUS_DOWN) {
		if (key_down) {
			key_devp->ffxav_key[index].status = KEY_STATUS_REPEAT;
			/* report here only for led indicate */
			report_key(index, KEY_RESULT_LONG);
			mod_timer(&(key_devp->ffxav_key[index].timer), jiffies+5*HZ);
		} else {
			del_timer(&(key_devp->ffxav_key[index].timer));
			report_key(index, KEY_RESULT_SHORT);
			key_devp->ffxav_key[index].status = KEY_STATUS_UP;
		}
	} else if (key_devp->ffxav_key[index].status == KEY_STATUS_REPEAT) {
		if (key_down) {
			key_devp->ffxav_key[index].status = KEY_STATUS_DB_REPEAT;
			/* report here only for led indicate */
			report_key(index, KEY_RESULT_FACTORY);
		} else {
			del_timer(&(key_devp->ffxav_key[index].timer));
			/* should report here */
			/* report_key(index, KEY_RESULT_LONG); */
			key_devp->ffxav_key[index].status = KEY_STATUS_UP;
		}
	} else if (key_devp->ffxav_key[index].status == KEY_STATUS_DB_REPEAT) {
			/* should report here */
			/* report_key(index, KEY_RESULT_FACTORY); */
		key_devp->ffxav_key[index].status = KEY_STATUS_UP;
	}
}

static void key_timer_handle_common_key(unsigned long value)
{
	volatile int key_up = gpio_get_value(value);
	int index = get_index_from_value(value);

	if (key_up) {
		/* key up or key shake */
		key_devp->ffxav_key[index].status = KEY_STATUS_UP;

	} else {
		key_devp->ffxav_key[index].status = KEY_STATUS_DOWN;
		report_key(index, KEY_RESULT_SHORT);
	}
}

static irqreturn_t gpio_detect_irq(int irq, void *dev_id)
{
	int key_num = *(int *)dev_id;
	int index = get_index_from_value(key_num);

	mod_timer(&(key_devp->ffxav_key[index].timer), jiffies + HZ/50);

	return IRQ_RETVAL(IRQ_HANDLED);
}

static ssize_t key_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct key_dev *dev = filp->private_data;
	unsigned long err;

	err = copy_to_user(buf, (void *)&(dev->result), sizeof(dev->result));

	return err? -EFAULT : 0;
}

static int key_fasync(int fd, struct file *filp, int mode)
{
	struct key_dev *dev = filp->private_data;

	pr_debug("Enter %s\n", __FUNCTION__);

	return fasync_helper(fd, filp, mode, &dev->key_fasync);
}

static void gpio_set(int gpio)
{
	int ret;

	ret = gpio_request(gpio, DRIVER_NAME);

	if(ret) {
		pr_err("request gpio%d err: %d!\n", gpio, ret);
		gpio_free(gpio);
		return;
	}
	gpio_direction_input(gpio);

	return;
}

static void key_irq_request(struct key_dev *dev)
{
	int i, ret;

	ENABLE_GPIO_GROUP2;
	ENABLE_GPIO_GROUP1;
	
	for (i = 0; i < NUM_KEY; i++) {
		gpio_set(dev->ffxav_key[i].irq_desc.number);
		ret = request_irq(dev->ffxav_key[i].irq_desc.irq, 
				gpio_detect_irq,
				dev->ffxav_key[i].irq_desc.flags,
				dev->ffxav_key[i].irq_desc.name,
				(void *)&(dev->ffxav_key[i].irq_desc.number));
		if (ret) {
			pr_err("request irq error: %d...\n", ret);
			break;
		}
	}

	if (ret) {
		i--;
		for (; i>= 0; i++) {
		free_irq(dev->ffxav_key[i].irq_desc.irq,
			&(dev->ffxav_key[i].irq_desc.number));
		gpio_free(dev->ffxav_key[i].irq_desc.number);
		}
	}
}

static int key_open(struct inode *inode, struct file *filp)
{
	if (!atomic_dec_and_test(&key_devp->key_open)) {
		atomic_inc(&key_devp->key_open);
		return -EBUSY;
	}

	filp->private_data = key_devp;

	key_irq_request(key_devp);

	return 0;
}

static int key_release(struct inode *inode, struct file *filp)
{
	struct key_dev *dev = filp->private_data;
	int i;

	pr_debug("Enter %s\n", __FUNCTION__);

	for (i = 0; i < NUM_KEY; i++) {
		free_irq(dev->ffxav_key[i].irq_desc.irq,
			&(dev->ffxav_key[i].irq_desc.number));
		gpio_free(dev->ffxav_key[i].irq_desc.number);
	}

	atomic_inc(&(dev->key_open));
	key_fasync(-1, filp, 0);

	return 0;
}

static struct file_operations gpio_fops = {
	.owner	 = THIS_MODULE,
	.open	 = key_open,
	.read	 = key_read,
	.fasync	 = key_fasync,
	.release = key_release,
};

static void key_attribute_init(struct key_dev *dev)
{
	int i;

	atomic_set(&(dev->key_open), 1);

	for (i = 0; i < NUM_KEY; i++) {
		init_timer(&(dev->ffxav_key[i].timer));
		dev->ffxav_key[i].timer.data = key_attr_table[i].data;
		dev->ffxav_key[i].timer.function = key_attr_table[i].function;

		add_timer(&(dev->ffxav_key[i].timer));

		dev->ffxav_key[i].irq_desc.flags = key_attr_table[i].flags;
		dev->ffxav_key[i].irq_desc.irq = key_attr_table[i].irq;
		dev->ffxav_key[i].irq_desc.number = key_attr_table[i].number;
		dev->ffxav_key[i].irq_desc.name = key_attr_table[i].name;

		dev->ffxav_key[i].status = key_attr_table[i].status;
	}
}

static void key_cdev_setup(struct key_dev *dev, int index)
{
	int err;
	int devnum = MKDEV(key_major, index);

	key_class = class_create(THIS_MODULE, "Key_Driver_LB");
	device_create(key_class, NULL, devnum, NULL, DRIVER_NAME);

	cdev_init(&dev->cdev, &gpio_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &gpio_fops;

	err = cdev_add(&dev->cdev, devnum, 1);
	if (err)
		pr_err("Error %d adding key%d\n", err, index);
}

static int __init key_lb_init(void)
{
	int ret;
	
	dev_t  devnum = MKDEV(KEY_MAJOR, 0);
	if (key_major) {
		ret = register_chrdev_region(devnum, 1, DRIVER_NAME);
		pr_info("static key major is: %d\n", key_major);
	} else {
		ret = alloc_chrdev_region(&devnum, 0, 1, DRIVER_NAME);
		key_major = MAJOR(devnum);
		pr_info("automatic key major is: %d\n", key_major);
	}
	
	if (ret < 0) {
		pr_err("key driver for LB: can't get major number...\n");
		return ret;
	}
	
	key_devp = kzalloc(sizeof(struct key_dev), GFP_KERNEL);
	if (!key_devp) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	key_cdev_setup(key_devp, 0);

	key_attribute_init(key_devp);

	pr_info("GPIO module insmod successed!\n");
	
	return ret;

fail_malloc: 
	unregister_chrdev_region(devnum, 1);
	return	ret;
}

static void __exit key_lb_exit(void)
{
	cdev_del(&key_devp->cdev);
	kfree(key_devp);
	key_devp = NULL;
	
	device_destroy(key_class, MKDEV(key_major, 0));
	class_destroy(key_class);
	
	unregister_chrdev_region(MKDEV(key_major, 0), 1);

	pr_info("GPIO module removed successed!\n");
}

module_init(key_lb_init);
module_exit(key_lb_exit);

MODULE_AUTHOR("pursuitxh");
MODULE_DESCRIPTION("GPIO driver for Lenovo Board");
MODULE_LICENSE("GPL");
