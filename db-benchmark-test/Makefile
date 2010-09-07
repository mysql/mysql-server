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

HERE = db-benchmark-test
include $(TOKUROOT)toku_include/Makefile.include

BENCHDBS = bench.bdb/ bench.tokudb ptest*.dir/ x.dir/ xfast.dir/ scanrace.tokudb/ 4g.dir/ rowsize.*.dir/
TDB_DLINK_FILES = $(TOKUROOT)lib/$(TOKUDB)

TARGET_BDB = db-benchmark-test-bdb$(BINSUF)
SCANSCAN_BDB = scanscan-bdb$(BINSUF)
TARGET_TDB = db-benchmark-test-tokudb$(BINSUF)
SCANSCAN_TDB = scanscan-tokudb$(BINSUF)
SCANRACE_TDB = scanrace-tokudb$(BINSUF)
MULTIBENCH_TDB = multi-bench-tokudb$(BINSUF)
TARGETS = $(TARGET_BDB) $(SCANSCAN_BDB) $(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB) $(MULTIBENCH_TDB)
TARGETS_BDB = $(TARGET_BDB) $(SCANSCAN_BDB)

ifeq ($(OS_CHOICE),windows)
ifdef BDBDIR
$(TARGETS_BDB): INCLUDEDIRS=-I$(BDBDIR)/include
$(TARGETS_BDB): RPATH_DIRS=$(BDBDIR)/lib
endif
ifeq ($(DEBUG),0)
WINDOWS_BDB_LIB_NAME=libdb.$(SOEXT)
$(WINDOWS_BDB_LIB_NAME):
	cp $(BDBDIR)/lib/libdb[0-9][0-9].$(SOEXT) ./
else
WINDOWS_BDB_LIB_NAME=libdbd.$(SOEXT)
$(WINDOWS_BDB_LIB_NAME):
	cp $(BDBDIR)/lib/libdb[0-9][0-9]d.$(SOEXT) ./
endif
$(TARGETS_BDB): DLINK_FILES=$(BDBDIR)/lib/$(WINDOWS_BDB_LIB_NAME)
#empty
else
WINDOWS_BDB_LIB_NAME=
#linux
ifdef BDBDIR
$(TARGETS_BDB): INCLUDEDIRS=-I$(BDBDIR)/include
$(TARGETS_BDB): RPATH_DIRS=$(BDBDIR)/lib
endif
$(TARGETS_BDB): DLINK_FILES=db.$(SOEXT)
endif
#empty on purpose

CPPFLAGS+= -D_GNU_SOURCE
TOKUDB=libtokudb.$(SOEXT)

default: build
ifeq ($(OS_CHOICE),windows)
build: build.tdb;
else
build: build.tdb build.bdb;
endif

build.bdb: $(TARGET_BDB) $(SCANSCAN_BDB) $(WINDOWS_BDB_LIB_NAME)
build.tdb: $(TARGET_TDB) $(SCANSCAN_TDB) $(MULTIBENCH_TDB)

check: check-default check-rowsize check-xfast check-x check-no-rollback check-4G child.benchmark.dir

SUPPORT_KEYSIZE=$$((3*1024)) 	# at least 3KiB
SUPPORT_ROWSIZE=$$((80*1024))  	# at least 80KiB

.PHONY: check-rowsize
check-rowsize: $(TARGET_TDB)
	( $(VGRIND) ./$(TARGET_TDB) $(VERBVERBOSE) --env rowsize.key.dir                  --keysize $(SUPPORT_KEYSIZE)                              --periter 256 1 && \
          $(VGRIND) ./$(TARGET_TDB) $(VERBVERBOSE) --env rowsize.val.dir                                 --valsize $(SUPPORT_ROWSIZE) --periter 256 1 && \
          $(VGRIND) ./$(TARGET_TDB) $(VERBVERBOSE) --env rowsize.keyval.dir               --keysize $(SUPPORT_KEYSIZE) --valsize $(SUPPORT_ROWSIZE) --periter 256 1 ) $(SUMMARIZE_CMD)

check-default: $(TARGET_TDB)
	$(VGRIND) ./$(TARGET_TDB) $(VERBVERBOSE) $(SUMMARIZE_CMD)

check-x: $(TARGET_TDB)
	$(VGRIND) ./$(TARGET_TDB) $(VERBVERBOSE) -x --xcount 1000 --periter 100000 --env x.dir 10 $(SUMMARIZE_CMD)

# check-scanrace: VGRIND=tcheck_cl -w 140 -s 4 -p nothreads,delay=2,stall=900,noverbose
check-scanrace: $(SCANRACE_TDB) $(TARGET_TDB)
	( ./$(TARGET_TDB) --env scanrace.tokudb --periter 10000 --pagesize 4096 1 && \
          $(VGRIND) ./$(SCANRACE_TDB) --env scanrace.tokudb ) $(SUMMARIZE_CMD)

