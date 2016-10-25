#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "common_data.h"

#define PLATFORM_DEVICE_NAME "platform_HCSR04"

struct hcsr_dev{
	struct miscdevice misc_dev;
	struct platform_device platform_dev;

	struct gpio_pair dev_gpio_pair;
	struct mode_pair dev_mode_pair;

	long buffer[5];				// buffer and its head, tail size vars
	int head;
	int tail;
	int size;

	unsigned int upcount;
	int enable;
	
	struct semaphore buffer_signal;		// GPIO irq handler signals non empty buffer and read func waits on it if buffer empty
	
	struct task_struct* trigger_task_struct; // kernel thread used to trigger in coninuous mode and check state
}*hcsr_devp[2];

static void hcsr_dev_release(struct device *dev){
	//complete(&device_release);
}

static int __init platform_hcsr_device_init(void){
	hcsr_devp[0] = kzalloc(sizeof(struct hcsr_dev), GFP_KERNEL);
	hcsr_devp[1] = kzalloc(sizeof(struct hcsr_dev), GFP_KERNEL);

	hcsr_devp[0]->platform_dev.name = PLATFORM_DEVICE_NAME;
	hcsr_devp[1]->platform_dev.name = PLATFORM_DEVICE_NAME;

	hcsr_devp[0]->platform_dev.id =0;
	hcsr_devp[1]->platform_dev.id =1;

	hcsr_devp[0]->platform_dev.dev.release = hcsr_dev_release;
	hcsr_devp[1]->platform_dev.dev.release = hcsr_dev_release;

	platform_set_drvdata(&(hcsr_devp[0]->platform_dev), hcsr_devp[0]);
	platform_set_drvdata(&(hcsr_devp[1]->platform_dev), hcsr_devp[1]);

	platform_device_register(&(hcsr_devp[0]->platform_dev));
	platform_device_register(&(hcsr_devp[1]->platform_dev));

	// hcsr_devp[0]->platform_dev = platform_device_alloc(PLATFORM_DEVICE_NAME, 0);
	// hcsr_devp[1]->platform_dev = platform_device_alloc(PLATFORM_DEVICE_NAME, 1);

	// platform_set_drvdata((hcsr_devp[0]->platform_dev), hcsr_devp[0]);
	// platform_set_drvdata((hcsr_devp[1]->platform_dev), hcsr_devp[1]);
	
	// platform_device_add(hcsr_devp[0]->platform_dev);
	// platform_device_add(hcsr_devp[1]->platform_dev);
	return 0;
}

/* Driver Exit */
void __exit platform_hcsr_device_exit(void){
	platform_device_unregister(&hcsr_devp[0]->platform_dev);
	platform_device_unregister(&hcsr_devp[1]->platform_dev);

	// platform_device_del(hcsr_devp[0]->platform_dev);
	// platform_device_del(hcsr_devp[1]->platform_dev);

	kfree(hcsr_devp[0]);
	kfree(hcsr_devp[1]);

	printk("hcsr platform device removed.\n");
}

module_init(platform_hcsr_device_init);
module_exit(platform_hcsr_device_exit);
MODULE_LICENSE("GPL v2");