CPPFLAGS = -I../../toku_include -I..
CFLAGS = -Wall -Werror -g -O0
LDFLAGS = ../libtokuportability.a -lpthread
SRCS = $(wildcard test-*.c)
TARGETS = $(patsubst %.c,%,$(SRCS))
RUNTARGETS = $(patsubst %,%.tdbrun,$(TARGETS))
VGRIND = valgrind

all: $(TARGETS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

test-gettime: test-gettime.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) -lrt

check: $(RUNTARGETS)

%.tdbrun: %
	$(VGRIND) ./$<

clean:
	rm -rf $(TARGETS)