# -*- Mode: Makefile -*-
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

# standard build: make
# build with Berkeley DB 4.1: make BDBDIR=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDBDIR=~/svn/tokudb
# build with g++: make CC=g++


.DEFAULT_GOAL= build
TOKUROOT=../
INCLUDEDIRS=-I. -I../ -I$(TOKUROOT)include -I$(TOKUROOT)toku_include -I$(TOKUROOT)newbrt -I../range_tree -I../lock_tree
DEPEND_COMPILE += \
	./*.h \
#end

HERE = db-benchmark-test-cxx
include $(TOKUROOT)toku_include/Makefile.include

BENCHDBS = bench.bdb/ bench.tokudb

OPTFLAGS = -O2
CXXFLAGS = $(GCC_VERSION_SPECIFIC) -Wall -Werror -g $(OPTFLAGS) $(GCOV_FLAGS)

ifeq ($(CC),icc)
CXX=icc
endif

# CFLAGS += -pg

ifdef BDBDIR
BDB_CPPFLAGS = -I$(BDBDIR)/include -DHAVE_CXX_STDHEADERS
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb_cxx -Wl,-rpath,$(BDBDIR)/lib
else
BDB_CPPFLAGS =
BDB_LDFLAGS = -ldb_cxx
endif
BDB_LDFLAGS += -lpthread -L../lib -Wl,-rpath,$(PWD)/../lib -ltokuportability

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
	$(VGRIND) ./$(TARGET_TDB) $(QUIET) $(SUMMARIZE_CMD)

check-x: $(TARGET_TDB)
	$(VGRIND) ./$(TARGET_TDB) -x  $(QUIET) $(SUMMARIZE_CMD)

clean:
	rm -rf $(TARGETS) $(BENCHDBS) *.gcno *.gcda *.gcov

db-benchmark-test-tokudb: ../lib/libtokudb_cxx.a
db-benchmark-test-tokudb: db-benchmark-test.cpp
	$(CXX) $(CXXFLAGS) -I../include -I../toku_include -L../lib -Wl,-rpath,$(PWD)/../lib $< -o $@ -ltokudb -ltokudb_cxx -DDIRSUF=tokudb -lz -lpthread -ltokuportability

db-benchmark-test-bdb: db-benchmark-test.cpp
	$(CXX) $(CXXFLAGS) $(BDB_CPPFLAGS) $(BDB_LDFLAGS)                      $< -o $@ -DDIRSUF=bdb
