#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kthread.h>

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <asm/msr.h>

#define DEVICE_1	"HCSR_1"
#define DEVICE_2	"HCSR_2"

#define GPIO_ECHO 13
#define GPIO_ECHO_LVL_SHFTR 34
#define GPIO_ECHO_PULL_UP 35
#define GPIO_ECHO_FMUX 77

#define GPIO_TRIGGER 14
#define GPIO_TRIGGER_LVL_SHFTR 16
#define GPIO_TRIGGER_PULL_UP 17
#define GPIO_TRIGGER_FMUX 76

#define GPIO_HIGH 1
#define GPIO_LOW 0

#define GPIO_INPUT 1
#define GPIO_OUTPUT 0

#define MODE_ONE_SHOT 0
#define MODE_CONTINUOUS 1

#define STOP_CONT_TRGGR 0
#define START_CONT_TRIGGER 1

#define STRINGIFY(x) #x

static struct hcsr_dev{
	struct miscdevice misc_dev;
	int mode;
	int frequency;
<<<<<<< HEAD
	int buffer[5];
=======
	unsigned long buffer[5];
>>>>>>> upstream/master
	struct task_struct* trigger_task_struct;
}*hcsr_devp[2];


void gpio_init(int pin, char* name, int direction, int value){
	int ret;
	ret = gpio_request(pin, name);
	if(ret)
		printk("GPIO pin number %d could not be requested.\n", pin);

	if(direction == GPIO_INPUT){
		ret = gpio_direction_input(pin);
		if(ret)
			printk("GPIO pin number %d could not be set as input.\n", pin);
	}
	else if(direction == GPIO_OUTPUT){
		ret = gpio_direction_output(pin, value);
		if(ret)
			printk("GPIO pin number %d could not be set as output.\n", pin);
		
		// Direction output didn't seem to init correctly.		
		gpio_set_value_cansleep(pin, value); 
	}
}

void free_GPIOs(void){

	gpio_free(GPIO_ECHO);
	gpio_free(GPIO_ECHO_LVL_SHFTR);
	gpio_free(GPIO_ECHO_PULL_UP);
	gpio_free(GPIO_ECHO_FMUX);

	gpio_free(GPIO_TRIGGER);
	gpio_free(GPIO_TRIGGER_LVL_SHFTR);
	gpio_free(GPIO_TRIGGER_PULL_UP);
	gpio_free(GPIO_TRIGGER_FMUX);
};

/// IRQ Handler
static unsigned long time_rise, time_fall, time_diff;
static irq_handler_t echo_handler(int irq, void *dev_id, struct pt_regs *regs){	
	static char entry =0;
	static int i =0;

	struct hcsr_dev *hcsr_devp = (struct hcsr_dev *) dev_id;
	
 	if(entry == 0){
	 	rdtscl(time_rise);
		entry =1;
		irq_set_irq_type(irq,IRQF_TRIGGER_FALLING);
	}
	else{
		rdtscl(time_fall);
		time_diff = time_fall - time_rise;
		hcsr_devp->buffer[0] = time_fall; //(time_diff / (58*400));
		hcsr_devp->buffer[1] = time_rise; //(time_diff / (58*400));
		entry =0;
		irq_set_irq_type(irq,IRQF_TRIGGER_RISING);
	}
	
	i = (i+1)%5; 	
	

	hcsr_devp->buffer[2] = 0;
	return (irq_handler_t)IRQ_HANDLED;
}

