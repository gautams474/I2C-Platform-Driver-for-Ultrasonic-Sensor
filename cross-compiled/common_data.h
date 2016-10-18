#define HCSR_MAGIC_NO 'P'
#define SETMODE _IOW(HCSR_MAGIC_NO, 0, unsigned long)
#define SETPINS _IOW(HCSR_MAGIC_NO, 1, unsigned long)

#define MODE_ONE_SHOT 0
#define MODE_CONTINUOUS 1

#define STOP_CONT_TRGGR 0
#define START_CONT_TRIGGER 1

struct gpio_pair{
	int echo;
	int trigger;
};

struct mode_pair{
	int mode;
	int frequency;
};