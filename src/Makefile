ifneq ($(KERNELRELEASE),)
    obj-m += ring.o
    obj-m += rc.o
    obj-m += rc_decoder.o
    rc_decoder-objs := ring.o rc.o 
    
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *.ko *.mod.* Module.* .hello* .tmp* modules.order

install:
	make && scp rc_decoder.ko root@192.168.1.6:~/ENEL675/rc

endif

