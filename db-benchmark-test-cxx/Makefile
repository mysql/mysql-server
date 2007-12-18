#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

# standard build: make
# build with Berkeley DB 4.1: make BDBDIR=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDBDIR=~/svn/tokudb

BENCHDBS = bench.bdb/ bench.tokudb

OPTFLAGS = -O2
CXXFLAGS = -Wall -Werror -g
# CFLAGS += -pg
CXXFLAGS += $(OPTFLAGS)

ifdef BDBDIR
BDB_CPPFLAGS = -I$(BDBDIR)/include
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb_cxx -Wl,-rpath,$(BDBDIR)/lib
else
BDB_CPPFLAGS =
BDB_LDFLAGS = -ldb_cxx
endif

TARGETS = db-benchmark-test-bdb db-benchmark-test-tokudb

default: $(TARGETS)

clean:
	rm -rf $(TARGETS) $(BENCHDBS)

db-benchmark-test-tokudb: db-benchmark-test.cpp
	$(CXX) $(CXXFLAGS) -I../include -L../lib -ldb -Wl,-rpath,$(PWD)/../lib $< -o $@ -DDIRSUF=tokudb
db-benchmark-test-bdb: db-benchmark-test.cpp
	$(CXX) $(CXXFLAGS) $(BDB_CPPFLAGS) $(BDB_LDFLAGS)                      $< -o $@ -DDIRSUF=bdb
