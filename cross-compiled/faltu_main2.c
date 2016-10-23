#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

#include "common_data.h"

void gpio_inits(void);
void gpio_unexport(void);

int main(){
	int fd_1 = 0, ret = 0, res = 0;
	int i = 10, input = 0;
	long output;

	gpio_inits();

	fd_1 = open("/dev/HCSR_1", O_RDWR);
	if (fd_1< 0 ){
		printf("Can not open device file.\n");		
		return 0;
	}

	struct gpio_pair dev1_gpio;
	struct mode_pair dev1_mode;

	dev1_gpio.echo = DEV_1_GPIO_ECHO;
	dev1_gpio.trigger = DEV_1_GPIO_TRIGGER;

	res = ioctl(fd_1, SETPINS, (unsigned long)&dev1_gpio);
	if(res < 0){
		perror("IOCTL Error: ");
		return 0;
	}

	dev1_mode.mode = MODE_ONE_SHOT;
	dev1_mode.frequency = 16;

	res = ioctl(fd_1,SETMODE,&dev1_mode);
	if(res < 0){
		perror("IOCTL Error: ");
		return 0;
	}
	
	// write input = 1 start periodic sampling
	
	i = 10;
	while(i > 0){
		ret = write(fd_1,&input, sizeof(input));
		if(ret < 0){
			perror("Write Error: ");
			printf("\n");
			fflush(stdout);
		}
		sleep(1);
		i--;
		printf("read number: %d\n",i);

		ret = read(fd_1,&output,sizeof(output));
		if(ret < 0){
			perror("Error: ");
			continue;
		}

		//display
		printf("Sensor 1 Distance = %ld \n",output);
		fflush(stdout);
		usleep(100*1000);
	}


	printf("user space sleeping\n");
	if(sleep(2) < 0)
		printf("%s: could not sleep\n", __FUNCTION__);

	printf("user space reading\n");
	fflush(stdout);
	/*
	i = 10;
	while(i > 0){
		ret = read(fd_1,&output,sizeof(output));
		if(ret < 0){
			perror("Error: ");
			continue;
		}

		//display
		printf("Sensor 1 Distance = %ld \n",output);
		fflush(stdout);
		usleep(100);
		i--;
	}
	*/
	//printf("dev 1 cont mode stopped\n");
	//fflush(stdout);
	input = 1; // device 1 continuous mode stopped
	ret = write(fd_1,&input, sizeof(input));
	if(ret < 0){
		perror("Write Error: ");
	}


	for(i =0; i < 6; i++){
		printf( "Reading:  device 1 should return fault on 6th reading \n");
		ret = read(fd_1,&output,sizeof(output));
		if(ret < 0){
			perror("Error: ");
			continue;
		}

		//display
		printf("Sensor 1 Distance = %ld \n",output);
		fflush(stdout);
		sleep(1);
	}

	printf("closing\n");
	fflush(stdout);

	close(fd_1);

	gpio_unexport();
	return 0;
}


void each_gpio_unexport(const int fd_export, const char* pin){
	int ret;

	ret = write(fd_export, pin, strlen(pin));
	if(ret<0)
		printf("%s: unexport %s failed\n", __FUNCTION__, pin);
	
}

void gpio_unexport(void){
	int fd;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if(fd < 0)
		printf("export failed\n");
	
	each_gpio_unexport(fd, "34");
	each_gpio_unexport(fd, "35");
	each_gpio_unexport(fd, "77");

	each_gpio_unexport(fd, "16");
	each_gpio_unexport(fd, "17");
	each_gpio_unexport(fd, "76");

	each_gpio_unexport(fd, "18");
	each_gpio_unexport(fd, "19");
	each_gpio_unexport(fd, "66");

	each_gpio_unexport(fd, "36");
	each_gpio_unexport(fd, "37");

	close(fd);
}

void each_gpio_init(const int fd_export,const char* baseAddress,const char* pin,const char* direction,const char* value){
	
	int fd_direction;
	int fd_value;
	char* string;

	int ret;

	//export pin
	ret = write(fd_export, pin, strlen(pin));
	if(ret<0)
		printf("%s: request %s failed\n", __FUNCTION__, pin);
	
	// Set GPIO Direction
	if(direction != NULL){
		string = malloc(sizeof(char) * (strlen(baseAddress) + strlen("gpio/") + strlen(pin) + strlen("direction") + 1/*for null character*/));
		sprintf(string , "%sgpio%s/direction",baseAddress,pin);
		fd_direction = open(string, O_WRONLY);
		if(fd_direction < 0)
			printf("%s: Could not open gpio: %s direction file\n", __FUNCTION__,pin);
		ret = write(fd_direction, direction, strlen(direction));
		if(ret < 0)
			printf("%s: Could not set gpio: %s value\n", __FUNCTION__, pin);
		close(fd_direction);
		free(string);
		string = NULL;
	}

	// Set GPIO Value
	if(value != NULL){
		string = malloc(sizeof(char) * (strlen(baseAddress) + strlen("gpio/") + strlen(pin) + strlen("value") + 1/*for null character*/));
		sprintf(string , "%sgpio%s/value",baseAddress,pin);
		fd_value = open(string, O_WRONLY);
		if(fd_value < 0)
			printf("%s: Could not open gpio: %s value file\n", __FUNCTION__,pin);
		ret = write(fd_value, value, strlen(value));
		if(ret < 0)
			printf("%s: Could not set gpio: %s value\n", __FUNCTION__, pin);
		close(fd_value);
		free(string);
		string = NULL;
	}
}

void gpio_inits(void){
	int fd;

	const char* baseAddress = "/sys/class/gpio/";

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if(fd < 0)
		printf("export failed\n");
	
	each_gpio_init(fd, baseAddress, "34", "out", "1");
	each_gpio_init(fd, baseAddress, "35", "out", "0");
	each_gpio_init(fd, baseAddress, "77", NULL, NULL);

	each_gpio_init(fd, baseAddress, "16", "out", "0");
	each_gpio_init(fd, baseAddress, "17", "out", "0");
	each_gpio_init(fd, baseAddress, "76", NULL, NULL);

	each_gpio_init(fd, baseAddress, "18", "out", "1");
	each_gpio_init(fd, baseAddress, "19", "out", "0");
	each_gpio_init(fd, baseAddress, "66", NULL, NULL);

	each_gpio_init(fd, baseAddress, "36", "out", "0");
	each_gpio_init(fd, baseAddress, "37", "out", "0");

	close(fd);
}