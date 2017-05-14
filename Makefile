# set KDIR to be the location of your kernel build directory
KDIR = ../linux

# For cross compiling, ensure the arm compiler is in
# your path and uncomment the following two lines
#export ARCH := arm
#export CROSS_COMPILE := arm-linux-gnueabihf-

# required for plat/dmtimer.h
EXTRA_CFLAGS += -I$(KDIR)/arch/arm/plat-omap/include


ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
obj-m  := pps-gmtimer.o

else
# normal makefile

.PHONY: default clean

default:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

DD-GPS-00A0.dtbo: DD-GPS-00A0.dts
	dtc -@ -I dts -O dtb -o $@ $<

DD-GPS-TCLKIN-00A0.dtbo: DD-GPS-TCLKIN-00A0.dts
	dtc -@ -I dts -O dtb -o $@ $<

endif
