Beaglebone Black Hardware Counter Capture Driver
================================================

Building the kernel
-------------------

put kernel source in directory ../linux from https://github.com/beagleboard/linux/
The official beaglebone 3.8.x kernel (branch "3.8") needs commit 8fc7fcb593ac3c5730c8391c2d7db5b87e2d0bf2
You can backport it with the command: git cherry-pick 8fc7fcb593ac3c5730

To build on an Intel Linux machine, you'll need to install a cross compiler:
(Fedora) sudo yum install gcc-arm-linux-gnu

in directory ../linux:
uncompress /proc/config.gz from beaglebone and put it in .config
modify .config: CONFIG_CROSS_COMPILE="arm-linux-gnu-"

	make ARCH=arm oldconfig
	make ARCH=arm zImage
	make ARCH=arm modules

Installing the Device Tree Overlay
----------------------------------

on the BBB:
(install zImage and modules)

	make DD-GPS-00A0.dtbo
	cp DD-GPS-00A0.dtbo /lib/firmware/

Then, add DD-GPS to your list of capes in /etc/default/capemgr


Using an external oscillator (TCLKIN)
-------------------------------------

To use an external clock source on pin P9.41 (TCLKIN).  It accepts up to a 24MHz clock.

 * apply the patch kernel-tclkin.patch to your kernel
 * If you're not using a 24MHz clock, update the DEFINE_CLK_FIXED_RATE tclkin_ck definition in arch/arm/mach-omap2/cclock33xx_data.c 
 * rebuild your kernel
 * Use the device tree overlay file DD-GPS-TCLKIN-00A0.dtbo, which has the pinctl changes needed

To use this clock as your system time source:

	echo timer4 > /sys/devices/system/clocksource/clocksource0/current_clocksource

If you're not using the timer4 hardware, use the other timer's name in place.

To switch back to the default time source:

	echo gp_timer > /sys/devices/system/clocksource/clocksource0/current_clocksource


Monitoring operation
--------------------

The sysfs files in /sys/devices/ocp.3/pps_gmtimer.* contain the counter's current state:

 * capture - the counter's raw value at PPS capture
 * count_at_interrupt - the counter's value at interrupt time
 * interrupt_delta - the value used for the interrupt latency offset
 * pps_ts - the final timestamp value sent to the pps system
 * timer_counter - the raw counter value
 * stats - the number of captures and timer overflows
 * timer_name - the name of time timer hardware
 * ctrlstatus - the state of the TCLR register (see the AM335x Technical Reference Manual for bit meanings)

The program "watch-pps" will watch these files and produce an output that looks like:

 > 1423775690.000 24000010 169 0.000007041 0 3988681035 -0.000001434

The columns are: pps timestamp, capture difference, cycles between capture and interrupt, interrupt_delta, raw capture value, offset
