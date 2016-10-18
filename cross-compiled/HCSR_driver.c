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
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <asm/msr.h>
#include "common_data.h"

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

#define STRINGIFY(x) #x
#define BUFFER_EMPTY(x) (x->size == 0 ? 1 : 0)

#define STOP_CONT_TRGGR 0
#define START_CONT_TRIGGER 1   

#define IS_MODE_CONTINUOUS(x) (x->dev_mode_pair.mode == MODE_CONTINUOUS ? 1 : 0)
#define IS_MODE_ONE_SHOT(x) (x->dev_mode_pair.mode == MODE_ONE_SHOT ? 1 : 0)
#define IS_STOPPED(x) (x->trigger_task_struct == NULL ? 1 : 0)

#define FREQ_TO_TIME(x) 1000/x

static struct hcsr_dev{
	struct miscdevice misc_dev;

	struct gpio_pair dev_gpio_pair;
	struct mode_pair dev_mode_pair;

	long buffer[5];				// buffer and its head, tail size vars
	int head;
	int tail;
	int size;
	
	struct semaphore buffer_signal;		// GPIO irq handler signals non empty buffer and read func waits on it if buffer empty
	
	struct task_struct* trigger_task_struct; // kernel thread used to trigger in coninuous mode and check state
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

void free_GPIOs(struct hcsr_dev* hcsr_devp){

	int echo = hcsr_devp->dev_gpio_pair.echo;
	int trigger = hcsr_devp->dev_gpio_pair.trigger;
	unsigned int echo_irq =0;

	if(echo != -1){
		echo_irq = gpio_to_irq(echo);
		free_irq(echo_irq, (void *)hcsr_devp);

		gpio_free(echo);
	}
	if(trigger != -1)
		gpio_free(trigger);
}

/// IRQ Handler
static irq_handler_t echo_handler(int irq, void *dev_id, struct pt_regs *regs){
	static unsigned long time_rise, time_fall, time_diff;
	static char toggle_entry =0;

	struct hcsr_dev *hcsr_devp = (struct hcsr_dev *) dev_id;
	
 	if(toggle_entry == 0){
	 	rdtscl(time_rise);
		irq_set_irq_type(irq,IRQF_TRIGGER_FALLING);
	}
	else{
		rdtscl(time_fall);
		time_diff = time_fall - time_rise;
		

		hcsr_devp->buffer[hcsr_devp->head] = time_diff/(58*400);
		hcsr_devp->head = (hcsr_devp->head+1)%5;
		if(hcsr_devp->head == hcsr_devp->tail)		// to ensure FIFO behavior
			hcsr_devp->tail = (hcsr_devp->tail+1)%5;
		hcsr_devp->size = (hcsr_devp->size +1) < 5 ? hcsr_devp->size + 1 : 5;

		irq_set_irq_type(irq,IRQF_TRIGGER_RISING);
		up(&(hcsr_devp->buffer_signal));
	}

	toggle_entry ^= 1;
	

	return (irq_handler_t)IRQ_HANDLED;
}

static int hcsr_driver_open(struct inode *inode, struct file *file){
	int device_no = 0, ret = 0;
	struct miscdevice *c;
	unsigned int echo_irq =0;

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
				printk(KERN_ERR"FILE OPEN: NAME NOT FOUND\n");
				return -1;
			}
		}
	}

	// echo_irq = gpio_to_irq(GPIO_ECHO);   // associate irq to echo pin

	// ret = request_irq(echo_irq, (irq_handler_t)echo_handler, IRQF_TRIGGER_RISING, "Echo_Dev", hcsr_devp[0]);
	// if(ret < 0){
	// 	printk("Error requesting IRQ: %d\n", ret);
	// }

	// /*TO DO remove this after user space is implemented*/
	// gpio_init(GPIO_ECHO_FMUX, STRINGIFY(GPIO_ECHO_FMUX), 2, GPIO_LOW);
	// gpio_init(GPIO_ECHO_PULL_UP, STRINGIFY(GPIO_ECHO_PULL_UP), GPIO_OUTPUT, GPIO_LOW);
	// gpio_init(GPIO_ECHO_LVL_SHFTR, STRINGIFY(GPIO_ECHO_LVL_SHFTR), GPIO_OUTPUT, GPIO_HIGH);
	// gpio_init(GPIO_ECHO, STRINGIFY(GPIO_ECHO), GPIO_INPUT, GPIO_LOW); // TO DO remove
	
	// gpio_init(GPIO_TRIGGER_FMUX, STRINGIFY(GPIO_TRIGGER_FMUX), 2, GPIO_LOW);
	// gpio_init(GPIO_TRIGGER_PULL_UP, STRINGIFY(GPIO_TRIGGER_PULL_UP), GPIO_OUTPUT, GPIO_LOW);
	// gpio_init(GPIO_TRIGGER_LVL_SHFTR, STRINGIFY(GPIO_TRIGGER_LVL_SHFTR), GPIO_OUTPUT, GPIO_LOW);
	// gpio_init(GPIO_TRIGGER, STRINGIFY(GPIO_TRIGGER), GPIO_OUTPUT, GPIO_LOW); // TO DO remove
	
	return 0;
}

