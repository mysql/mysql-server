# -*- Mode: Makefile -*-

.DEFAULT_GOAL=install
TOKUROOT=../
INCLUDEDIRS=-I.
include $(TOKUROOT)toku_include/Makefile.include

OPT_AROPT=-qnoipo #Disable ipo for lib creation even when optimization is on.

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.$(OEXT),$(SRCS))
TARGET = $(LIBPORTABILITY_SO) $(LIBPORTABILITY_A)

build install: $(TARGET)

ifeq ($(CC),icc)
#LINUX_NOSTDLIB=-nostdlib
LINUX_NOSTDLIB=-static-intel -diag-disable 10237
else
LINUX_NOSTDLIB=
endif

$(LIBPORTABILITY_SO): $(OBJS)
	$(CC) -shared $(SYMBOLS) $(OBJS) $(SKIP_WARNING) -o $@ $(LINUX_NOSTDLIB)
$(LIBPORTABILITY_A): $(OBJS)
	$(CC) -shared $(SYMBOLS) $(OBJS) $(SKIP_WARNING) -o $@ $(LINUX_NOSTDLIB)

$(OBJS): CFLAGS += -DTOKU_ALLOW_DEPRECATED -D_GNU_SOURCE
$(OBJS): VISIBILITY=
#Blank on purpose

check: $(TARGET)
	cd tests && $(MAKE) check

clean:
	rm -rf $(TARGET)
	cd tests && $(MAKE) clean

