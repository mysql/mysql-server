# -*- Mode: Makefile -*-

.DEFAULT_GOAL=install
TOKUROOT=../
INCLUDEDIRS=-I.
SKIP_LIBPORTABILITYRULE=1
include $(TOKUROOT)toku_include/Makefile.include

OPT_AROPT=-qnoipo #Disable ipo for lib creation even when optimization is on.

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.$(OEXT),$(SRCS))
TARGET = libtokuportability.$(AEXT)

build install: $(LIBPORTABILITY)

$(LIBPORTABILITY): $(TARGET)
	if ! diff $< $@ 2>/dev/null; then cp $< $@; fi

$(TARGET): $(OBJS)

$(OBJS): CFLAGS += -DTOKU_ALLOW_DEPRECATED
#Blank on purpose

clean:
	rm -rf $(TARGET) $(LIBPORTABILITY)

