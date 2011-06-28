# -*- Mode: Makefile -*-
CPPFLAGS = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CPPFLAGS += -I../../toku_include -I.. -I.
OPTFLAGS = -O0 
CFLAGS = -Wall -Werror -g $(OPTFLAGS) -std=c99
ifeq ($(GCCVERSION),4.4.4)
	CFLAGS += -Wno-deprecated
else ifeq ($(GCCVERSION),4.4.2)
	CFLAGS += -Wno-deprecated
else ifeq ($(GCCVERSION),4.4.1)
	CFLAGS += -Wno-deprecated
else ifeq ($(GCCVERSION),4.4.0)
	CFLAGS += -Wno-deprecated
endif
ifneq ($(GCOV),)
	CFLAGS += -fprofile-arcs -ftest-coverage -DGCOV
endif
LDFLAGS = -L../../lib -Wl,-rpath,../../lib
LDLIBS = -ltokuportability -lpthread
SRCS=$(sort $(filter-out dir.%.c,$(wildcard *.c)))
TARGETS = $(patsubst %.c,%,$(SRCS))
RUNTARGETS = $(patsubst %,%.tdbrun,$(TARGETS))
VGRIND = valgrind

ifeq ($(CC),icc)
	CFLAGS += -diag-disable 981
endif

HERE=linux/tests
ifeq ($(SUMMARIZE),1)
SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sPASS\n" $(HERE)/$@; else printf "%-60sFAIL\n" $(HERE)/$@ ; test 0 = 1; fi
SUMMARIZE_SHOULD_FAIL= ;if test $$? = 0; then printf "%-60sXFAIL\n" $(HERE)/$@; else printf "%-60sXPASS\n" $(HERE)/$@ ; test 0 = 1; fi
else
SUMMARIZE_CMD =
endif
INVERTER=;test $$? -ne 0

all: $(TARGETS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS) -lrt

.PHONY: check build
build: $(TARGETS)
check: $(TARGETS) $(RUNTARGETS);

test-rwlock: OPTFLAGS=-O3
test-rwlock: ../toku_pthread.h

# pwrite4g needs an argument to tell it which directory to write temporary files
test-pwrite4g.tdbrun: TEST_EXTRA_ARGS=.
%.tdbrun: %
ifeq ($(VGRIND),)
	./$< $(TEST_EXTRA_ARGS) $(SUMMARIZE_CMD)
else
	$(VGRIND) --error-exitcode=1 --quiet --leak-check=full --show-reachable=yes --log-file=$<.check.valgrind ./$< $(TEST_EXTRA_ARGS) >$<.check.output 2>&1; \
	if [ $$? = 0 ] ; then \
		lines=`cat $<.check.valgrind | wc -l`; \
		if [ $$lines -ne 0 ] ; then cat $<.check.valgrind; test 0 = 1; fi \
	else \
	        cat $<.check.valgrind; test 0 = 1; \
	fi \
	$(SUMMARIZE_CMD)
endif

# check to see if that logic makes an error when there is a leak.
try-leak-lost.tdbrun: try-leak-lost
ifeq ($(VGRIND),)
	./$< $(SUMMARIZE_SHOULD_FAIL)
else
	$(VGRIND) --error-exitcode=1 --quiet --leak-check=full --show-reachable=yes --log-file=$<.check.valgrind ./$< >$<.check.output 2>&1; \
	if [ $$? = 0 ] ; then \
		lines=`cat $<.check.valgrind | wc -l`; \
		if [ $$lines -ne 0 ] ; then cat $<.check.valgrind; test 0 = 1; fi \
	else \
	        test 0 = 1; \
	fi \
	$(INVERTER) \
	$(SUMMARIZE_SHOULD_FAIL)
endif

try-leak-reachable.tdbrun: try-leak-reachable
ifeq ($(VGRIND),)
	./$< $(SUMMARIZE_SHOULD_FAIL)
else
	$(VGRIND) --error-exitcode=1 --quiet --leak-check=full --show-reachable=yes --log-file=$<.check.valgrind ./$< >$<.check.output 2>&1; \
	if [ $$? = 0 ] ; then \
		lines=`cat $<.check.valgrind | wc -l`; \
		if [ $$lines -ne 0 ] ; then cat $<.check.valgrind; test 0 = 1; fi \
	else \
	        test 0 = 1; \
	fi \
	$(INVERTER) \
	$(SUMMARIZE_SHOULD_FAIL)
endif

try-assert0.tdbrun: try-assert0
	./$< 2> /dev/null $(INVERTER) $(SUMMARIZE_SHOULD_FAIL)

try-assert-zero.tdbrun: try-assert-zero
	./$< 2> /dev/null $(INVERTER) $(SUMMARIZE_SHOULD_FAIL)

clean:
	rm -rf $(TARGETS) *.check.output *.check.valgrind pwrite4g.data testdir dir.*.c

try-assert0.tdbrun:


foo:
	echo $(TARGETS)
