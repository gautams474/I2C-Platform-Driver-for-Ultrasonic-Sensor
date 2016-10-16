#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

void display(int buf[5]){
	int i;
	for(i=0;i<5;i++){
		printf("%d ",buf[i]);
	}
	printf("\n");
}

int main(){
	int fd_1 = 0, fd_2 = 0, ret = 0;
	int buf1[5] = {1,2,3,4,5};
	int buf2[5] = {6,7,8,9,10};
	int rbuf[5];

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

	ret = write(fd_1,buf1, sizeof(buf1));
	if(ret < 0){
		perror("Error: ");
	}

	ret = write(fd_2,buf2, sizeof(buf2));
	if(ret < 0){
		perror("Error: ");
	}

	ret = read(fd_1,rbuf,sizeof(rbuf));
	if(ret < 0){
		perror("Error: ");
	}

	display(rbuf);

	ret = read(fd_2,rbuf,sizeof(rbuf));
	if(ret < 0){
		perror("Error: ");
	}

	display(rbuf);


	return 0;
}

