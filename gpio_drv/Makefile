# Makefile for the GPIO driver 
# KERNELDIR should be pointed to the build directory of your kernel.

DEBUG ?= y
KERNELDIR ?= /home/pursuitxh/kernel/linux-2.6.30.10

KBUILD_V:ERBOSE:=1

gpio-objs   := gpio.o 
obj-m += gpio.o

ifeq ($(DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG
endif

default:
	make -C $(KERNELDIR) LANG=C KBUILD_VERBOSE=${KBUILD_VERBOSE}  M=`pwd` modules

check:
	make -C $(KERNELDIR) LANG=C KBUILD_VERBOSE=${KBUILD_VERBOSE} C=1 M=`pwd` modules

%.x:%.c
	$(CC) -o $@ $< $(EXTRA_CFLAGS)

.PHONY: clean
clean:
	make -C $(KERNELDIR) LANG=C KBUILD_VERBOSE=${KBUILD_VERBOSE}  M=`pwd` clean
	#rm -f *.x *~ *.cmd *.o *.ko *.mod.c
