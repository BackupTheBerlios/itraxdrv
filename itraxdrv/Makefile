# toplevel Makefile for itraxdrv


export KVERSION=$(shell uname -r)
export KDIR=/lib/modules/$(KVERSION)/source

DEVICE=/dev/input/tracker0

CFLAGS= -Wall -DITRAX_DEBUG -I$(INCLUDEDIR)
CFLAGSMODULE=-D__KERNEL__ -DMODULE -O 


# decide in which subdir to decend
SUBDIR=$(shell uname -r|cut -c 1-3)

INSTALL = install

ifndef PREFIX
PREFIX = /usr/local
endif

export LIB=itraxdrv
export INCDIR = $(PREFIX)/include/$(LIB)


all: 
#	@echo Kernelversion: $(KVERSION)
#	@echo Kernel at: $(KDIR)
#	@echo Patchlevel: $(PATCHLEVEL)
#	@echo Subdir: $(SUBDIR)
	make -C $(SUBDIR)


install: 
	if [ ! -d "/lib/modules/`uname -r`/misc" ]; then mkdir /lib/modules/`uname -r`/misc; fi
	if [ ! -d $(INCDIR) ]; then\
		mkdir -p $(INCDIR);\
	fi
	make -C $(SUBDIR) install
	depmod -a
	if [ ! -c "$(DEVICE)" ]; then mknod $(DEVICE) c 13 128; fi 

example:
	make -C examples LOCALINC=$(SUBDIR)

clean:
	make -C 2.4 clean
	make -C 2.6 clean
