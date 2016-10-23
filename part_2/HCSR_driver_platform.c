#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
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

#define DEBUG 1

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
#define BUFFER_EMPTY(x) (x->size == 0 ? 1 : 0)

#define STOP_CONT_TRGGR 0
#define START_CONT_TRIGGER 1   

#define IS_MODE_CONTINUOUS(x) (x->dev_mode_pair.mode == MODE_CONTINUOUS ? 1 : 0)
#define IS_MODE_ONE_SHOT(x) (x->dev_mode_pair.mode == MODE_ONE_SHOT ? 1 : 0)
#define IS_STOPPED(x) (x->trigger_task_struct == NULL ? 1 : 0)

#define FREQ_TO_TIME(x) 1000/x

struct hcsr_dev{
	struct miscdevice misc_dev;
	struct platform_device platform_dev;

	struct gpio_pair dev_gpio_pair;
	struct mode_pair dev_mode_pair;

	long buffer[5];				// buffer and its head, tail size vars
	int head;
	int tail;
	int size;
	
	struct semaphore buffer_signal;		// GPIO irq handler signals non empty buffer and read func waits on it if buffer empty
	
	struct task_struct* trigger_task_struct; // kernel thread used to trigger in coninuous mode and check state
} *hcsr_any_devp;


void gpio_init(int pin, char* name, int direction, int value){
	int ret;
	ret = gpio_request(pin, name);
	if(ret)
		printk("%s: GPIO pin number %d could not be requested.\n",__FUNCTION__, pin);

	if(direction == GPIO_INPUT){
		ret = gpio_direction_input(pin);
		if(ret)
			printk("%s: GPIO pin number %d could not be set as input.\n",__FUNCTION__, pin);
	}
	else if(direction == GPIO_OUTPUT){
		ret = gpio_direction_output(pin, value);
		if(ret)
			printk("%s: GPIO pin number %d could not be set as output.\n",__FUNCTION__, pin);
		
		// Direction output didn't seem to init correctly.		
		gpio_set_value_cansleep(pin, value); 
	}
}

/*
*Free resources
*/

void free_GPIOs(struct hcsr_dev* hcsr_devp){

	int echo = hcsr_devp->dev_gpio_pair.echo;
	int trigger = hcsr_devp->dev_gpio_pair.trigger;
	unsigned int echo_irq =0;

	if(hcsr_devp->trigger_task_struct != NULL && IS_MODE_CONTINUOUS(hcsr_devp)){
		kthread_stop(hcsr_devp->trigger_task_struct);
		hcsr_devp->trigger_task_struct = NULL;
	}

	if(echo != -1){
		echo_irq = gpio_to_irq(echo);
		free_irq(echo_irq, (void *)hcsr_devp);

		gpio_free(echo);
		hcsr_devp->dev_gpio_pair.echo = -1;
	}
	if(trigger != -1){
		gpio_free(trigger);
		hcsr_devp->dev_gpio_pair.trigger = -1;
	}
}

inline void platform_system_abort(struct hcsr_dev* devp){
		up(&(devp->buffer_signal));						// signal pending reads
		free_GPIOs(devp);
		kfree(devp->misc_dev.name);
}

/**********************************************************************/


/// IRQ Handler
static unsigned long time_rise, time_fall, time_diff;
static irq_handler_t echo_handler(int irq, void *dev_id, struct pt_regs *regs){
	
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
		hcsr_devp->head = ((hcsr_devp->head) + 1) % 5;
		if(hcsr_devp->head == hcsr_devp->tail)		// to ensure FIFO behavior
			hcsr_devp->tail = (hcsr_devp->tail+1)%5;
		hcsr_devp->size = (hcsr_devp->size +1) < 6 ? hcsr_devp->size + 1 : 5;

		irq_set_irq_type(irq,IRQF_TRIGGER_RISING);
		up(&(hcsr_devp->buffer_signal));
	}

	toggle_entry ^= 1;
	
	return (irq_handler_t)IRQ_HANDLED;
}

