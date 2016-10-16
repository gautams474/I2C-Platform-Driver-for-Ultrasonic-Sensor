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
#include <linux/jiffies.h>

#define DEVICE_1	"HCSR_1"
#define DEVICE_2	"HCSR_2"

static struct hcsr_dev{
	struct miscdevice misc_dev;
	int mode;
	int frequency;
	int buffer[5];
}*hcsr_devp[2];

struct timer_list my_timer;

static int hcsr_driver_open(struct inode *inode, struct file *file){
	int device_no = 0;
	struct miscdevice *c;

	device_no = MINOR(inode->i_rdev);
	printk(KERN_ALERT"\nIn open, minor no = %d\n",device_no);
	
	list_for_each_entry(c, &hcsr_devp[0]->misc_dev.list, list) { 
		if(strlcmp(c->name, DEVICE_1, 6)){
			printk(KERN_ALERT"HSCR 1 Opened");
			file->private_data = hcsr_devp[0];
			break;
		}
		else if(strlcmp(c->name, DEVICE_2, 6)){
			printk(KERN_ALERT"HSCR 2 Opened");
			file->private_data = hcsr_devp[1];
			break;
		}
	}
	return 0;
}

static int hcsr_driver_close(struct inode *inode, struct file *file){
	return 0;

}

struct timer_struct{
	struct timer_list my_timer;
	int mode;
	int time;
};

void Start_Usonic (unsigned long data){
	
	struct timer_struct *timer_data = (struct timer_struct)data;
	struct timer_list *timer = &timer_data->my_timer;
	
	gpio_set_value(trigger_pin, 1); //14
	udelay(10);
	gpio_set_value(trigger_pin, 0); //14
	
	if(mode){
		timer->expires = jiffies+timer_data->time;
		add_timer(timer);
	}
	return ;
}

void start_trigger(int mode, int frequency){
	struct timer_struct timer_data;
	timer_data.mode = mode;
	// calculate time from frequency, replace 15
	timer_data.time = mode*15;
	timer_data.my_timer = my_timer;
	// Initialize timer
	my_timer.function = Start_Usonic;
	my_timer.expires = jiffies + timer_data.time;
	my_timer.data = (unsigned long)&timer_data; // for restart
	init_timer(&my_timer);
	
	add_timer(&my_timer);
}

/// IRQ Handler

static irq_handler_t echo_handler(int irq, void *dev_id, struct pt_regs *regs)
{	
	static unsigned long time_rise, time_fall, time_diff;
	static char entry =0;
	static int i =0;
	
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
	int input;
	if(copy_from_user(&input, buf, sizeof(int)) != 0)
		return -EFAULT;

	if(!hcsr_devp->mode){  //one shot mode

	}
	else{  //continous mode 
		if(!input){ // stop continuous triggering
			del_timer(&my_timer);
		}
		else{  // start continuous triggering
			start_trigger(hcsr_devp->mode,hcsr_devp->frequency);
		}
	}
	

	return 0;
}

static ssize_t hcsr_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos){
	int bytes_read = 0;
	struct hcsr_dev *hcsr_devp = file->private_data;
	
	if(copy_to_user(buf, &(hcsr_devp->buffer)[0] , sizeof(int)*5) != 0)
		return -EFAULT;
	
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
  	kfree(hcsr_devp);
  	return ret;
  }
  
  ret = misc_register(&hcsr_dev2);
  if (ret){
  	printk(KERN_ERR"Unable to register misc device 2\n");
  	return ret;
  }
  	  hcsr_devp[0]->misc_dev = hcsr_dev1;
  	  hcsr_devp[1]->misc_dev = hcsr_dev2;
	hcsr_devp[0]->minor = hcsr_dev1.minor;
	hcsr_devp[1]->minor = hcsr_dev2.minor;
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






