# -*- Mode: Makefile -*-

.DEFAULT_GOAL=all
TOKUROOT=../../
INCLUDEDIRS=-I$(TOKUROOT)include/windows -I$(TOKUROOT)newbrt
include $(TOKUROOT)toku_include/Makefile.include

SKIP_WARNING += $(ICC_NOWARN)1418 #Non static functions do not need prototypes.

SRCS = $(wildcard test-*.c)
BINS_RAW = $(patsubst %.c,%,$(SRCS))
#Bins will be generated.
$(BINS): $(LIBPORTABILITY)
$(BINS): CFLAGS+=-DTESTDIR=\"dir.$<.dir\"

all: $(BINS)

