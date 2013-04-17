# -*- Mode: Makefile -*-

.DEFAULT_GOAL=all
TOKUROOT=../../
INCLUDEDIRS=-I$(TOKUROOT)include/windows -I$(TOKUROOT)newbrt -I$(TOKUROOT)windows/tests
include $(TOKUROOT)toku_include/Makefile.include

SKIP_WARNING += $(ICC_NOWARN)1418 #Non static functions do not need prototypes.

SRCS=$(sort $(filter-out dir.%.c,$(wildcard *.c)))
BINS_RAW = $(patsubst %.c,%,$(SRCS))
#Bins will be generated.
$(BINS): $(LIBPORTABILITY)
$(BINS): CFLAGS+=-DTESTDIR=\"dir.$<.dir\"

RUNTARGETS = $(patsubst %$(BINSUF),%.tdbrun,$(BINS))
VGRIND = 

all: $(BINS)

.PHONY: check
check: $(BINS) $(RUNTARGETS);

test-pwrite4g.tdbrun: TEST_EXTRA_ARGS=.
test-pwrite4g.tdbrun: VERBVERBOSE=
%.tdbrun: %$(BINSUF) $(PTHREAD_LOCAL)
	mkdir -p dir.$*
ifeq ($(VGRIND),)
	cd dir.$* && ../$< $(TEST_EXTRA_ARGS) $(VERBVERBOSE) $(SUMMARIZE_CMD)
else
	cd dir.$* && .$(VGRIND) --log-file=$<.valgrind ../$< $(TEST_EXTRA_ARGS) $(VERBVERBOSE); \
	if [ $$? = 0 ] ; then \
		grep "LEAK SUMMARY" dir.$*/$<.valgrind >/dev/null 2>&1; \
		if [ $$? = 0 ] ; then cat dir.$*/$<.valgrind; test 0 = 1; fi \
	fi \
	$(SUMMARIZE_CMD)
endif

clean:
	rm -rf $(TARGETS) *.valgrind dir.*

