TARGET=itrax.o
HEADER = itrax.h
#CFLAGS=-g -Wall


OBJS= itrax.ko 


ifneq ($(KERNELRELEASE),)
obj-m	:= ${TARGET}

else
PWD	:= $(shell pwd)

endif

all: 	$(OBJS)
	@echo Kernelversion: $(KVERSION)
	@echo Kernel at: $(KDIR)

clean:
	rm -f *~ ${TARGET}.o ${TARGET}.ko .${TARGET}*
	rm -f .built_in.o.cmd built_in.o
	rm -f .*.cmd *.ko *.mod.c *.mod.o *.o *.ko

install: all
	$(INSTALL) -m 644 $(HEADER) $(INCDIR)
	cp itrax.ko /lib/modules/`uname -r`/kernel/misc


$(OBJS): %.ko: %.c %.h
	$(MAKE)	-C $(KDIR)	SUBDIRS=$(PWD) modules

