# -*- Mode: Makefile -*-

.DEFAULT_GOAL=install
TOKUROOT=../
INCLUDEDIRS=-I.
SKIP_LIBPORTABILITYRULE=1
include $(TOKUROOT)include/Makefile.include

OPT_AROPT=-qnoipo #Disable ipo for lib creation even when optimization is on.

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.$(OEXT),$(SRCS))
TARGET = tokuwindows.$(AEXT)

install: $(LIBPORTABILITY)

$(LIBPORTABILITY): $(TARGET)
	$(MAYBEATSIGN)cp $< $@

$(TARGET): $(OBJS)

clean:
	$(MAYBEATSIGN)rm -rf $(TARGET) $(LIBPORTABILITY)

