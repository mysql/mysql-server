SRCS = $(wildcard *.c)
TARGETS = $(patsubst %.c,%,$(SRCS)) $(patsubst %.c,%-bdb,$(SRCS))
CPPFLAGS = -I../include -D_GNU_SOURCE
CFLAGS = -g -std=c99 -Wall -Wextra -Werror -Wno-missing-field-initializers
ifeq ($(USE_STATIC_LIBS),1)
LIBTOKUDB = tokufractaltree_static
LIBTOKUPORTABILITY = tokuportability_static
else
LIBTOKUDB = tokufractaltree
LIBTOKUPORTABILITY = tokuportability
endif
LDFLAGS = -L../lib -l$(LIBTOKUDB) -l$(LIBTOKUPORTABILITY) -Wl,-rpath,../lib -lpthread -lz -ldl

default local: $(TARGETS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%-bdb: %.c
	$(CC) -D_GNU_SOURCE -DBDB $(CFLAGS) $^ -o $@ -ldb

check: $(TARGETS)
	./db-insert -x && ./db-scan --lwc --prelock --prelockflag

checknox: $(TARGETS)
	./db-insert && ./db-scan --nox --lwc --prelock --prelockflag

clean:
	rm -rf $(TARGETS) bench.* update.env.* insertm.env.*
