#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

# standard build: make
# build with Berkeley DB 4.1: make BDBDIR=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDBDIR=~/svn/tokudb

BENCHDBS = bench.bdb/ bench.tokudb

OPTFLAGS = -O2
CXXFLAGS = -Wall -Werror -g $(OPTFLAGS) $(GCOV_FLAGS)
# CFLAGS += -pg

ifdef BDBDIR
BDB_CPPFLAGS = -I$(BDBDIR)/include
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb_cxx -lpthread -Wl,-rpath,$(BDBDIR)/lib
else
BDB_CPPFLAGS =
BDB_LDFLAGS = -ldb_cxx -lpthread
endif

TARGET_BDB = db-benchmark-test-bdb
TARGET_TDB = db-benchmark-test-tokudb
TARGETS = $(TARGET_BDB) $(TARGET_TDB)

HERE = db-benchmark-test-cxx
ifeq ($(SUMMARIZE),1)
SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sPASS\n" $(HERE)/$@; else printf "%-60sFAIL\n" $(HERE)/$@ ; test 0 = 1; fi
QUIET = -q
else
SUMMARIZE_CMD =
QUIET =
endif

default: build
build: $(TARGETS)

check: check-default

check-default: $(TARGET_TDB)
	$(VALGRIND) ./$(TARGET_TDB) $(QUIET) $(SUMMARIZE_CMD)

check-x: $(TARGET_TDB)
	$(VALGRIND) ./$(TARGET_TDB) -x  $(QUIET) $(SUMMARIZE_CMD)

clean:
	rm -rf $(TARGETS) $(BENCHDBS) *.gcno *.gcda *.gcov

db-benchmark-test-tokudb: ../lib/libtokudb_cxx.a
db-benchmark-test-tokudb: db-benchmark-test.cpp
	$(CXX) $(CXXFLAGS) -I../include -L../lib -Wl,-rpath,$(PWD)/../lib $< -o $@ -ltokudb -ltokudb_cxx -lz -DDIRSUF=tokudb

db-benchmark-test-bdb: db-benchmark-test.cpp
	$(CXX) $(CXXFLAGS) $(BDB_CPPFLAGS) $(BDB_LDFLAGS)                      $< -o $@ -DDIRSUF=bdb
