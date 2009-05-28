# -*- Mode: Makefile -*-

.DEFAULT_GOAL=install
TOKUROOT=../
INCLUDEDIRS=-I. -I$(TOKUROOT)newbrt
SKIP_LIBPORTABILITYRULE=1
include $(TOKUROOT)toku_include/Makefile.include

OPT_AROPT=-qnoipo #Disable ipo for lib creation even when optimization is on.

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.$(OEXT),$(SRCS))
TARGET = libtokuportability.$(AEXT)

build install: $(LIBPORTABILITY) $(PTHREAD_LIB)

PTHREAD_LIB_CRUNTIME=$(TOKUROOT)windows/lib/$(CRUNTIME)/pthreadVC2.dll
$(PTHREAD_LIB): $(PTHREAD_LIB_CRUNTIME)
	cp -u $< $@

$(LIBPORTABILITY): $(TARGET)
	cp -u $< $@

$(TARGET): $(OBJS)

check: $(TARGET)
	cd tests && $(MAKE) check

clean:
	rm -rf $(TARGET) $(LIBPORTABILITY) $(PTHREAD_LIB)
	cd tests && $(MAKE) clean
