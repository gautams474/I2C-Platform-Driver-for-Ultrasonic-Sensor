KERNELDIR := /lib/modules/$(shell uname -r)/build
CLEANFILE := *.dis *.o *.ko *.mod.* *.symvers *.*.old *.order *.cmd
obj-m := HCSR_driver.o 

default:
	make -C $(KERNELDIR)  M=$(CURDIR) modules
	gcc main.c -o main
	sudo insmod HCSR_driver.ko
	sudo chmod 666 /dev/HCSR_1
	sudo chmod 666 /dev/HCSR_2

clean:
	rm -f $(CLEANFILE)
	sudo rmmod HCSR_driver
	
