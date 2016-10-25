#define HCSR_MAGIC_NO 'P'
#define SETMODE _IOW(HCSR_MAGIC_NO, 0, unsigned long)
#define SETPINS _IOW(HCSR_MAGIC_NO, 1, unsigned long)

#define MODE_ONE_SHOT 0
#define MODE_CONTINUOUS 1

#define STOP_CONT_TRGGR 0
#define START_CONT_TRIGGER 1

#define DEV_1_GPIO_ECHO 13			//GPIO 13 == IO2
//#define GPIO_ECHO 13
#define DEV_1_GPIO_TRIGGER 14		//GPIO 14 == IO3
//#define GPIO_TRIGGER 14

#define DEV_2_GPIO_ECHO 12			//GPIO 12 == IO1
#define DEV_2_GPIO_TRIGGER 6		//GPIO 6 == IO4

struct gpio_pair{
	int echo;
	int trigger;
};

struct mode_pair{
	int mode;
	int frequency;
};