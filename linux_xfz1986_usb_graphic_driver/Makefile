######################################
#
#    RoboPeak USB LCD Display Linux Driver
#    
#    Copyright (C) 2009 - 2013 RoboPeak Team
#    This file is licensed under the GPL. See LICENSE in the package.
#
#    http://www.robopeak.net
#
#    Author Shikai Chen
#
######################################


DRIVER_NAME := xfz_udisp
KERNEL_SOURCE_DIR ?= /lib/modules/`uname -r`/build

EXTRA_CFLAGS +=-g -I$(PWD)/src -I$(PWD)/../common

obj-m := $(DRIVER_NAME).o

DRIVER_FILES := src/main.o \
                src/usb_transfer.o \
		src/fbhandlers.o \
		src/tiny_jpeg.o src/enc_raw_rgb.o src/enc_base.o src/enc_raw_rgb.o src/enc_jpg.o\
		src/test_pattern.o

$(DRIVER_NAME)-objs:= $(DRIVER_FILES)

modules:
	$(MAKE) -C $(KERNEL_SOURCE_DIR) KCPPFLAGS="$(EXTRA_CFLAGS)" M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNEL_SOURCE_DIR) M=$(PWD) modules_install

install: modules_install

clean:
	$(MAKE) -C $(KERNEL_SOURCE_DIR) M=$(PWD) clean
