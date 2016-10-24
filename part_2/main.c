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
	int fd_1 = 0, fd_2 = 0, ret = 0;
	int i = 10, input = 1;
	long output;

	gpio_inits();

	fd_1 = open("/dev/HCSR_1", O_RDWR);
	if (fd_1< 0 ){
		printf("Can not open device file 1.\n");		
		return 0;
	}

	printf("**********************************************************\n");
	printf("Starting with HCSR_1\n");
	printf("**********************************************************\n");

	printf("CORNER CASE:\n");
	printf("Reading from HCSR_1 when sampling is not started. It should fail and not block\n");
	ret = read(fd_1,&output,sizeof(output));
	if(ret < 0){
		perror("Error: ");
	}

	printf("HCSR_1 starting periodic sampling \n");
	// write input = 1 start periodic sampling for device 1
	ret = write(fd_1, &input, sizeof(input));
	if(ret < 0){
		perror("Write Error: ");
		printf("\n");
		fflush(stdout);
	}
	
	printf("Reading from HCSR_1 25 times\n");
	// read device 1 value 100 times, may sleep if buffer is empty
	i = 25;
	while(i > 0){
		ret = read(fd_1,&output,sizeof(output));
		if(ret < 0){
			perror("Error: ");
		}

		//display
		printf("Sensor 1 Distance = %ld \n",output);
		fflush(stdout);
		usleep(100*1000);
		i--;
	}

	printf("Stopping HCSR_1\n");
	// device 1 continuous mode stopped
	input = 0; 
	ret = write(fd_1,&input, sizeof(input));
	if(ret < 0){
		perror("Write Error: ");
	}

	printf("Reading last 6 values from HCSR_1");
	printf("this may cause the last value to return with error\n");
	for(i =0; i < 6; i++){
	// device 1 should return fault on 6th reading as continuos mode stopped, buffer size = 5
		output = -1;
		ret = read(fd_1,&output,sizeof(output));
		if(ret < 0){
			perror("Error: ");
			break;
		}

		//display
		printf("Sensor 1 Distance = %ld \n",output);
		fflush(stdout);
	}

	printf("User thread sleeping sleeping\n");
	sleep(2);

	printf("\n\n**********************************************************\n");
	printf("Starting with HCSR_2\n");
	printf("**********************************************************\n");

	fd_2 = open("/dev/HCSR_2", O_RDWR);
	if (fd_2 < 0 ){
		printf("Can not open device file 2.\n");		
		return 0;
	}

	printf("HCSR_2 triggering one short mode, buffer cleared\n");
	// device 2 in one shot mode buffer not cleared, trigger measurement
	input = 1; 
	ret = write(fd_2,&input, sizeof(input));
	if(ret < 0){
		perror("Write Error: ");
		printf("\n");
		fflush(stdout);
	}

	// printf("Reading from HCSR_2 which was triggered earlier\n");
	// // read device 2 value, triggered earlier
	// ret = read(fd_2,&output,sizeof(output));
	// if(ret < 0){
	// 	perror("Error: ");
	// 	printf("\n");
	// 	fflush(stdout);
	// }

	// //display device 2 value
	// printf("\nSensor 2 Distance = %ld\n",output);
	// fflush(stdout);

	for(i=0; i< 25; i++){
	//device 2 read shows previous value triggered by write, read  also triggers another one shot measurement
		ret = read(fd_2,&output,sizeof(output));
		if(ret < 0){
			perror("Error: ");
		}

		//display
		printf("Sensor 2 Distance = %ld \n",output);
		fflush(stdout);
	}
	printf("closing\n");
	fflush(stdout);

	close(fd_1);
	close(fd_2);

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

