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

install: $(LIBPORTABILITY)

$(LIBPORTABILITY): $(TARGET)
	cp $< $@

$(TARGET): $(OBJS)

clean:
	rm -rf $(TARGET) $(LIBPORTABILITY)

