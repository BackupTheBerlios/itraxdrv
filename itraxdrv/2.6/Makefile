DEVICE=/dev/input/tracker0
TARGET=itrax.o
INSTALL = install
HEADER = itrax.h
#CFLAGS=-g -Wall
LIB=Itrax_Module

OBJS= itrax.ko 
PROGRAM=testtracker


ifndef PREFIX
ifndef MIPSRC
PREFIX = /usr/local
else
PREFIX = $(MIPSRC)
endif
endif

INCDIR = $(PREFIX)/include

ifneq ($(KERNELRELEASE),)
obj-m	:= ${TARGET}

else
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

endif

all: $(OBJS) $(PROGRAM)
	$(MAKE)	-C $(KDIR)	SUBDIRS=$(PWD) modules

clean:
	rm -f *~ ${TARGET}.o ${TARGET}.ko .${TARGET}*
	rm -f .built_in.o.cmd built_in.o
	rm -f .*.cmd *.ko *.mod.c *.mod.o *.o *.ko

install: all
	if [ ! -d $(INCDIR)/$(LIB) ]; then\
		mkdir -p $(INCDIR)/$(LIB);\
	fi
	$(INSTALL) -m 644 $(HEADER) $(INCDIR)/$(LIB)
	if [ ! -d "/lib/modules/`uname -r`/kernel/misc" ]; then mkdir /lib/modules/`uname -r`/kernel/misc; fi
	cp itrax.ko /lib/modules/`uname -r`/kernel/misc
	depmod -a
	if [ ! -c "$(DEVICE)" ]; then mknod $(DEVICE) c 13 128; fi 

$(OBJS): %.ko: %.c %.h
	$(MAKE)	-C $(KDIR)	SUBDIRS=$(PWD) modules

${PROGRAM}.o:  ${PROGRAM}.c
	${CC} ${CFLAGS} -c ${PROGRAM}.c
 
${PROGRAM}: ${PROGRAM}.o
	 ${CC} ${CFLAGS} -o ${PROGRAM} ${PROGRAM}.o ${LDLIBS} 

