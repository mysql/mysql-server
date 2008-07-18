#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

# standard build: make
# build with Berkeley DB 4.1: make BDBDIR=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDBDIR=~/svn/tokudb
# build with g++: make CC=g++

HERE = db-benchmark-test
ifeq ($(SUMMARIZE),1)
SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sPASS\n" $(HERE)/$@; else printf "%-60sFAIL\n" $(HERE)/$@ ; test 0 = 1; fi
QUIET = -q
else
SUMMARIZE_CMD =
QUIET =
endif

BENCHDBS = bench.bdb/ bench.tokudb ptest*.dir/ x.dir/ xfast.dir/

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
TARGETS = $(TARGET_BDB) $(TARGET_TDB) scanscan-tokudb

default: build
build: $(TARGETS) 

check: check-default check-xfast check-x

check-default: $(TARGET_TDB)
	$(VGRIND) ./$(TARGET_TDB) $(QUIET) $(SUMMARIZE_CMD)

check-x: $(TARGET_TDB)
	$(VGRIND) ./$(TARGET_TDB) $(QUIET) -x --xcount 1000 --periter 100000 --env x.dir 10 $(SUMMARIZE_CMD)


# A fast transaction test that detects #455.
check-xfast: $(TARGET_TDB)
	./$(TARGET_TDB) $(QUIET) --noserial -x --valsize 1000  --cachesize 8000000 --xcount 1000 --periter 20000 --env xfast.dir 1  $(SUMMARIZE_CMD)


clean:
	rm -rf $(TARGETS) $(BENCHDBS) *.gcno *.gcda *.gcov

# A hack to make gprof work.  See #515.
ifeq ($(PROF_FLAGS),-pg)
OFILES = \
	../src/ydb.o ../src/errors.o ../src/elocks.o \
	../newbrt/newbrt.o \
	../src/lock_tree/locktree.o ../src/lock_tree/rth.o ../src/lock_tree/lth.o ../src/lock_tree/idlth.o ../src/lock_tree/db_id.o \
	../src/range_tree/rangetree.o
db-benchmark-test-tokudb: db-benchmark-test.c
	$(CC) $(CFLAGS) $(TDB_CPPFLAGS) $(OFILES) -lz $< -o $@ -DDIRSUF=tokudb
scanscan-tokudb: scanscan.c
	$(CC) $(CFLAGS) $(TDB_CPPFLAGS) $(OFILES) -lz $< -o $@ -DDIRSUF=tokudb
else
db-benchmark-test-tokudb: db-benchmark-test.c
	$(CC) $(CFLAGS) $(TDB_CPPFLAGS) $(TDB_LDFLAGS) $< -o $@ -DDIRSUF=tokudb
scanscan-tokudb: scanscan.c
	$(CC) $(CFLAGS) $(TDB_CPPFLAGS) $(TDB_LDFLAGS) $< -o $@ -DDIRSUF=tokudb
endif
db-benchmark-test-bdb: db-benchmark-test.c
	$(CC) $(CFLAGS) $(BDB_CPPFLAGS) $(BDB_LDFLAGS) $< -o $@ -DDIRSUF=bdb

PARGS =
ptest0 ptest1 ptest2 ptest3 ptest4 ptest5 ptest6 ptest7: db-benchmark-test-tokudb
	./db-benchmark-test-tokudb --env $@.dir $(PARGS)
parallel-test-8: ptest0 ptest1 ptest2 ptest3 ptest4 ptest5 ptest6 ptest7

