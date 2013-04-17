CPPFLAGS = -I../../include -I..
CFLAGS = -Wall -Werror -g -O0
LDFLAGS = ../tokulinux.a 
SRCS = $(wildcard test-*.c)
TARGETS = $(patsubst %.c,%,$(SRCS))

all: $(TARGETS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

test-gettime: test-gettime.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) -lrt

clean:
	rm -rf $(TARGETS)