# -*- Mode: Makefile -*-

.DEFAULT_GOAL=install
TOKUROOT=../
INCLUDEDIRS=-I.
SKIP_LIBPORTABILITYRULE=1
include $(TOKUROOT)toku_include/Makefile.include

OPT_AROPT=-qnoipo #Disable ipo for lib creation even when optimization is on.

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.$(OEXT),$(SRCS))
TARGET = $(LIBPORTABILITY)

build install: $(TARGET)

ifeq ($(CC),icc)
#LINUX_NOSTDLIB=-nostdlib
LINUX_NOSTDLIB=-static-intel -diag-disable 10237
else
LINUX_NOSTDLIB=
endif

$(TARGET): $(OBJS)
	echo $(patsubst %,linux/%,$(OBJS)) > ../lib/tokuportability.olist
	$(CC) -shared $(SYMBOLS) $(OBJS) $(SKIP_WARNING) -o $(TARGET) $(LINUX_NOSTDLIB)

$(OBJS): CFLAGS += -DTOKU_ALLOW_DEPRECATED -D_GNU_SOURCE
$(OBJS): VISIBILITY=
#Blank on purpose

check: $(TARGET)
	cd tests && $(MAKE) check

clean:
	rm -rf $(TARGET) $(LIBPORTABILITY)
	cd tests && $(MAKE) clean

