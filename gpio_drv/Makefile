# Makefile for the MTUNL GPIO driver 
# Please modify here or set environments.
# KERNELDIR should be pointed to the build directory of your kernel.
# 
DEBUG ?= y
KERNELDIR ?= /home/pursuitxh/Perforce/indctlp4_1676/liwm_marvell/avb/kernel/linux-2.6.30.10

KBUILD_V:ERBOSE:=1

mtunl_gpio-objs   := gpio.o 
obj-m += mtunl_gpio.o

ifeq ($(DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG
endif

default:
	make -C $(KERNELDIR) LANG=C KBUILD_VERBOSE=${KBUILD_VERBOSE}  M=`pwd` modules

check:
	make -C $(KERNELDIR) LANG=C KBUILD_VERBOSE=${KBUILD_VERBOSE} C=1 M=`pwd` modules

%.x:%.c
	$(CC) -o $@ $< $(EXTRA_CFLAGS)

.PHONY: cscope
cscope:
	cscope -b -k -R

.PHONY: romfs
romfs:
	$(FS_INSTALL) mtunl_gpio.ko /lib/modules/mtunl_gpio/mtunl_gpio.ko

.PHONY: clean
clean:
	make -C $(KERNELDIR) LANG=C KBUILD_VERBOSE=${KBUILD_VERBOSE}  M=`pwd` clean
	#rm -f *.x *~ *.cmd *.o *.ko *.mod.c