static int hcsr_driver_open(struct inode *inode, struct file *file){
	int device_no = 0;
	struct miscdevice *c;
	struct hcsr_dev* hcsr_devp;

	device_no = MINOR(inode->i_rdev);
	printk(KERN_ALERT"\nIn open, minor no = %d\n",device_no);

	hcsr_devp = hcsr_any_devp;

	//find minor number using the list
	list_for_each_entry(c, &hcsr_devp->misc_dev.list, list) { 
		if(c->minor == device_no){
			printk(KERN_ALERT"Device: %s Opened\n", c->name);

			if(!strncmp(c->name, DEVICE_1, 6)){
				printk(KERN_ALERT"HSCR 1 Opened\n");
				file->private_data = container_of(c, struct hcsr_dev, misc_dev);  //hcsr_devp[0];

				break;
			}
			else if(!strncmp(c->name, DEVICE_2, 6)){
				printk(KERN_ALERT"HSCR 2 Opened\n");
				file->private_data = container_of(c, struct hcsr_dev, misc_dev);  //hcsr_devp[0];
				break;
			}
			else{
				printk(KERN_ERR"FILE OPEN: NAME NOT FOUND\n");
				return -EFAULT;
			}
		}
	}
	return 0;
}

static int hcsr_driver_close(struct inode *inode, struct file *file){
	//free_GPIOs(file->private_data);
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
		printk("\t\tsleep time: %d\n", time);
		while(!kthread_should_stop()){
			trigger_HCSR(hcsr_devp);
			msleep(time);
			#if DEBUG
				printk("\t\t%s: %s triggered !!\n",__FUNCTION__, hcsr_devp->misc_dev.name);
			#endif
		}
	}
	else if(mode == MODE_ONE_SHOT){
		trigger_HCSR(hcsr_devp);
		msleep(60);  						// minimum sleep required between two triggers as mentioned in datasheet		
	}
	else
		printk(KERN_ERR"%s: %d Incorrect Mode\n", __FUNCTION__, hcsr_devp->dev_mode_pair.mode);

	hcsr_devp->trigger_task_struct = NULL;

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
	
	if(buf == NULL){
		printk("%s: buf NULL\n",__FUNCTION__);
		return -EFAULT;
	}
	
	if(copy_from_user(&input, buf, sizeof(int)) != 0)
		return -EFAULT;

	if(hcsr_devp->dev_mode_pair.mode == MODE_ONE_SHOT){  //one shot mode
		hcsr_devp->size = 0;
		hcsr_devp->tail = 0;
		hcsr_devp->head = 0;
		if(input)  // clear buffer for non zero input
			for(i = 0; i< 5; i++)
				hcsr_devp->buffer[i] = -1;

		#if DEBUG
			printk("%s: buffer cleared\n",__FUNCTION__);
		#endif
		
		if(hcsr_devp->trigger_task_struct == NULL)  // if not triggered, start triggering
			ret = start_triggers(hcsr_devp);
		else
			printk(KERN_ERR "%s: trigger_task_struct is not null\n",__FUNCTION__);
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

	if(BUFFER_EMPTY(hcsr_devp) && IS_STOPPED(hcsr_devp) && IS_MODE_ONE_SHOT(hcsr_devp)){
			trigger_HCSR(hcsr_devp);
			#if DEBUG
				printk(KERN_ALERT "%s: triggering in one shot mode\n",__FUNCTION__);
			#endif
		}
	// Avoids sleeping indefinately when continuous mode is not on
	if(BUFFER_EMPTY(hcsr_devp) && IS_MODE_CONTINUOUS(hcsr_devp)){
		#if DEBUG
		printk(KERN_ERR "%s: Buffer empty and mode is continuous.\n", __FUNCTION__);
		#endif
		return -EINPROGRESS;
	}

	//sleep while buffer is empty
	#if DEBUG
		printk("size: %d before sleeping\n", hcsr_devp->size);
	#endif

	if(down_interruptible(&(hcsr_devp->buffer_signal))){
		printk(KERN_ALERT "%s: semaphore interrupted\n",__FUNCTION__);
		platform_system_abort(hcsr_devp);
		return -EFAULT;// semaphore interrupted
	}

	val = hcsr_devp->buffer[hcsr_devp->tail];
	hcsr_devp->tail = (hcsr_devp->tail+1)%5;
	hcsr_devp->size = (hcsr_devp->size -1) > 0 ? hcsr_devp->size - 1 : 0;

	#if DEBUG
	printk(KERN_ALERT "val: %ld time_diff: %ld\n\n", val, time_diff/(58l*400l));
	#endif

	
	if(copy_to_user(buf, &val, sizeof(long)) != 0)
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

			// printk("mode : %d , frequency : %d \n",hcsr_devp->dev_mode_pair.mode, hcsr_devp->dev_mode_pair.frequency);
			if(hcsr_devp->dev_mode_pair.mode != MODE_CONTINUOUS && hcsr_devp->dev_mode_pair.mode != MODE_ONE_SHOT){
				printk("%s: wrong mode %d\n",__FUNCTION__, hcsr_devp->dev_mode_pair.mode);
				
				return -EFAULT;
			}

			if(hcsr_devp->dev_mode_pair.mode == MODE_CONTINUOUS){ 
				if(hcsr_devp->dev_mode_pair.frequency > 16 || hcsr_devp->dev_mode_pair.frequency < 1){
					printk("%s: wrong freq %d . Enter freq between 1 to 16 Hz.\n",__FUNCTION__, hcsr_devp->dev_mode_pair.frequency);
					return -EFAULT;
				}
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
			
			gpio_init(hcsr_devp->dev_gpio_pair.echo, STRINGIFY(GPIO_ECHO), GPIO_INPUT, -1);
			gpio_init(hcsr_devp->dev_gpio_pair.trigger, STRINGIFY(GPIO_TRIGGER), GPIO_OUTPUT, GPIO_LOW);

			echo_irq = gpio_to_irq(hcsr_devp->dev_gpio_pair.echo);
			ret = request_irq(echo_irq, (irq_handler_t)echo_handler, IRQF_TRIGGER_RISING, "Echo_Dev", hcsr_devp);
			if(ret < 0){
				printk("Error requesting IRQ: %d\n", ret);
				return -EFAULT;
			}
			
			printk(KERN_ALERT"%s: echo: %d  echo_irq: %d trigger: %d   for device %s\n",
								__FUNCTION__, hcsr_devp->dev_gpio_pair.echo, echo_irq, hcsr_devp->dev_gpio_pair.trigger, hcsr_devp->misc_dev.name);
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

/*
*Platform function implementations
**/
static int platform_hcsr_probe(struct platform_device* pdev){
	static int i; 			// static i is always initialized to 0 hence no initialization
	char* misc_dev_name;
	int ret;
	struct hcsr_dev* hcsr_devp;

	hcsr_devp = platform_get_drvdata(pdev);  // container_of(pdev, struct hcsr_dev, platform_dev);
	i = pdev->id;
	if(i>9)
		return -ENOMEM;

	/*TO DO: Free this appropriately*/
	misc_dev_name = kmalloc(sizeof(char) * (strlen("HCSR_") + 2) , GFP_KERNEL);   // supports from HCSR_0 to HCSR_9
	
	hcsr_devp->misc_dev.minor = MISC_DYNAMIC_MINOR;
	sprintf(misc_dev_name, "HCSR_%d", i);
	hcsr_devp->misc_dev.name = misc_dev_name;
	hcsr_devp->misc_dev.fops = &hcsr_fops;
	
	hcsr_devp->head = 0;
	hcsr_devp->tail = 0;
	hcsr_devp->size = 0;

	hcsr_devp->dev_gpio_pair.echo = -1;
	hcsr_devp->dev_gpio_pair.trigger = -1;

	hcsr_devp->dev_mode_pair.frequency = 16;

	hcsr_devp->trigger_task_struct = NULL;

	sema_init(&(hcsr_devp->buffer_signal),0);

	if(i == 0)
		hcsr_any_devp = hcsr_devp;

	ret = misc_register(&(hcsr_devp->misc_dev));
	if(ret < 0){
		printk("%d %s: misc dev register failed\n",__LINE__, __FUNCTION__);
		kfree(hcsr_devp->misc_dev.name);
		return -ECANCELED;
	}
	i++;
	return 0;
}

static int platform_hcsr_remove(struct platform_device *pdev){
	struct hcsr_dev* hcsr_devp = platform_get_drvdata(pdev);  // container_of(pdev, struct hcsr_dev, platform_dev);
	platform_system_abort(hcsr_devp);
	misc_deregister(&(hcsr_devp->misc_dev));
	return 0;
}

static struct platform_driver platform_HCSR04 = {
	.probe = platform_hcsr_probe,
	.remove = platform_hcsr_remove,
	.driver = {
		.name = "platform_HCSR04",
		.owner = THIS_MODULE,
	},
};

static int __init platform_hcsr_driver_init(void){
	platform_driver_register(&platform_HCSR04);
	return 0;
}

/* Driver Exit */
void __exit platform_hcsr_driver_exit(void){
	platform_driver_unregister(&platform_HCSR04);
	printk("hcsr platform driver removed.\n");
}

module_init(platform_hcsr_driver_init);
module_exit(platform_hcsr_driver_exit);
MODULE_LICENSE("GPL v2");