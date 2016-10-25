CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SROOT=/opt/iot-devkit/1.7.3/sysroots/i586-poky-linux/

obj-m := HCSR_driver_platform.o
obj-m += HCSR_device_platform.o

default:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(SROOT)/usr/src/kernel M=$(PWD) modules
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(SROOT)/usr/src/kernel M=$(PWD) modules
	i586-poky-linux-gcc main.c -o main -Wall --sysroot=$(SROOT)
	
clean:
	rm main
	
	make ARCH=x86 CROSS_COMPILE=i586-poky-linux- -C $(SROOT)/usr/src/kernel M=$(PWD) clean
