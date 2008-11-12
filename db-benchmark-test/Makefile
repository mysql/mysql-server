# standard build: make
# build with Berkeley DB 4.1: make BDBDIR=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDBDIR=~/svn/tokudb
# build with g++: make CC=g++
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../
INCLUDEDIRS=-I. -I../ -I$(TOKUROOT)newbrt -I../range_tree -I../lock_tree
DEPEND_COMPILE += \
	../*.h \
	../range_tree/*.h \
	../lock_tree/*.h \
	test.h \
#end

SKIP_NORETURN=1 #Do not add the -Wmissing-noreturn flag
HERE = db-benchmark-test
include $(TOKUROOT)include/Makefile.include

BENCHDBS = bench.bdb/ bench.tokudb ptest*.dir/ x.dir/ xfast.dir/

ifdef BDBDIR
BDB_CPPFLAGS = -I$(BDBDIR)/include
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb -Wl,-rpath,$(BDBDIR)/lib -lpthread
else
BDB_CPPFLAGS =
BDB_LDFLAGS = -ldb
endif
TOKUDB=libtokudb.$(SOEXT)
TDB_DLINK_FILES = $(TOKUROOT)lib/$(TOKUDB)

TARGET_BDB = db-benchmark-test-bdb$(BINSUF)
TARGET_TDB = db-benchmark-test-tokudb$(BINSUF)
SCANSCAN_TDB = scanscan-tokudb$(BINSUF)
TARGETS = $(TARGET_BDB) $(TARGET_TDB) $(SCANSCAN_TDB)

default: build
build: $(TARGETS) 
build.tdb: $(TARGET_TDB) $(SCANSCAN_TDB)

check: check-default check-xfast check-x

check-default: $(TARGET_TDB)
	$(MAYBEATSIGN)$(VGRIND) ./$(TARGET_TDB) $(QUIET) $(SUMMARIZE_CMD)

check-x: $(TARGET_TDB)
	$(MAYBEATSIGN)$(VGRIND) ./$(TARGET_TDB) $(QUIET) -x --xcount 1000 --periter 100000 --env x.dir 10 $(SUMMARIZE_CMD)


# A fast transaction test that detects #455.
check-xfast: $(TARGET_TDB)
	$(MAYBEATSIGN)./$(TARGET_TDB) $(QUIET) --noserial -x --valsize 1000  --cachesize 8000000 --xcount 1000 --periter 20000 --env xfast.dir 1  $(SUMMARIZE_CMD)


clean:
	$(MAYBEATSIGN)rm -f $(TARGETS)
	$(MAYBEATSIGN)rm -rf $(BENCHDBS)

$(TARGET_TDB) $(SCANSCAN_TDB): BIN_FROM_C_FLAGS += -DDIRSUF=tokudb

ifneq ($(PROF),)
    USE_STATIC=1
endif

# A hack to make gprof work.  See #515.
ifneq ($(USE_STATIC),)
$(TARGET_TDB) $(SCANSCAN_TDB): DLINK_FILES=

OFILES = \
	$(TOKUROOT)src/ydb.lib \
	$(TOKUROOT)src/range_tree/rangetree.lib \
	$(TOKUROOT)src/lock_tree/locktree.lib \
	$(TOKUROOT)newbrt/newbrt.lib \
	$(LIBPORTABILITY) \
#end

$(TARGET_TDB): db-benchmark-test.c
	$(MAYBEATSIGN)$(CC) $< $(BIN_FROM_C_FLAGS) $(OFILES)
$(SCANSCAN_TDB): scanscan.c
	$(MAYBEATSIGN)$(CC) $< $(BIN_FROM_C_FLAGS) $(OFILES)
else
$(TARGET_TDB) $(SCANSCAN_TDB): DLINK_FILES=$(TDB_DLINK_FILES)
ifeq ($(SOEXT),dll) #Windows does not support rpath.
$(TOKUDB): $(TOKUROOT)lib/$(TOKUDB)
	$(MAYBEATSIGN)cp $< $@
$(TOKUROOT)lib/$(TOKUDB):
	$(MAYBEATSIGN)cd $(@D) && $(MAKE) $(@F).install
$(TARGET_TDB) $(SCANSCAN_TDB): $(TOKUDB)
endif
$(TARGET_TDB) $(SCANSCAN_TDB): RPATH_DIRS=$(dir $(TDB_DLINK_FILES))
$(TARGET_TDB): db-benchmark-test.c
	$(MAYBEATSIGN)$(CC) $< $(BIN_FROM_C_FLAGS)
$(SCANSCAN_TDB): scanscan.c
	$(MAYBEATSIGN)$(CC) $< $(BIN_FROM_C_FLAGS)
endif

$(TARGET_BDB): db-benchmark-test.c
	$(CC) $(CFLAGS) $(BDB_CPPFLAGS) $(BDB_LDFLAGS) $< -o $@ -DDIRSUF=bdb

PARGS =

ptest%: $(TARGET_TDB)
	./$< --env $@.dir $(PARGS)
parallel-test-8: ptest0 ptest1 ptest2 ptest3 ptest4 ptest5 ptest6 ptest7