static int hcsr_driver_close(struct inode *inode, struct file *file){
	free_GPIOs(file->private_data);
	return 0;
}

inline void trigger_HCSR(struct hcsr_dev* hcsr_devp){
	gpio_set_value(hcsr_devp->dev_gpio_pair.trigger, 1); //14
	udelay(10);
	gpio_set_value(hcsr_devp->dev_gpio_pair.trigger, 0); //14
}

int trigger_func(void* data){
	struct hcsr_dev* hcsr_devp = (struct hcsr_dev *)data;
	int mode = hcsr_devp->dev_mode_pair.mode;
	int freq = hcsr_devp->dev_mode_pair.frequency, time;

	if(mode == MODE_CONTINUOUS){
		time = FREQ_TO_TIME(freq);
		while(!kthread_should_stop()){
			trigger_HCSR(hcsr_devp);
			msleep(time);
		}
	}
	else if(mode == MODE_ONE_SHOT){
		trigger_HCSR();
		hcsr_devp->trigger_task_struct = NULL;
		msleep(60);  						// minimum sleep required between two triggers as mentioned in datasheet		
	}
	else
		printk(KERN_ERR"%s: %d Incorrect Mode", __FUNCTION__, hcsr_devp->dev_mode_pair.mode);

	return 0;
}

int start_triggers(struct hcsr_dev *hcsr_devp){
	//int mode = hcsr_devp->mode;
	hcsr_devp->trigger_task_struct = kthread_run(trigger_func, hcsr_devp, "%s-trigger_func",hcsr_devp->misc_dev.name);
	if(IS_ERR(hcsr_devp->trigger_task_struct)){
		printk("WRITE: Could not start Kthread\n");
		return PTR_ERR(hcsr_devp->trigger_task_struct);
	}
	return 0;
}

static ssize_t hcsr_driver_write(struct file *file, const char *buf,size_t count, loff_t *ppos){
	struct hcsr_dev *hcsr_devp = file->private_data;
	int input, ret = 0, i;
	unsigned long time;
	
	if(buf == NULL)
		printk("buf NULL\n");
	
	if(copy_from_user(&input, buf, sizeof(int)) != 0)
		return -EFAULT;

	if(hcsr_devp->dev_mode_pair.mode == MODE_ONE_SHOT){  //one shot mode
		if(hcsr_devp->trigger_task_struct != NULL){  // if not triggered, start triggering
			ret = start_triggers(hcsr_devp);
		}
		else
			printk(KERN_ERR "%s: trigger_task_struct is null\n"__FUNCTION__);

		if(input){  // clear buffer for non zero input
			for(i = 0; i< 5; i++){
				hcsr_devp->buffer[i] = -1;
				hcsr_devp->size = 0;
				hcsr_devp->tail = 0;
				hcsr_devp->head = 0;
			}
		}
	}
	else if(hcsr_devp->dev_mode_pair.mode == MODE_CONTINUOUS){  //continous mode 
		if(input == STOP_CONT_TRGGR){ // stop continuous triggering
			if(hcsr_devp->trigger_task_struct != NULL){
				ret = kthread_stop(hcsr_devp->trigger_task_struct);
				if(ret != -EINTR)
					printk("triggering stopped\n");
			}			
		}
		else if(input == START_CONT_TRIGGER){  // start continuous triggering			
			printk("before start trigger %lu\n",rdtscl(time));
			ret = start_triggers(hcsr_devp);
		}
	}
	
	return ret;
}

static ssize_t hcsr_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos){
	
	struct hcsr_dev *hcsr_devp = file->private_data;
	long val;

	if(BUFFER_EMPTY(hcsr_devp) && IS_STOPPED(hcsr_devp) && IS_MODE_ONE_SHOT(hcsr_devp))
			trigger_HCSR();

		// to avoid against sleeping indefinately 
	if(BUFFER_EMPTY(hcsr_devp) && IS_MODE_CONTINUOUS(hcsr_devp)){
		printk(KERN_ERR"%S: Buffer empty and mode is continuous.", __FUNCTION__);
		return -EFAULT;
	}

	//sleep while buffer is empty
	if(down_interruptible(&(hcsr_devp->buffer_signal))){
		printk(KERN_ALERT "%s: semaphore interrupted\n",__FUNCTION__);
		return -EFAULT;// semaphore interrupted
	}

	val = hcsr_devp->buffer[hcsr_devp->tail];
	hcsr_devp->tail = (hcsr_devp->tail+1)%5;
	hcsr_devp->size = (hcsr_devp->size -1) > 0 ? hcsr_devp->size - 1 : 0;

	printk(KERN_ALERT "val: %lu \n\n", val);
	
	if(copy_to_user(buf, &val, sizeof(long) != 0))
		return -EFAULT;

	//returning bytes read
	return sizeof(long);

}