static int hcsr_driver_open(struct inode *inode, struct file *file){
	int device_no = 0, ret = 0;
	struct miscdevice *c;
<<<<<<< HEAD
	int echo_irq;
	int ret;
=======
	unsigned int echo_irq =0;
>>>>>>> upstream/master

	device_no = MINOR(inode->i_rdev);
	printk(KERN_ALERT"\nIn open, minor no = %d\n",device_no);
	
	list_for_each_entry(c, &hcsr_devp[0]->misc_dev.list, list) { 

		if(c->minor == device_no){
			printk(KERN_ALERT"Device: %s Opened\n", c->name);

			if(!strncmp(c->name, DEVICE_1, 6)){
				printk(KERN_ALERT"HSCR 1 Opened\n");
				file->private_data = hcsr_devp[0];
				break;
			}
			else if(!strncmp(c->name, DEVICE_2, 6)){
				printk(KERN_ALERT"HSCR 2 Opened\n");
				file->private_data = hcsr_devp[1];
				break;
			}
			else{
				printk(KERN_ALERT"FILE OPEN: NAME NOT FOUND\n");
				return -1;
			}
		}
	}

<<<<<<< HEAD
=======
	echo_irq = gpio_to_irq(GPIO_ECHO);   // associate irq to echo pin

	ret = request_irq(echo_irq, (irq_handler_t)echo_handler, IRQF_TRIGGER_RISING, "Echo_Dev", hcsr_devp[0]);
	if(ret < 0){
		printk("Error requesting IRQ: %d\n", ret);
	}

>>>>>>> upstream/master
	gpio_init(GPIO_ECHO_FMUX, STRINGIFY(GPIO_ECHO_FMUX), 2, GPIO_LOW);
	gpio_init(GPIO_ECHO_PULL_UP, STRINGIFY(GPIO_ECHO_PULL_UP), GPIO_OUTPUT, GPIO_LOW);
	gpio_init(GPIO_ECHO_LVL_SHFTR, STRINGIFY(GPIO_ECHO_LVL_SHFTR), GPIO_OUTPUT, GPIO_HIGH);
	gpio_init(GPIO_ECHO, STRINGIFY(GPIO_ECHO), GPIO_INPUT, GPIO_LOW);
	
	gpio_init(GPIO_TRIGGER_FMUX, STRINGIFY(GPIO_TRIGGER_FMUX), 2, GPIO_LOW);
	gpio_init(GPIO_TRIGGER_PULL_UP, STRINGIFY(GPIO_TRIGGER_PULL_UP), GPIO_OUTPUT, GPIO_LOW);
	gpio_init(GPIO_TRIGGER_LVL_SHFTR, STRINGIFY(GPIO_TRIGGER_LVL_SHFTR), GPIO_OUTPUT, GPIO_LOW);
	gpio_init(GPIO_TRIGGER, STRINGIFY(GPIO_TRIGGER), GPIO_OUTPUT, GPIO_LOW);

<<<<<<< HEAD
	ret = request_irq(echo_irq, (irq_handler_t)echo_handler, IRQF_TRIGGER_RISING, "Echo_Dev", hcsr_devp);
	if(ret < 0){
		printk("Error requesting IRQ: %d\n", ret);
	}

=======
>>>>>>> upstream/master
	return 0;
}

static int hcsr_driver_close(struct inode *inode, struct file *file){
<<<<<<< HEAD
	unsigned int echo_irq =0;
	echo_irq = gpio_to_irq(GPIO_ECHO);
	free_irq(echo_irq, (void *)hcsr_devp);
	
=======
>>>>>>> upstream/master
	free_GPIOs();
	return 0;

}

inline void trigger_HCSR(void){
	gpio_set_value(GPIO_TRIGGER, 1); //14
	udelay(10);
	gpio_set_value(GPIO_TRIGGER, 0); //14
}

int trigger_func(void* data){
	while(!kthread_should_stop()){
		trigger_HCSR();
		msleep(60);
	}
<<<<<<< HEAD
=======
		return 0;
>>>>>>> upstream/master
}

static ssize_t hcsr_driver_write(struct file *file, const char *buf,size_t count, loff_t *ppos){
	struct hcsr_dev *hcsr_devp = file->private_data;
	int input;
	unsigned long time;
	struct task_struct* trigger_task_struct;
	//unsigned int echo_irq =0;
	if(buf == NULL){
		printk("buf NULL\n");
	}
	if(copy_from_user(&input, buf, sizeof(int)) != 0)
		return -EFAULT;

	// TO DO: remove later
	hcsr_devp->mode = MODE_CONTINUOUS;

	printk("In write\n");
	printk("mode: %d\n", input);

	if(hcsr_devp->mode == MODE_ONE_SHOT){  //one shot mode
		kthread_stop(hcsr_devp->trigger_task_struct);
	}
	else if(hcsr_devp->mode == MODE_CONTINUOUS){  //continous mode 
		if(input == STOP_CONT_TRGGR){ // stop continuous triggering
<<<<<<< HEAD
			kthread_stop(hcsr_devp->trigger_task_struct);
=======
			printk("stop triggering\n");
			
>>>>>>> upstream/master
		}
		else if(input == START_CONT_TRIGGER){  // start continuous triggering
			
			printk("before start trigger %lu\n",rdtscl(time));
			trigger_task_struct = kthread_run(trigger_func, NULL, "%s-trigger_func",hcsr_devp->misc_dev.name);
			if(IS_ERR(trigger_task_struct)){
				printk("WRITE: Could not start Kthread\n");
				return PTR_ERR(trigger_task_struct);
			}
<<<<<<< HEAD
=======

>>>>>>> upstream/master
			hcsr_devp->trigger_task_struct = trigger_task_struct;
		}
	}
	
	return 0;
}