# A fast transaction test that detects #455.
check-xfast: $(TARGET_TDB)
	./$(TARGET_TDB) $(VERBVERBOSE) --noserial -x --valsize 1000  --cachesize 8000000 --xcount 1000 --periter 20000 --env xfast.dir 1  $(SUMMARIZE_CMD)

# A relatively fast test that detects #853 (don't log changes to a dictionary created in the same txn)
check-no-rollback: $(TARGET_TDB)
	./$(TARGET_TDB) $(VERBVERBOSE) --env no-rollback.dir --singlex --nolog --check_small_rollback $(SUMMARIZE_CMD)

# Check to make sure that if we make a file that's bigger than 4GB that we can read the file back out and get all the rows.
ifeq ($(TOKU_SKIP_4G),1)
check-4G:
	@echo SKIPPED SLOW TEST $@
else
check-4G: $(TARGET_TDB)  $(SCANSCAN_TDB)
	( ./$(TARGET_TDB) $(VERBVERBOSE) --env 4g.dir --norandom --compressibility 1 --valsize 10000 1 && \
          ./$(SCANSCAN_TDB) --env 4g.dir --lwc --prelock --prelockflag --nox > 4g.out && \
          fgrep "(1048576 rows)" 4g.out > /dev/null ) $(SUMMARIZE_CMD)
endif

clean:
	rm -f $(TARGETS) 4g.out
	rm -rf *.dir $(BENCHDBS)

$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB) $(MULTIBENCH_TDB): BIN_FROM_C_FLAGS+=-DDIRSUF=tokudb -I$(PORTABILITY_HEADERS) -I$(TOKUROOT)toku_include -I$(TOKUROOT)include

ifneq ($(PROF),)
    USE_STATIC=1
endif

# A hack to make gprof work.  See #515.
ifneq ($(USE_STATIC),)
$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB) $(MULTIBENCH_TDB): DLINK_FILES=

OFILES = \
	$(TOKUROOT)src/ydb.lib \
	$(TOKUROOT)src/range_tree/rangetree.lib \
	$(TOKUROOT)src/lock_tree/locktree.lib \
	$(TOKUROOT)newbrt/newbrt.lib \
	$(LIBPORTABILITY) \
#end

$(TARGET_TDB): db-benchmark-test.c $(PTHREAD_LOCAL)
	$(CC) $< $(BIN_FROM_C_FLAGS) $(OFILES) $(LINK_MUST_BE_LAST)
$(SCANSCAN_TDB): scanscan.c $(PTHREAD_LOCAL)
	$(CC) $< $(BIN_FROM_C_FLAGS) $(OFILES) $(LINK_MUST_BE_LAST)
$(SCANRACE_TDB): scanrace.c $(PTHREAD_LOCAL)
	$(CC) $< $(BIN_FROM_C_FLAGS) $(OFILES) $(LINK_MUST_BE_LAST)
$(MULTIBENCH_TDB): multi-bench.c $(PTHREAD_LOCAL)
	$(CC) $< $(BIN_FROM_C_FLAGS) $(OFILES) $(LINK_MUST_BE_LAST)

else
ifeq ($(OS_CHOICE),windows)
$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB): $(WIN_YDB) $(PTHREAD_LOCAL)
$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB): LINK_FILES+=$(WIN_YDB)
else
$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB) $(MULTIBENCH_TDB): DLINK_FILES=$(TDB_DLINK_FILES)
$(TARGET_TDB) $(SCANSCAN_TDB) $(SCANRACE_TDB) $(MULTIBENCH_TDB): RPATH_DIRS=$(dir $(TDB_DLINK_FILES))
endif
$(TARGET_TDB): db-benchmark-test.c $(PTHREAD_LOCAL)
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)
$(SCANSCAN_TDB): scanscan.c $(PTHREAD_LOCAL)
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)
$(SCANRACE_TDB): scanrace.c $(PTHREAD_LOCAL)
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)
$(MULTIBENCH_TDB): multi-bench.c $(PTHREAD_LOCAL)
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)
endif

$(TARGETS_BDB): CPPFLAGS+=-DDIRSUF=bdb  -DTOKU_ALLOW_DEPRECATED
$(TARGET_BDB): db-benchmark-test.c
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)
$(SCANSCAN_BDB): scanscan.c
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)

PARGS =

ptest%: $(TARGET_TDB)
	./$< --env $@.dir $(PARGS)
parallel-test-8: ptest0 ptest1 ptest2 ptest3 ptest4 ptest5 ptest6 ptest7

