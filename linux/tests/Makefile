CPPFLAGS = -I../../include
CFLAGS = -Wall -Werror -g -O0
LDFLAGS = ../tokulinux.a
SRCS = $(wildcard test-*.c)
TARGETS = $(patsubst %.c,%,$(SRCS))

all: $(TARGETS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -rf $(TARGETS)