# -*- Mode: Makefile -*-
CPPFLAGS = -D_GNU_SOURCE
CPPFLAGS += -I../../toku_include -I.. -I.
CFLAGS = -Wall -Werror -g -O0 -std=c99
ifneq ($(GCOV),)
	CFLAGS += -fprofile-arcs -ftest-coverage -DGCOV
endif
LDFLAGS = ../libtokuportability.a -lpthread
SRCS = $(wildcard test-*.c)
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
INVERTER=;test $$? -ne 0
else
SUMMARIZE_CMD =
endif

all: $(TARGETS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

test-gettime: test-gettime.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) -lrt

.PHONY: check build
build: $(TARGETS)
check: $(TARGETS) $(RUNTARGETS);

%.tdbrun: %
ifeq ($(VGRIND),)
	./$< $(SUMMARIZE_CMD)
else
	$(VGRIND) --error-exitcode=1 --quiet --leak-check=full --log-file=$<.valgrind ./$<; \
	if [ $$? = 0 ] ; then \
		lines=`cat $<.valgrind | wc -l`; \
		if [ $$lines -ne 0 ] ; then cat $<.valgrind; test 0 = 1; fi \
	fi \
	$(SUMMARIZE_CMD)
endif

clean:
	rm -rf $(TARGETS) *.valgrind pwrite4g.data testdir

