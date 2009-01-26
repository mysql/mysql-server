# -*- Mode: Makefile -*-

.DEFAULT_GOAL=install
TOKUROOT=../
include $(TOKUROOT)toku_include/Makefile.include

OPT_AROPT=-qnoipo #Disable ipo for lib creation even when optimization is on.

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.$(OEXT),$(SRCS))
TARGET = libtokuportability.$(AEXT)

$(OBJS): CFLAGS += -DTOKU_ALLOW_DEPRECATED

install: $(TARGET)
	cp $(TARGET) $(LIBPORTABILITY)

$(TARGET): $(OBJS)

clean:
	rm -rf $(TARGET) $(LIBPORTABILITY)

# For build, do nothing
local build:;

