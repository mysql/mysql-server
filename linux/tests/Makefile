CPPFLAGS = -D_GNU_SOURCE
CPPFLAGS += -I../../toku_include -I..
CFLAGS = -Wall -Werror -g -O0
LDFLAGS = ../libtokuportability.a -lpthread
SRCS = $(wildcard test-*.c)
TARGETS = $(patsubst %.c,%,$(SRCS))
RUNTARGETS = $(patsubst %,%.tdbrun,$(TARGETS))
VGRIND = valgrind

ifeq ($(CC),icc)
	CFLAGS += -diag-disable 981
endif

all: $(TARGETS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

test-gettime: test-gettime.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) -lrt

check: $(TARGETS) $(RUNTARGETS)

%.tdbrun: %
ifeq ($(VGRIND),)
	./$<
else
	$(VGRIND) --log-file=$<.valgrind ./$<; \
	if [ $$? = 0 ] ; then \
		grep "LEAK SUMMARY" $<.valgrind >/dev/null 2>&1; \
		if [ $$? = 0 ] ; then cat $<.valgrind; exit 1; fi \
	fi
endif

clean:
	rm -rf $(TARGETS) *.valgrind
