INCLUDEDIR= /lib/modules/`uname -r`/build/include
KERNELSOURCE=/usr/src/linux
DEVICE=/dev/input/tracker0

CFLAGS= -Wall -DITRAX_DEBUG -I$(INCLUDEDIR)
CFLAGSMODULE=-D__KERNEL__ -DMODULE -O 

KERNEL24 := $(shell if [`grep ^PATCHLEVEL $(KERNELSOURCE)/Makefile | cut -d "=" -f 2` == "4" ]; then echo -DK24; fi)


CC=gcc

OBJS= itrax.o 


LIBS=-lncurses 

PROGRAM=testtracker

all: $(OBJS) $(PROGRAM)


install: all
	if [ ! -d "/lib/modules/`uname -r`/misc" ]; then mkdir /lib/modules/`uname -r`/misc; fi
	cp itrax.o /lib/modules/`uname -r`/misc
	depmod -a
	if [ ! -c "$(DEVICE)" ]; then mknod $(DEVICE) c 13 128; fi 

$(OBJS): %.o: %.c %.h
	$(CC) -c $(CFLAGS) $(CFLAGSMODULE) $(KERNEL24) -o $@  $< 

clean:
	rm -f *.o *.ps *~ $(PROGRAM)

${PROGRAM}.o:  ${PROGRAM}.c
	${CC} ${CFLAGS} -c ${PROGRAM}.c
 
${PROGRAM}: ${PROGRAM}.o
	 ${CC} ${CFLAGS} -o ${PROGRAM} ${PROGRAM}.o ${LIBS} 






