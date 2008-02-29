#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

# standard build: make
# build with Berkeley DB 4.1: make BDBDIR=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDBDIR=~/svn/tokudb
# build with g++: make CC=g++

BENCHDBS = bench.bdb/ bench.tokudb

OPTFLAGS = -O2
CFLAGS = -Wall -Werror -g $(OPTFLAGS) $(GCOV_FLAGS)
# CFLAGS += -pg

ifdef BDBDIR
BDB_CPPFLAGS = -I$(BDBDIR)/include
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb -lpthread -Wl,-rpath,$(BDBDIR)/lib
else
BDB_CPPFLAGS =
BDB_LDFLAGS = -ldb
endif
TDB_CPPFLAGS = -I../include
TDB_LDFLAGS = -L../lib -ltokudb -Wl,-rpath,$(PWD)/../lib

TARGET_BDB = db-benchmark-test-bdb
TARGET_TDB = db-benchmark-test-tokudb
TARGETS = $(TARGET_BDB) $(TARGET_TDB)

default: $(TARGETS)

check: check-default check-x2

check-default: $(TARGET_TDB)
	$(VALGRIND) ./$(TARGET_TDB)

check-x: $(TARGET_TDB)
	$(VALGRIND) ./$(TARGET_TDB) -x


# A fast transaction test that detects #455.
check-xfast: $(TARGET_TDB)
	./$(TARGET_TDB) --noserial -x --valsize 1000  --cachesize 8000000 --xcount 1000 --periter 20000 1


clean:
	rm -rf $(TARGETS) $(BENCHDBS) *.gcno *.gcda *.gcov

db-benchmark-test-tokudb: db-benchmark-test.c
	$(CC) $(CFLAGS) $(TDB_CPPFLAGS) $(TDB_LDFLAGS) $< -o $@ -DDIRSUF=tokudb
db-benchmark-test-bdb: db-benchmark-test.c
	$(CC) $(CFLAGS) $(BDB_CPPFLAGS) $(BDB_LDFLAGS) $< -o $@ -DDIRSUF=bdb
