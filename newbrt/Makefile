# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../
INCLUDEDIRS=-I. -I../include
ifneq ($(COMBINE),0)
COMBINE=1
endif

COMPRESS = 1
ifneq ($(COMPRESS), 1)
  CFLAGS += -DTOKU_CONFIG_NO_COMPRESSION
endif

#TODO: Replace DEPEND_COMPILE with auto-dependancy generation.
DEPEND_COMPILE += \
  $(wildcard *.h) \
  log_header.h \
# keep this line so I can have a \ on the previous line

NEWBRT_SO        = $(TOKUROOT)lib/libnewbrt.$(SOEXT)

SKIP_NEWBRTRULE=1
include $(TOKUROOT)toku_include/Makefile.include
LDFLAGS+=-L$(TOKUROOT)lib -Wl,-rpath,$(shell pwd)/$(TOKUROOT)lib
LDLIBS+=-lnewbrt -ltokuportability

# When debugging, try: valgrind --show-reachable=yes --leak-check=full ./brt-test

BINS_RAW= \
  brtdump \
  tdb_logprint \
  tdb-recover \
# Intentionally left blank
# BINS will be defined automatically.

.PHONY: build default bins libs local

build default: local
	cd tests;$(MAKE) build

local: bins libs $(TEST_NEWBRT);


BRT_SOURCES = \
  block_allocator \
  block_table \
  brtloader-callback \
  brt-serialize \
  brt-verify \
  brt \
  brt_msg \
  brt-test-helpers \
  cachetable \
  checkpoint \
  dbufio \
  fifo \
  key \
  kibbutz \
  leafentry \
  le-cursor \
  logfilemgr \
  logger \
  log_code \
  log_upgrade \
  log_print \
  logcursor \
  memarena \
  minicron \
  omt \
  pqueue \
  queue \
  recover \
  roll \
  rollback \
  sort \
  sub_block \
  ule \
  threadpool \
  txn \
  workqueue \
  x1764 \
  xids \
  ybt \
# keep this line so I can have a \ on the previous line
ifneq ($(OS_CHOICE),windows)
    BRT_SOURCES += trace_mem #Windows does not handle 'empty' file
endif


TEST_NEWBRT = brt-test-helpers.$(OEXT)

BRT_C_FILES = $(patsubst %,%.c,$(BRT_SOURCES))
BRT_O_FILES = $(patsubst %,%.$(OEXT),$(BRT_SOURCES))

newbrt.$(OEXT): $(BRT_C_FILES) $(DEPEND_COMPILE)
	$(CC) -c $(BRT_C_FILES) $(COMBINE_C) $(CPPFLAGS) $(CFLAGS) $(OOUTPUT)$@

brt-serialize.$(OEXT): $(wildcard backwards_*.c)

ifneq ($(CYGWIN),)
NEWBRT_O_FILES = $(BRT_O_FILES)
else ifeq ($(CC),icc)
NEWBRT_O_FILES = $(BRT_O_FILES)
else ifeq ($(COMBINE),0)
NEWBRT_O_FILES = $(BRT_O_FILES)
else
NEWBRT_O_FILES = newbrt.o
endif

NEWBRT_O_FILES += brtloader.$(OEXT) quicklz.$(OEXT) compress.$(OEXT)

brtloader.$(OEXT): $(DEPEND_COMPILE)

$(NEWBRT_O_FILES): VISIBILITY=
$(NEWBRT_SO): DISABLE_WARNING += 10237 # Do not complain about -lcilkrts being linked in dynamically, static library not available
$(NEWBRT_SO): $(NEWBRT_O_FILES)
	echo $(patsubst %,newbrt/%,$(NEWBRT_O_FILES)) > ../lib/newbrt.olist
	$(TOKULINKER) $(SHARED) $(SYMBOLS) $(GCOV_FLAGS) $(SKIP_WARNING) $(NEWBRT_O_FILES) -o$(NEWBRT_SO) $(LINUX_NOSTDLIB) $(LCILKRTS)

log_code.$(OEXT): log_header.h wbuf.h log-internal.h rbuf.h

# This version runs logformat twice.  There is something screwing in make that if you have a pattern form with two outputs
# then it runs the thing only once, but if it has no % symbols it runs it twice.
## log_header.h log_code.c: logformat$(BINSUF)
##	./logformat
# So we do it this way
log_code.c: logformat$(BINSUF)
	./logformat .
log_print.c log_header.h: log_code.c
	test 1 = 1

#Needs to be done manually since it does not include newbrt.
logformat$(BINSUF): logformat.c $(LIBPORTABILITY_SO)
	$(CC) $< $(BIN_FROM_O_FLAGS_NOLIB) $(LDFLAGS) $(ALWAYS_LINK) $(LINK_MUST_BE_LAST) $(LIBPORTABILITY_SO)


libs: $(NEWBRT_SO)
bins: $(BINS)

# Put the benchmarktest_256 first since it takes the longest (and we want to use parallelism in the make)

# Put check_benchmarktest_256 first because it is long-running (and therefore on the critical path, so get it started)
check: bins
	cd tests;$(MAKE) check

$(BINS): $(NEWBRT_SO) $(LIBPORTABILITY_SO)

foo2:
	echo $(BINS)

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
	rm -rf log_code.c log_header.h log_print.c logformat

# After doing (cd ../src/tests;make test_log5.recover), run these.  The files should have no differences.
testdump: brtdump$(BINSUF)
	./brtdump  ../src/tests/dir.test_log5.c.tdb.recover/foo.db > dump.r && ./brtdump  ../src/tests/dir.test_log5.c.tdb/foo.db > dump.$(OEXT) && diff dump.$(OEXT) dump.r

foo:
	@echo BRTLOADER $(BRTLOADER)
	@echo BDBDIR $(BDBDIR)
