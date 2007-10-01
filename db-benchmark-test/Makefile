# standard build: make
# build with Berkeley DB 4.1: make BDB=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDB=~/svn/tokudb

BENCHDBS = bench.bdb/ bench.tokudb

CFLAGS = -Wall -Werror -g
# CFLAGS += -pg
CFLAGS += -O2 

ifdef BDB
CPPFLAGS = -I$(BDB)/include
LDFLAGS = -L$(BDB)/lib -ldb -lpthread -Wl,-rpath,$(BDB)/lib
else
CPPFLAGS =
LDFLAGS = -ldb
endif

TARGETS = db-benchmark-test-bdb db-benchmark-test-tokudb

default: $(TARGETS)

clean:
	rm -rf $(TARGETS) $(BENCHDBS)

db-benchmark-test-tokudb: db-benchmark-test.c
	cc -Wall -Werror -g -O2  -I../include -L../lib -ldb -Wl,-rpath,../lib  $<  -o $@ -DDIRSUF=tokudb
db-benchmark-test-bdb: db-benchmark-test.c
	cc -Wall -Werror -g -O2                        -ldb                    $<  -o $@ -DDIRSUF=bdb
