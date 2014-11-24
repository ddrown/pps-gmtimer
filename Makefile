ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
obj-m  := pps-gmtimer.o

else
# normal makefile
KDIR = ../linux

.PHONY: default clean

default:
	$(MAKE) -C $(KDIR) M=$$PWD ARCH=arm

clean:
	$(MAKE) -C $(KDIR) M=$$PWD ARCH=arm clean

endif
