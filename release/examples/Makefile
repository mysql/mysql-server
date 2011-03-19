SRCS = $(wildcard *.c)
TARGETS = $(patsubst %.c,%,$(SRCS)) $(patsubst %.c,%-bdb,$(SRCS))
CPPFLAGS = -I../include -D_GNU_SOURCE
CFLAGS = -g -std=c99 -Wall
LIBTOKUDB = tokudb
LDFLAGS = -L../lib -l$(LIBTOKUDB) -Wl,-rpath,../lib

default: $(TARGETS)

%-bdb: %.c
	$(CC) -D_GNU_SOURCE -DBDB $(CFLAGS) $^ -o $@ -ldb

check: $(TARGETS)
	./db-insert -x && ./db-scan --lwc --prelock --prelockflag

checknox: $(TARGETS)
	./db-insert && ./db-scan --nox --lwc --prelock --prelockflag

clean:
	rm -rf $(TARGETS) bench.*
