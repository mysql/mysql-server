# standard build: make
# build with Berkeley DB 4.1: make BDBDIR=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDBDIR=~/svn/tokudb
# build with g++: make CC=g++
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../
INCLUDEDIRS=-I. -I$(TOKUROOT)newbrt -I$(TOKUROOT)include -I$(TOKUROOT)/toku_include -I$(TOKUROOT)src
DEPEND_COMPILE += \
	../*.h \
	../range_tree/*.h \
	../lock_tree/*.h \
	test.h \
#end

SKIP_NORETURN=1 #Do not add the -Wmissing-noreturn flag
HERE = db-benchmark-test
include $(TOKUROOT)toku_include/Makefile.include

BENCHDBS = bench.bdb/ bench.tokudb ptest*.dir/ x.dir/ xfast.dir/ scanrace.tokudb/

ifdef BDBDIR
BDB_CPPFLAGS = -D_GNU_SOURCE -I$(BDBDIR)/include -I../include
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb -Wl,-rpath,$(BDBDIR)/lib $(LIBPORTABILITY) -lpthread
else
BDB_CPPFLAGS = 
BDB_LDFLAGS = -ldb $(LIBPORTABILITY)
endif
BDB_CPPFLAGS+=-D_SVID_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_XOPEN_SOURCE=600 -DBDB -I$(TOKUROOT)toku_include -I$(PORTABILITY_HEADERS)

TOKUDB=libtokudb.$(SOEXT)
TDB_DLINK_FILES = $(TOKUROOT)lib/$(TOKUDB)

TARGET_BDB = db-benchmark-test-bdb$(BINSUF)
SCANSCAN_BDB = scanscan-bdb$(BINSUF)
TARGET_TDB = db-benchmark-test-tokudb$(BINSUF)
SCANSCAN_TDB = scanscan-tokudb$(BINSUF)
SCANRACE_TDB = scanrace-tokudb$(BINSUF)
TARGETS = $(TARGET_BDB) $(SCANSCAN_BDB) $(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB)

default: build
build: $(TARGETS) 
build.bdb: $(TARGET_BDB) $(SCANSCAN_BDB)
build.tdb: $(TARGET_TDB) $(SCANSCAN_TDB)

check: check-default check-xfast check-x check-no-rolltmp check-4G

check-default: $(TARGET_TDB)
	$(VGRIND) ./$(TARGET_TDB) $(VERBVERBOSE) $(SUMMARIZE_CMD)

check-x: $(TARGET_TDB)
	$(VGRIND) ./$(TARGET_TDB) $(VERBVERBOSE) -x --xcount 1000 --periter 100000 --env x.dir 10 $(SUMMARIZE_CMD)

# check-scanrace: VGRIND=tcheck_cl -w 140 -s 4 -p nothreads,delay=2,stall=900,noverbose
check-scanrace: $(SCANRACE_TDB) $(TARGET_TDB)
	./$(TARGET_TDB) --env scanrace.tokudb --periter 10000 --pagesize 4096 1
	$(VGRIND) ./$(SCANRACE_TDB) --env scanrace.tokudb $(SUMMARIZE_CMD)

# A fast transaction test that detects #455.
check-xfast: $(TARGET_TDB)
	./$(TARGET_TDB) $(VERBVERBOSE) --noserial -x --valsize 1000  --cachesize 8000000 --xcount 1000 --periter 20000 --env xfast.dir 1  $(SUMMARIZE_CMD)

# A relatively fast test that detects #853 (don't log changes to a dictionary created in the same txn)
check-no-rolltmp: $(TARGET_TDB)
	./$(TARGET_TDB) $(VERBVERBOSE) --env no-rolltmp.dir --singlex --nolog --check_small_rolltmp $(SUMMARIZE_CMD)

# Check to make sure that if we make a file that's bigger than 4GB that we can read the file back out and get all the rows.
check-4G: $(TARGET_TDB)  $(SCANSCAN_TDB)
	$(TARGET_TDB) -q --env 4g.dir --norandom --compressibility 1 --valsize 10000 1
	$(SCANSCAN_TDB) --env 4g.dir --lwc --prelock --prelockflag --nox > 4g.out
	fgrep "(5242880 rows)" 4g.out

clean:
	rm -f $(TARGETS)
	rm -rf $(BENCHDBS)

$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB): BIN_FROM_C_FLAGS+=-DDIRSUF=tokudb -I$(PORTABILITY_HEADERS) -I$(TOKUROOT)toku_include -I$(TOKUROOT)include

ifneq ($(PROF),)
    USE_STATIC=1
endif

# A hack to make gprof work.  See #515.
ifneq ($(USE_STATIC),)
$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB): DLINK_FILES=

OFILES = \
	$(TOKUROOT)src/ydb.lib \
	$(TOKUROOT)src/range_tree/rangetree.lib \
	$(TOKUROOT)src/lock_tree/locktree.lib \
	$(TOKUROOT)newbrt/newbrt.lib \
	$(LIBPORTABILITY) \
#end

$(TARGET_TDB): db-benchmark-test.c
	$(CC) $< $(BIN_FROM_C_FLAGS) $(OFILES) $(LINK_MUST_BE_LAST)
$(SCANSCAN_TDB): scanscan.c
	$(CC) $< $(BIN_FROM_C_FLAGS) $(OFILES) $(LINK_MUST_BE_LAST)
$(SCANRACE_TDB): scanrace.c
	$(CC) $< $(BIN_FROM_C_FLAGS) $(OFILES) $(LINK_MUST_BE_LAST)

else
$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB): DLINK_FILES=$(TDB_DLINK_FILES)
ifeq ($(SOEXT),dll) #Windows does not support rpath.
$(TOKUDB): $(TOKUROOT)lib/$(TOKUDB)
	cp $< $@
$(TOKUROOT)lib/$(TOKUDB):
	cd $(@D) && $(MAKE) $(@F).install
$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB): $(TOKUDB)
endif
$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB): RPATH_DIRS=$(dir $(TDB_DLINK_FILES))
$(TARGET_TDB): db-benchmark-test.c
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)
$(SCANSCAN_TDB): scanscan.c
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)
$(SCANRACE_TDB): scanrace.c
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)
endif

$(TARGET_BDB): db-benchmark-test.c
	$(CC) $(CFLAGS) $(BDB_CPPFLAGS) $< -o $@ -DDIRSUF=bdb $(BDB_LDFLAGS)
$(SCANSCAN_BDB): scanscan.c
	$(CC) $(CFLAGS) $(BDB_CPPFLAGS) $< -o $@ -DDIRSUF=bdb $(BDB_LDFLAGS)

PARGS =

ptest%: $(TARGET_TDB)
	./$< --env $@.dir $(PARGS)
parallel-test-8: ptest0 ptest1 ptest2 ptest3 ptest4 ptest5 ptest6 ptest7