static ssize_t hcsr_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos){
	int bytes_read = 0;
	struct hcsr_dev *hcsr_devp = file->private_data;
	/*
	if(copy_to_user(buf, &(hcsr_devp->buffer)[0] , sizeof(int)) != 0)
		return -EFAULT;
	*/

	//for(i=0;i<5;i++){
		/*printk(KERN_ALERT "Reading: %lu \n", hcsr_devp->buffer[0]);
		printk(KERN_ALERT "Reading: %lu \n", hcsr_devp->buffer[1]);
		printk(KERN_ALERT "Reading, should be 0: %lu \n", hcsr_devp->buffer[2]);

		printk(KERN_ALERT "rise time: %lu \n", time_rise);
		printk(KERN_ALERT "fall time: %lu \n", time_fall);*/
		printk(KERN_ALERT "diff: %lu \n", (time_diff / (58*400)));
		printk(KERN_ALERT "Reading, should be 0: %lu \n\n", hcsr_devp->buffer[2]);
		msleep(1000);

	//}
	return bytes_read;

}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations hcsr_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= hcsr_driver_open,        /* Open method */
    .release	= hcsr_driver_close,     /* Release method */
    .write		= hcsr_driver_write,       /* Write method */
    .read		= hcsr_driver_read,        /* Read method */
};

/* Misc structure */
static struct miscdevice hcsr_dev1 = {
 .minor = MISC_DYNAMIC_MINOR, 
 .name = DEVICE_1,      
 .fops = &hcsr_fops  
};

static struct miscdevice hcsr_dev2 = {
 .minor = MISC_DYNAMIC_MINOR, 
 .name = DEVICE_2,      
 .fops = &hcsr_fops  
};

static int __init hcsr_driver_init(void){
	int ret;
	
<<<<<<< HEAD
	


=======
>>>>>>> upstream/master
  hcsr_devp[0] = kmalloc(sizeof(struct hcsr_dev), GFP_KERNEL);
   if(!hcsr_devp[0]){
   		printk("Kmalloc failed 1\n");
   		return -1;
   }

   hcsr_devp[1] = kmalloc(sizeof(struct hcsr_dev), GFP_KERNEL);
   if(!hcsr_devp[1]){
   		printk("Kmalloc failed");
   		return -1;
   }

  ret = misc_register(&hcsr_dev1);
  if (ret){
  	printk(KERN_ERR"Unable to register misc device 1\n");
  	kfree(hcsr_devp[0]);
  	kfree(hcsr_devp[1]);
  	return ret;
  }
  
  ret = misc_register(&hcsr_dev2);
  if (ret){
  	printk(KERN_ERR"Unable to register misc device 2\n");
  	kfree(hcsr_devp[0]);
  	kfree(hcsr_devp[1]);
  	return ret;
  }

  hcsr_devp[0]->misc_dev = hcsr_dev1;
  //hcsr_devp[0]->minor = hcsr_dev1.minor;

  hcsr_devp[1]->misc_dev = hcsr_dev2;
  //hcsr_devp[1]->minor = hcsr_dev2.minor;
  return 0;
  
}

/* Driver Exit */
void __exit hcsr_driver_exit(void){
<<<<<<< HEAD

=======
	kthread_stop(hcsr_devp[0]->trigger_task_struct);
>>>>>>> upstream/master

	kfree(hcsr_devp[0]);
	kfree(hcsr_devp[1]);
	misc_deregister(&hcsr_dev1);
	misc_deregister(&hcsr_dev2);
	
	//
	printk("hcsr driver removed.\n");
}

module_init(hcsr_driver_init);
module_exit(hcsr_driver_exit);
MODULE_LICENSE("GPL v2");