#benchmarks
.PHONY: %.manybenchmark
.PHONY: %.manyflattenedscan %.flattenedscan
.PHONY: %.manyflatteningscan %.flatteningscan

BENCH_TIME=time
SCAN_TIME=time
BENCH_RUNS=5
QUIET_BENCH_ARG=

%.manybenchmark: SHELL=/bin/bash
%.manybenchmark: 
	$(BENCH_TIME) for (( i = 0; i < $(BENCH_RUNS); i++ )) ; do rm -rf $*.benchmark.dir; $(MAKE) -s $*.benchmark.dir BENCH_TIME= QUIET_BENCH_ARG=-q ; done
%.benchmark.dir: SHELL=/bin/bash
%.benchmark.dir: $(TARGET_TDB)
	rm -rf $@
	$(BENCH_TIME) ./$< --env $@ $(QUIET_BENCH_ARG) $(EXTRA_BENCH_ARGS)

DB_TYPES = no-txn txn abort child child_abort child_abortfirst txn1 abort1 child1 child-abort1 child_abortfirst1

no-txn.benchmark.dir:            EXTRA_BENCH_ARGS=
txn.benchmark.dir:               EXTRA_BENCH_ARGS= -x --singlex --prelock
abort.benchmark.dir:             EXTRA_BENCH_ARGS= -x --singlex --prelock --abort
child.benchmark.dir:             EXTRA_BENCH_ARGS= -x --singlex --prelock         --singlex-child
child-abort.benchmark.dir:       EXTRA_BENCH_ARGS= -x --singlex --prelock --abort --singlex-child
child-abortfirst.benchmark.dir:  EXTRA_BENCH_ARGS= -x --singlex --prelock --abort --singlex-child --finish-child-first
txn1.benchmark.dir:              EXTRA_BENCH_ARGS= -x --singlex --prelock                                              --insert1first
abort1.benchmark.dir:            EXTRA_BENCH_ARGS= -x --singlex --prelock --abort                                      --insert1first
child1.benchmark.dir:            EXTRA_BENCH_ARGS= -x --singlex --prelock         --singlex-child                      --insert1first
child-abort1.benchmark.dir:      EXTRA_BENCH_ARGS= -x --singlex --prelock --abort --singlex-child                      --insert1first
child-abortfirst1.benchmark.dir: EXTRA_BENCH_ARGS= -x --singlex --prelock --abort --singlex-child --finish-child-first --insert1first


QUIET_SCAN_ARG=
SCANSCAN_ARGS=--lwc --prelock --prelockflag --cachesize 268435456 # scanscan default, cache of windows (256MB)
SCANSCAN_TDB = scanscan-tokudb$(BINSUF)

%.manyflatteningscan: SHELL=/bin/bash
%.manyflatteningscan: 
	$(SCAN_TIME) for (( i = 0; i < $(BENCH_RUNS); i++ )) ; do $(MAKE) -s $*.flatteningscan SCAN_TIME= "QUIET_SCAN_ARG= >/dev/null" ; done

%.flatteningscan: QUIET_BENCH_ARG=-q
%.flatteningscan: SHELL=/bin/bash
%.flatteningscan: BENCH_TIME=
%.flatteningscan: $(SCANSCAN_TDB) %.benchmark.dir
	rm -rf $@.dir
	cp -R $*.benchmark.dir $@.dir
	$(SCAN_TIME) ./$< --env $@.dir $(SCANSCAN_ARGS) $(QUIET_SCAN_ARG)

%.manyflattenedscan: SHELL=/bin/bash
%.manyflattenedscan: %.flattenedscan.dir
	$(SCAN_TIME) for (( i = 0; i < $(BENCH_RUNS); i++ )) ; do $(MAKE) -s $*.flattenedscan SCAN_TIME= "QUIET_SCAN_ARG= >/dev/null" ; done

%.flattenedscan.dir: QUIET_BENCH_ARG=-q
%.flattenedscan.dir: QUIET_SCAN_ARGS=-q
%.flattenedscan.dir: BENCH_TIME=
%.flattenedscan.dir: $(SCANSCAN_TDB) %.benchmark.dir
	rm -rf $@
	cp -R $*.benchmark.dir $@
	./$< --env $@ $(SCANSCAN_ARGS) $(QUIET_SCAN_ARG)

%.flattenedscan: SHELL=/bin/bash
%.flattenedscan: $(SCANSCAN_TDB) %.flattenedscan.dir
	$(SCAN_TIME) ./$< --env $@.dir $(SCANSCAN_ARGS)$(QUIET_SCAN_ARG)

.SECONDARY: $(patsubst %,%.flattenedscan.dir, $(DB_TYPES))

