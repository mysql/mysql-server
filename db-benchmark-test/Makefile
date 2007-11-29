# standard build: make
# build with Berkeley DB 4.1: make BDBDIR=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDBDIR=~/svn/tokudb

BENCHDBS = bench.bdb/ bench.tokudb

OPTFLAGS = -O2
CFLAGS = -Wall -Werror -g
# CFLAGS += -pg
CFLAGS += $(OPTFLAGS)

ifdef BDBDIR
BDB_CPPFLAGS = -I$(BDBDIR)/include
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb -lpthread -Wl,-rpath,$(BDBDIR)/lib
else
BDB_CPPFLAGS =
BDB_LDFLAGS = -ldb
endif

TARGETS = db-benchmark-test-bdb db-benchmark-test-tokudb

default: $(TARGETS)

clean:
	rm -rf $(TARGETS) $(BENCHDBS)

db-benchmark-test-tokudb: db-benchmark-test.c
	cc $(CFLAGS) -I../include -L../lib -ldb -Wl,-rpath,$(PWD)/../lib $< -o $@ -DDIRSUF=tokudb
db-benchmark-test-bdb: db-benchmark-test.c
	cc $(CFLAGS) $(BDB_CPPFLAGS) $(BDB_LDFLAGS)                      $< -o $@ -DDIRSUF=bdb
