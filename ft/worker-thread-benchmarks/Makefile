CC = g++
CPPFLAGS = -I. -D_GNU_SOURCE
CFLAGS = -Wall -g
LDFLAGS = -lpthread
TARGET = worker-test
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

$(TARGET): $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(TARGET) $(OBJS)

