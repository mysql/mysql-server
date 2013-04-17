# -*- Mode: Makefile -*-

.DEFAULT_GOAL=all
TOKUROOT=../../
INCLUDEDIRS=-I$(TOKUROOT)include/windows -I$(TOKUROOT)newbrt -I$(TOKUROOT)windows/tests
include $(TOKUROOT)toku_include/Makefile.include

SKIP_WARNING += $(ICC_NOWARN)1418 #Non static functions do not need prototypes.

SRCS = $(wildcard test-*.c)
BINS_RAW = $(patsubst %.c,%,$(SRCS))
#Bins will be generated.
$(BINS): $(LIBPORTABILITY)
$(BINS): CFLAGS+=-DTESTDIR=\"dir.$<.dir\"

RUNTARGETS = $(patsubst %$(BINSUF),%.tdbrun,$(BINS))
VGRIND = 

all: $(BINS)

.PHONY: check
check: $(BINS) $(RUNTARGETS);

%.tdbrun: %$(BINSUF) $(PTHREAD_LOCAL)
	mkdir -p dir.$*
ifeq ($(VGRIND),)
	cd dir.$* && ../$< $(VERBVERBOSE) $(SUMMARIZE_CMD)
else
	cd dir.$* && .$(VGRIND) --log-file=$<.valgrind ../$< $(VERBVERBOSE); \
	if [ $$? = 0 ] ; then \
		grep "LEAK SUMMARY" dir.$*/$<.valgrind >/dev/null 2>&1; \
		if [ $$? = 0 ] ; then cat dir.$*/$<.valgrind; test 0 = 1; fi \
	fi \
	$(SUMMARIZE_CMD)
endif

clean:
	rm -rf $(TARGETS) *.valgrind dir.*