static long HCSR_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
	struct hcsr_dev* hcsr_devp = file->private_data;
	int ret;
	unsigned int echo_irq =0;

	switch (cmd){
		case SETMODE:
			
			if(copy_from_user(&(hcsr_devp->dev_mode_pair), (struct mode_pair*)arg, sizeof(struct mode_pair)) != 0)
				return -EFAULT;
			if(hcsr_devp->dev_mode_pair.mode != MODE_CONTINUOUS || hcsr_devp->dev_mode_pair.mode != MODE_ONE_SHOT){
				printk("%s: wrong mode\n",__FUNCTION__);
				return -EFAULT;
			}
			if(hcsr_devp->dev_mode_pair.frequency > 16 || hcsr_devp->dev_mode_pair.frequency < 1){
				printk("%s: wrong freq %d . Enter freq between 1 to 16 Hz.\n",__FUNCTION__, hcsr_devp->dev_mode_pair.frequency);
				return -EFAULT;
			}
			return 0;

			/*TO DO: Check SETPINS PROPERLY*/
		 case SETPINS:
		 	if(hcsr_devp->dev_gpio_pair.echo != -1){
				echo_irq = gpio_to_irq(hcsr_devp->dev_gpio_pair.echo);
		 		free_irq(echo_irq, (void *)hcsr_devp);
				gpio_free(hcsr_devp->dev_gpio_pair.echo);
		 	}
			if(hcsr_devp->dev_gpio_pair.trigger != -1)
				gpio_free(hcsr_devp->dev_gpio_pair.trigger);

		 	if(copy_from_user(&(hcsr_devp->dev_gpio_pair), (struct gpio_pair*)arg, sizeof(struct gpio_pair)) != 0)
		 		return -EFAULT;
			if(hcsr_devp->dev_gpio_pair.echo < 0 || hcsr_devp->dev_gpio_pair.trigger < 0)
				return -EFAULT;
			
			gpio_init(hcsr_devp->dev_gpio_pair.echo, STRINGIFY(GPIO_ECHO), GPIO_INPUT, GPIO_LOW);
			gpio_init(hcsr_devp->dev_gpio_pair.trigger, STRINGIFY(GPIO_TRIGGER), GPIO_OUTPUT, GPIO_LOW);

			ret = request_irq(hcsr_devp->dev_gpio_pair.echo, (irq_handler_t)echo_handler, IRQF_TRIGGER_RISING, "Echo_Dev", hcsr_devp);
			if(ret < 0){
				printk("Error requesting IRQ: %d\n", ret);
			}
			return 0;
		default:
			return -EFAULT;

	}
	return -ENOMSG;
}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations hcsr_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= hcsr_driver_open,        /* Open method */
    .release	= hcsr_driver_close,     /* Release method */
    .write		= hcsr_driver_write,       /* Write method */
    .read		= hcsr_driver_read,        /* Read method */
    .unlocked_ioctl = HCSR_ioctl,
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
	hcsr_devp[0]->head = 0;
	hcsr_devp[0]->tail = 0;
	hcsr_devp[0]->size = 0;

	hcsr_devp[1]->misc_dev = hcsr_dev2;
	hcsr_devp[1]->head = 0;
	hcsr_devp[1]->tail = 0;
	hcsr_devp[1]->size = 0;

	hcsr_devp[0]->dev_gpio_pair.echo = -1;
	hcsr_devp[0]->dev_gpio_pair.trigger = -1;

	hcsr_devp[1]->dev_gpio_pair.echo = -1;
	hcsr_devp[1]->dev_gpio_pair.trigger = -1;

	hcsr_devp[0]->dev_mode_pair.frequency = 16;
	hcsr_devp[1]->dev_mode_pair.frequency = 16;

	hcsr_devp[0]->trigger_task_struct = NULL;
	hcsr_devp[1]->trigger_task_struct = NULL;

	sema_init(&(hcsr_devp[0]->buffer_signal),1);
	sema_init(&(hcsr_devp[1]->buffer_signal),1);

	return 0; 
}

/* Driver Exit */
void __exit hcsr_driver_exit(void){

	/*TO DO Check status before stopping both drivers*/
	kthread_stop(hcsr_devp[0]->trigger_task_struct);

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
