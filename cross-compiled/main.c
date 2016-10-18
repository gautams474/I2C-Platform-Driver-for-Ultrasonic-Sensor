#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "common_data.h"

/*
void display(int buf[5]){
	int i;
	for(i=0;i<5;i++){
		printf("%d ",buf[i]);
	}
	printf("\n");
}
*/
void gpio_inits();

int main(){
	int fd_1 = 0, fd_2 = 0, ret = 0, res = 0;
	int i = 10, input = 1, output = -1; ;

	gpio_inits();

	fd_1 = open("/dev/HCSR_1", O_RDWR);
	if (fd_1< 0 ){
		printf("Can not open device file.\n");		
		return 0;
	}
	
	fd_2 = open("/dev/HCSR_2", O_RDWR);
	if (fd_2 < 0 ){
		printf("Can not open device file.\n");		
		return 0;
	}

	struct gpio_pair dev1_gpio, dev2_gpio;
	struct mode_pair dev1_mode, dev2_mode;

	dev1_gpio.echo = 2;
	dev1_gpio.trigger = 3;

	dev2_gpio.echo = 5;
	dev2_gpio.trigger = 4;

	res = ioctl(fd_1, SETPINS, (unsigned long)&dev1_gpio);
	if(res < 0){
		perror("IOCTL Error: ");
		return 0;
	}

	res = ioctl(fd_2, SETPINS, (unsigned long)&dev2_gpio);
	if(res < 0){
		perror("IOCTL Error: ");
		return 0;
	}

	dev1_mode.mode = MODE_CONTINUOUS;
	dev1_mode.frequency = 1;

	dev2_mode.mode = MODE_ONE_SHOT;

	res = ioctl(fd_1,SETMODE,&dev1_mode);
	if(res < 0){
		perror("IOCTL Error: ");
		return 0;
	}
	
	res = ioctl(fd_2,SETMODE,&dev2_mode);
	if(res < 0){
		perror("IOCTL Error: ");
		return 0;
	}

	// write input = 1 start periodic sampling
	ret = write(fd_1,&input, sizeof(input));
	if(ret < 0){
		perror("Write Error: ");
	}

	input = 0; // device 2 in one shot mode buffer not cleared
	ret = write(fd_2,&input, sizeof(input));
	if(ret < 0){
		perror("Write Error: ");
	}
	printf("user space sleeping\n");
	usleep(1000*1000);
	i = 30;
	while(i > 0){
		ret = read(fd_1,&output,sizeof(output));
		if(ret < 0){
			perror("Error: ");
		}

		//display
		printf("Sensor 1 Distance = %d ",output);
		usleep(100*1000);
		i--;
	}
	sleep(3);

	ret = read(fd_2,&output,sizeof(output));
	if(ret < 0){
		perror("Error: ");
	}

	//display
	printf("Sensor 2 Distance = %d ",output);

	input = 0; // device 1 continuous mode stopped
	ret = write(fd_1,&input, sizeof(input));
	if(ret < 0){
		perror("Write Error: ");
	}

	for(i =0; i < 6; i++){
	// device 1 should return fault on 6th reading
		ret = read(fd_1,&output,sizeof(output));
		if(ret < 0){
			perror("Error: ");
		}

		//display
		printf("Sensor 1 Distance = %d ",output);
	}

	//device 2 buffer is empty, read triggers one shot measurement
	ret = read(fd_2,&output,sizeof(output));
	if(ret < 0){
		perror("Error: ");
	}

	//display
	printf("Sensor 2 Distance = %d ",output);

	close(fd_1);
	close(fd_2);


	return 0;
}

void gpio_inits(){
	int fd,ret;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if(fd < 0)
		printf("export failed\n");
	
	ret = write(fd, "34", 2);
	if(ret<0)
		printf("request 34 failed\n");
	close(fd);

	// Set GPIO Direction
	fd = open("/sys/class/gpio/gpio34/direction", O_WRONLY);
	write(fd, "out", 3);
	close(fd);


	// Set Value
	fd = open("/sys/class/gpio/gpio34/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "35", 2);
	close(fd);

	// Set GPIO Direction
	fd = open("/sys/class/gpio/gpio35/direction", O_WRONLY);
	write(fd, "out", 3);
	close(fd);


	// Set Value
	fd = open("/sys/class/gpio/gpio35/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "77", 2);
	close(fd);

	// Set Value
	fd = open("/sys/class/gpio/gpio77/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//
	//*************************************************************//

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "16", 2);
	close(fd);

	// Set GPIO Direction
	fd = open("/sys/class/gpio/gpio16/direction", O_WRONLY);
	write(fd, "out", 3);
	close(fd);


	// Set Value
	fd = open("/sys/class/gpio/gpio16/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "17", 2);
	close(fd);

	// Set GPIO Direction
	fd = open("/sys/class/gpio/gpio17/direction", O_WRONLY);
	write(fd, "out", 3);
	close(fd);


	// Set Value
	fd = open("/sys/class/gpio/gpio17/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "76", 2);
	close(fd);

	// Set Value
	fd = open("/sys/class/gpio/gpio76/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//
	//*************************************************************//

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "18", 2);
	close(fd);

	// Set GPIO Direction
	fd = open("/sys/class/gpio/gpio18/direction", O_WRONLY);
	write(fd, "out", 3);
	close(fd);


	// Set Value
	fd = open("/sys/class/gpio/gpio18/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "19", 2);
	close(fd);

	// Set GPIO Direction
	fd = open("/sys/class/gpio/gpio19/direction", O_WRONLY);
	write(fd, "out", 3);
	close(fd);


	// Set Value
	fd = open("/sys/class/gpio/gpio19/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "66", 2);
	close(fd);

	// Set Value
	fd = open("/sys/class/gpio/gpio66/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//
	//*************************************************************//

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "36", 2);
	close(fd);

	// Set GPIO Direction
	fd = open("/sys/class/gpio/gpio36/direction", O_WRONLY);
	write(fd, "out", 3);
	close(fd);


	// Set Value
	fd = open("/sys/class/gpio/gpio36/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//

	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "37", 2);
	close(fd);

	// Set GPIO Direction
	fd = open("/sys/class/gpio/gpio37/direction", O_WRONLY);
	write(fd, "out", 3);
	close(fd);


	// Set Value
	fd = open("/sys/class/gpio/gpio37/value", O_WRONLY);
	write(fd, "0", 1);
	close(fd);

	//*************************************************************//
	//*************************************************************//
}