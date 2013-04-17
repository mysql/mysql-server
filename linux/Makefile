# -*- Mode: Makefile -*-

.DEFAULT_GOAL=install
TOKUROOT=../
INCLUDEDIRS=-I$(TOKUROOT)include/linux -I$(TOKUROOT)newbrt
include $(TOKUROOT)include/Makefile.include

OPT_AROPT=-qnoipo #Disable ipo for lib creation even when optimization is on.

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.$(OEXT),$(SRCS))
TARGET = libtokuportability.$(AEXT)

$(OBJS): CFLAGS += -DTOKU_ALLOW_DEPRECATED_FSTAT

install: $(TARGET)
	$(MAYBEATSIGN)cp $(TARGET) $(LIBPORTABILITY)

$(TARGET): $(OBJS)

clean:
	$(MAYBEATSIGN)rm -rf $(TARGET) $(LIBPORTABILITY)

