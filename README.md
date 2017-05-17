Beaglebone Black Hardware Counter Capture Driver
================================================

Building the kernel
-------------------

The stock BBB kernel needs a small change to arch/arm/plat-omap/dmtimer.c. A patch file is included for the mainline am33x-v4.10 kernel, but the change is easy to apply to other versions.

The instructions here are for building the 4.9.x TI BSP kernels following the details from: https://eewiki.net/display/linuxonarm/BeagleBone+Black#BeagleBoneBlack-TIBSP

 * Download and checkout the latest 4.9 scripts

> git clone https://github.com/RobertCNelson/ti-linux-kernel-dev  
> cd ti-linux-kernel-dev  
> export BUILD\_DIR=\`pwd\`  
> git checkout origin/ti-linux-4.9.y -b tmp  

 * Perform an initial build. The build scripts will download the kernel and apply the appropriate patches. If you are cross compiling, the scripts will also automatically download the Linaro arm-linux-gnueabihf- compiler

> ./build\_kernel.sh  

 * The kernel sources will now be in the KERNEL directory, apply the included patch or manually edit $BUILD\_DIR/KERNEL/arm/plat-omap/dmtimer.c
 * To edit, search for *OMAP\_TIMER\_SRC\_EXT\_CLK* and then change the next line *parent\_name = "timer\_ext\_ck"* to be *parent_name = "tclkin\_ck"*
    
> case OMAP\_TIMER\_SRC\_EXT\_CLK:  
>     parent\_name = "tclkin\_ck";  
>     break;  

 * Rebuild the kernel

> tools/rebuild.sh  

 * The newly built kernel and associated files will be in the $BUILD\_DIR/deploy directory. Copy these files to your BBB and install them

> mkdir /boot/dtbs/4.9.28-ti-r34  
> tar -xvzf 4.9.28-ti-r34-dtbs.tar.gz -C /boot/dtbs/4.9.28-ti-r34/  
> tar -xvzf 4.9.28-ti-r34-firmware.tar.gz -C /lib/firmware/  
> tar -xvzf 4.9.28-ti-r34-modules.tar.gz -C /  
> cp 4.9.28-ti-r34.zImage /boot/vmlinuz-4.9.28-ti-r34  
> cp config-4.9.28-ti-r34 /boot/config-4.9.28-ti-r34  
> update-initramfs -c -k 4.9.28-ti-r34  

 * Edit uEnv.txt and set uname\_r to be the new kernel

> uname\_r=4.9.28-ti-r34  

 * Reboot and check you are now using the new kernel using dmesg

Building the module
-------------------

 * KDIR must point to the kernel source, if you built a new kernel using the instructions above, this will be the *ti-linux-kernel-dev/KERNEL* directory
 * If you are cross compiling, you must make sure the arm compiler is in your PATH and include ARCH=arm and CROSS\_COMPILE=arm-linux-gnueabihf-. If you built a new kernel as above, the arm compiler will be in the *ti-linux-kernel-dev/dl* directory

> export PATH=$PATH:$BUILD\_DIR/dl/gcc-linaro-6.3.1-2017.02-x86\_64\_arm-linux-gnueabihf/bin  
> export KDIR=$BUILD\_DIR/KERNEL  
> make ARCH=arm CROSS\_COMPILE=arm-linux-gnueabihf-  

Installing the Device Tree Overlay
----------------------------------

 * Build the device tree overlays

> make DD-GPS-00A0.dtbo DD-GPS-TCLKIN-00A0.dtbo  
> cp DD-GPS-00A0.dtbo DD-GPS-TCLKIN-00A0.dtbo /lib/firmware/

If you want base hardware timer capture without using TCLKIN, use the DD-GPS overlay. Otherwise follow the section below to enable TCLKIN.

 * The overlay can be enabled manually for testing, or add the overlay to uEnv.txt or /etc/default/capemgr so it is enabled on reboot

> echo DD-GPS > /sys/devices/platform/bone\_capemgr/slots  

 * load the pps-gmtimer module

> insmod pps-gmtimer.ko

Using an external oscillator (TCLKIN)
-------------------------------------

TCLKIN support allows use of an external clock source on pin P9.41. The external clock rate is configurable up to 24Mhz and HDMI must be disabled.

 * The default clock frequency is 12Mhz and is configured via the device tree. Changing the frequency via an overlay does not appear to be supported, so the instructions below creates a new full device tree that is loaded at boot.

 * Choose one of the stock device trees where HDMI is disabled and decompile it

> cd /boot/dtbs/4.9.28-ti-r34  
> dtc -o am335x-boneblack-tclkin.dts am335x-boneblack-emmc-overlay.dtb  

 * edit the am335x-boneblack-tclkin.dts file, search for *tclkin_ck* and change the clock-frequency line to be the rate of the external clock

> tclkin_ck {  
>     #clock-cells = <0x0>;  
>     compatible = "fixed-clock";  
>     clock-frequency = <24000000>;  
>     linux,phandle = <0x17>;  
>     phandle = <0x17>;  
> };  

 * compile the new device tree

> dtc -o am335x-boneblack-tclkin.dtb am335x-boneblack-tclkin.dts  

 * edit /boot/uEnv.txt and set *dtb* to be the new compiled device tree

> dtb=am335x-boneblack-tclkin.dtb  

 * reboot to pick up the new device tree

 * The TCLKIN overlay can be enabled manually for testing, or add the overlay to uEnv.txt or /etc/default/capemgr so it is enabled on reboot

> echo DD-GPS-TCLKIN > /sys/devices/platform/bone\_capemgr/slots  

 * or add the overlay to uEnv.txt or /etc/default/capemgr so it gets enabled on reboot.

 * load the pps-gmtimer module

> insmod pps-gmtimer.ko  

The kernel will automatically switch to using the new timer as a clock source.

The clock source can be manually changed via sysfs. 

 * To use the new timer (if you aren't using timer4, use whichever timer you have configured in the overlay instead)

> echo timer4 > /sys/devices/system/clocksource/clocksource0/current\_clocksource  

 * To switch back to the default time source

> echo gp\_timer > /sys/devices/system/clocksource/clocksource0/current\_clocksource  

Monitoring operation
--------------------

The sysfs files in /sys/devices/platform/ocp/ocp:pps\_gmtimer/\* contain the counter's current state:

 * capture - the counter's raw value at PPS capture
 * count\_at\_interrupt - the counter's value at interrupt time
 * interrupt\_delta - the value used for the interrupt latency offset
 * pps\_ts - the final timestamp value sent to the pps system
 * timer\_counter - the raw counter value
 * stats - the number of captures and timer overflows
 * timer\_name - the name of time timer hardware
 * ctrlstatus - the state of the TCLR register (see the AM335x Technical Reference Manual for bit meanings)

The program "watch-pps" will watch these files and produce an output that looks like

> 1423775690.000 24000010 169 0.000007041 0 3988681035 -0.000001434  

The columns are: pps timestamp, capture difference, cycles between capture and interrupt, interrupt\_delta, raw capture value, offset

Notes on using timer4
---------------------

Some kernels have a problem where using timer4 causes an ethernet driver timeout and loss of network connectivity. This appears to be an issue in the 4.4.x kernels, but has been fixed by 4.9.28-ti-r34. If you get problems using timer4, then try upgrading to the latest kernel, or use a different timer.

