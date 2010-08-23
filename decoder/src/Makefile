obj-m := rc_decoder.o timestamp.o decoder.o 

KDIR  := /lib/modules/$(shell uname -r)/build
PWD   := $(shell pwd)
INCLUDE := $(PWD)/../include/

make:
	$(MAKE) -I$(INCLUDE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -I$(INCLUDE) -C $(KDIR) M=$(PWD) clean
