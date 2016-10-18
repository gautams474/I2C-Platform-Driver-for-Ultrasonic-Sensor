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

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>

#define DEVICE_1	"HCSR_1"
#define DEVICE_2	"HCSR_2"


#define GPIO_ECHO_LVL_SHFTR 34
#define GPIO_ECHO_PULL_UP 35
#define GPIO_ECHO_FMUX 77

#define GPIO_TRIGGER_LVL_SHFTR 16
#define GPIO_TRIGGER_PULL_UP 17
#define GPIO_TRIGGER_FMUX 76

#define GPIO_HIGH 1
#define GPIO_LOW 0

#define GPIO_INPUT 1
#define GPIO_OUTPUT 0

#define STRINGIFY(x) #x

static struct hcsr_dev{
	struct miscdevice misc_dev;
	int mode;
	int frequency;
	int buffer[5];
}*hcsr_devp[2];

struct timer_list my_timer;

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

void free_GPIOs(struct hcsr_dev *hcsr_devp){
	unsigned int echo_irq;
	int echo_pin = hcsr_devp->dev_gpio_pair.echo;
	int trigger_pin = hcsr_devp->dev_gpio_pair.trigger;

	if(echo != -1){
		echo_irq = gpio_to_irq(echo_pin);
		free_irq(echo_irq, (void *)hcsr_devp);
		gpio_free(echo_pin);
	}
	if(trigger_pin != -1)
		gpio_free(trigger_pin);
};

inline void trigger_HCSR(void){
	gpio_set_value(GPIO_TRIGGER, 1); //14
	udelay(10);
	gpio_set_value(GPIO_TRIGGER, 0); //14
}

static int hcsr_driver_open(struct inode *inode, struct file *file){
	int device_no = 0;
	struct miscdevice *c;

	device_no = MINOR(inode->i_rdev);
	printk(KERN_ALERT"\nIn open, minor no = %d\n",device_no);
	
	list_for_each_entry(c, &hcsr_devp[0]->misc_dev.list, list) { 
		if(strncmp(c->name, DEVICE_1, 6)){
			printk(KERN_ALERT"HSCR 1 Opened");
			file->private_data = hcsr_devp[0];
			break;
		}
		else if(strncmp(c->name, DEVICE_2, 6)){
			printk(KERN_ALERT"HSCR 2 Opened");
			file->private_data = hcsr_devp[1];
			break;
		}
	}
	return 0;
}

static int hcsr_driver_close(struct inode *inode, struct file *file){
	
	free_GPIOs(file->private_data);
	return 0;

}

/// IRQ Handler
static irq_handler_t echo_handler(int irq, void *dev_id, struct pt_regs *regs)
{	
	static unsigned long time_rise, time_fall, time_diff;
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
		hcsr_devp->buffer[i] = (time_diff / (58*400));
		entry =0;
		irq_set_irq_type(irq,IRQF_TRIGGER_RISING);
	}
	
	i = (i+1)%5; 	
	return (irq_handler_t)IRQ_HANDLED;
}


static ssize_t hcsr_driver_write(struct file *file, const char *buf,size_t count, loff_t *ppos){
	struct hcsr_dev *hcsr_devp = file->private_data;
	int input, ret;
	struct task_struct* trigger
	unsigned int echo_irq =0;
	if(copy_from_user(&input, buf, sizeof(int)) != 0)
		return -EFAULT;
	// todo: remove later
	hcsr_devp->mode = 1;

	if(!hcsr_devp->mode){  //one shot mode

	}
	//continous mode 
	else{  
		if(!input){ // stop continuous triggering
			echo_irq = gpio_to_irq(GPIO_ECHO);
			free_irq(echo_irq, (void *)hcsr_devp);

		}
		else{  // start continuous triggering
			echo_irq = gpio_to_irq(GPIO_ECHO);   // associate irq to echo pin

			ret = request_irq(echo_irq, (irq_handler_t)echo_handler, IRQF_TRIGGER_RISING, "Echo_Dev", (void *)hcsr_devp);
			if(ret < 0)
				printk("Error requesting IRQ: %d\n", ret);
			
			kthread_run
		}
	}
	

	return 0;
}

static ssize_t hcsr_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos){
	int bytes_read = 0, i;
	struct hcsr_dev *hcsr_devp = file->private_data;
	/*
	if(copy_to_user(buf, &(hcsr_devp->buffer)[0] , sizeof(int)) != 0)
		return -EFAULT;
	*/

	for(i=0;i<5;i++){
		printk(KERN_ALERT "%d: %d \n", i, hcsr_devp->buffer[i]);
	}
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

	gpio_init(GPIO_ECHO, STRINGIFY(GPIO_ECHO), GPIO_INPUT, GPIO_LOW);
	gpio_init(GPIO_ECHO_LVL_SHFTR, STRINGIFY(GPIO_ECHO_LVL_SHFTR), GPIO_OUTPUT, GPIO_HIGH);
	gpio_init(GPIO_ECHO_PULL_UP, STRINGIFY(GPIO_ECHO_PULL_UP), GPIO_OUTPUT, GPIO_LOW);
	gpio_init(GPIO_ECHO_FMUX, STRINGIFY(GPIO_ECHO_FMUX), GPIO_OUTPUT, GPIO_LOW);

	gpio_init(GPIO_TRIGGER, STRINGIFY(GPIO_TRIGGER), GPIO_OUTPUT, GPIO_LOW);
	gpio_init(GPIO_TRIGGER_LVL_SHFTR, STRINGIFY(GPIO_TRIGGER_LVL_SHFTR), GPIO_OUTPUT, GPIO_LOW);
	gpio_init(GPIO_TRIGGER_PULL_UP, STRINGIFY(GPIO_TRIGGER_PULL_UP), GPIO_OUTPUT, GPIO_LOW);
	gpio_init(GPIO_TRIGGER_FMUX, STRINGIFY(GPIO_TRIGGER_FMUX), GPIO_OUTPUT, GPIO_LOW);

	


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