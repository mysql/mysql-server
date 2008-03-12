#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

# standard build: make
# build with Berkeley DB 4.1: make BDBDIR=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDBDIR=~/svn/tokudb
# build with g++: make CC=g++

BENCHDBS = bench.bdb/ bench.tokudb

OPTFLAGS = -O2
CFLAGS = -Wall -Werror -g $(OPTFLAGS) $(GCOV_FLAGS) $(PROF_FLAGS)
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

check: check-default check-xfast

check-default: $(TARGET_TDB)
	$(VALGRIND) ./$(TARGET_TDB)

check-x: $(TARGET_TDB)
	$(VALGRIND) ./$(TARGET_TDB) -x


# A fast transaction test that detects #455.
check-xfast: $(TARGET_TDB)
	./$(TARGET_TDB) --noserial -x --valsize 1000  --cachesize 8000000 --xcount 1000 --periter 20000 1


clean:
	rm -rf $(TARGETS) $(BENCHDBS) *.gcno *.gcda *.gcov

# A hack to make gprof work.  See #515.
ifeq ($(PROF_FLAGS),-pg)
OFILES = ../src/ydb.o ../src/errors.o ../src/elocks.o ../newbrt/brt.o ../newbrt/brt-serialize.o ../newbrt/brt-verify.o ../newbrt/cachetable.o ../newbrt/fifo.o ../newbrt/key.o ../newbrt/memory.o ../newbrt/mempool.o ../newbrt/pma.o ../newbrt/ybt.o ../newbrt/primes.o ../newbrt/log.o ../newbrt/fingerprint.o ../newbrt/log_code.o ../newbrt/roll.o ../newbrt/toku_assert.o ../src/lock_tree/locktree.o ../src/lock_tree/rth.o ../src/lock_tree/lth.o ../src/range_tree/rangetree.o ../src/range_tree/tokuredblack.o
db-benchmark-test-tokudb: db-benchmark-test.c
	$(CC) $(CFLAGS) $(TDB_CPPFLAGS) $(OFILES) -lz $< -o $@ -DDIRSUF=tokudb
else
db-benchmark-test-tokudb: db-benchmark-test.c
	$(CC) $(CFLAGS) $(TDB_CPPFLAGS) $(TDB_LDFLAGS) $< -o $@ -DDIRSUF=tokudb
endif
db-benchmark-test-bdb: db-benchmark-test.c
	$(CC) $(CFLAGS) $(BDB_CPPFLAGS) $(BDB_LDFLAGS) $< -o $@ -DDIRSUF=bdb
