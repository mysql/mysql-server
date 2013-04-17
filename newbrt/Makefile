# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../
INCLUDEDIRS=-I. -I../include
COMBINE=1
#TODO: Replace DEPEND_COMPILE with auto-dependancy generation.
DEPEND_COMPILE += \
  $(wildcard *.h) \
  log_header.h \
# keep this line so I can have a \ on the previous line

NEWBRT           = newbrt.$(AEXT)
IPO_NEWBRT       = ipo_newbrt.$(AEXT)
NEWBRT_BUNDLE    = newbrt.bundle
#All executables need to statically link to newbrt
LINK_FILES += $(NEWBRT)

SKIP_NEWBRTRULE=1
include $(TOKUROOT)toku_include/Makefile.include

# When debugging, try: valgrind --show-reachable=yes --leak-check=full ./brt-test

BINS_RAW= \
  brtdump \
  tdb_logprint \
  tdb-recover \
# Intentionally left blank
# BINS will be defined automatically.

BINS_O = $(patsubst %,%.$(OEXT),$(BINS_RAW))

.PHONY: build default bins libs

build default: bins libs $(TEST_NEWBRT)
	cd tests;$(MAKE) build

BRT_SOURCES = \
  block_allocator \
  block_table \
  bread \
  brt-serialize \
  brt-verify \
  brt \
  brt_msg \
  brt-test-helpers \
  cachetable \
  checkpoint \
  fifo \
  fifo_msg \
  fingerprint \
  key \
  leafentry \
  leaflock \
  logfilemgr \
  logger \
  log_code \
  logcursor \
  memarena \
  mempool \
  minicron \
  omt \
  recover \
  roll \
  rollback \
  ule \
  threadpool \
  toku_worker \
  trace_mem \
  txn \
  x1764 \
  xids \
  ybt \
# keep this line so I can have a \ on the previous line

TEST_NEWBRT = brt-test-helpers.$(OEXT)

BRT_C_FILES = $(patsubst %,%.c,$(BRT_SOURCES))
BRT_O_FILES = $(patsubst %,%.$(OEXT),$(BRT_SOURCES))

newbrt.$(OEXT): $(BRT_C_FILES) $(DEPEND_COMPILE)
	$(CC) -c $(BRT_C_FILES) $(COMBINE_C) $(CPPFLAGS) $(CFLAGS) $(OOUTPUT)$@


ifneq ($(CYGWIN),)
NEWBRT_O_FILES = $(BRT_O_FILES)
else ifeq ($(CC),icc)
NEWBRT_O_FILES = $(BRT_O_FILES)
else ifeq ($(COMBINE),0)
NEWBRT_O_FILES = $(BRT_O_FILES)
else
NEWBRT_O_FILES = newbrt.o
endif

$(NEWBRT): $(NEWBRT_O_FILES)

$(NEWBRT_BUNDLE): log_code.c log_header.h

ipo_newbrt.obj: $(NEWBRT_BUNDLE)
	$(CC) $(CFLAGS) $(CPPFLAGS) -Qipo-c $(filter %.$(OEXT),$^) $(patsubst %.bundle, %.bundle/*.$(OEXT), $(filter-out %.$(OEXT),$^))
	mv ipo_out.obj $@

$(IPO_NEWBRT): ipo_newbrt.obj
	xilib /out:$@ $^

log_code.$(OEXT): log_header.h wbuf.h log-internal.h rbuf.h

# This version runs logformat twice.  There is something screwing in make that if you have a pattern form with two outputs
# then it runs the thing only once, but if it has no % symbols it runs it twice.
## log_header.h log_code.c: logformat$(BINSUF)
##	./logformat
# So we do it this way
log_code.c: logformat$(BINSUF)
	./logformat
log_header.h: log_code.c
	test 1 = 1

#Needs to be done manually since it does not include newbrt.
logformat$(BINSUF): logformat.c $(LIBPORTABILITY)
	$(CC) $< $(BIN_FROM_O_FLAGS_NOLIB) $(LIBPORTABILITY) $(LINK_MUST_BE_LAST)


libs: $(NEWBRT) $(NEWBRT_BUNDLE)
bins: $(BINS)

# Put the benchmarktest_256 first since it takes the longest (and we want to use parallelism in the make)

# Put check_benchmarktest_256 first because it is long-running (and therefore on the critical path, so get it started)
check: bins
	cd tests;$(MAKE) check

%$(BINSUF): $(NEWBRT) $(LIBPORTABILITY)

checko2: SHELL=/bin/bash
checko2:
	@shopt -s compat31; if [[ "$(OPTFLAGS)" =~ "-O([2-3x])" ]] ; then \
		echo OPTFLAGS=$(OPTFLAGS) ok; \
	else \
		echo OPTFLAGS=$(OPTFLAGS) bad; exit 1; \
	fi

clean: clean-local clean-tests
clean-tests:
	cd tests;$(MAKE) clean
clean-local:
	rm -rf $(NEWBRT)
	rm -rf test_oexcl.c.tmp *.brt
	rm -rf log_code.c log_header.h logformat

# After doing (cd ../src/tests;make test_log5.recover), run these.  The files should have no differences.
testdump: brtdump$(BINSUF)
	./brtdump  ../src/tests/dir.test_log5.c.tdb.recover/foo.db > dump.r && ./brtdump  ../src/tests/dir.test_log5.c.tdb/foo.db > dump.$(OEXT) && diff dump.$(OEXT) dump.r

