

KERNEL24 := $(shell if [`grep ^PATCHLEVEL $(KDIR)/Makefile | cut -d "=" -f 2` = "4" ]; then echo -DK24; fi)


OBJS= itrax.o 
HEADER = itrax.h
# this is a fake to tell install where to install the header, 
# no lib will be build
LIB=Itrax_Module


INCDIR = $(PREFIX)/include


all: $(OBJS)
#	@echo Kernelversion: $(RUNNINGKERNEL)
#	@echo Kernel at: $(KERNELSOURCE)

install: all
	$(INSTALL) -m 644 $(HEADER) $(INCDIR)/$(LIB)
	cp itrax.o /lib/modules/`uname -r`/misc


$(OBJS): %.o: %.c %.h
	$(CC) -c $(CFLAGS) $(CFLAGSMODULE) $(KERNEL24) -o $@  $< 

clean:
	rm -f *.o *.ps *~ $(PROGRAM)

distclean: clean






