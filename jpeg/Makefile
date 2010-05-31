#
#   Makefile for JPEG 
#
#   Copyright(c) 2004-2006, Samsung Electronics, Co., Ltd.
#

obj-m		:= s3c_jpeg.o 

s3c_jpeg-y 	:= LogMsg.o JPGOpr.o JPGMisc.o JPGMem.o s3c-jpeg.o

all:
	@echo "$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules"
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules

clean:
	rm -rf *.o *.ko *.mod.* *~ .*.cmd Module.symvers modules.order .tmp_versions .*.o.d 

install:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules_install
