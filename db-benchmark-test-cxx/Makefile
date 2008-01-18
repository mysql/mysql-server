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

TARGETS = db-benchmark-test-bdb db-benchmark-test-tokudb

default: $(TARGETS)

clean:
	rm -rf $(TARGETS) $(BENCHDBS) *.gcno *.gcda *.gcov

db-benchmark-test-tokudb: ../lib/libtdb_cxx.a
db-benchmark-test-tokudb: db-benchmark-test.cpp
#	$(CXX) $(CXXFLAGS) -I../include $< -o $@ -DDIRSUF=tokudb ../lib/libdb.a ../lib/libtdb_cxx.a -lz
	$(CXX) $(CXXFLAGS) -I../include -L../lib -Wl,-rpath,$(PWD)/../lib $< -o $@ -ldb -ltdb_cxx -DDIRSUF=tokudb

db-benchmark-test-bdb: db-benchmark-test.cpp
	$(CXX) $(CXXFLAGS) $(BDB_CPPFLAGS) $(BDB_LDFLAGS)                      $< -o $@ -DDIRSUF=bdb
