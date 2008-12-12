# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../
INCLUDEDIRS=-I.
COMBINE=1
#TODO: Replace DEPEND_COMPILE with auto-dependancy generation.
DEPEND_COMPILE += \
  $(wildcard *.h) \
  log_header.h \
# keep this line so I can have a \ on the previous line

NEWBRT           = newbrt.$(AEXT)
NEWBRT_BUNDLE    = newbrt.bundle
#All executables need to statically link to newbrt
LINK_FILES += $(NEWBRT)

SKIP_NEWBRTRULE=1
include $(TOKUROOT)include/Makefile.include

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
	$(MAYBEATSIGN)cd tests;$(MAKE) build

BRT_SOURCES = \
  block_allocator \
  bread \
  brt-serialize \
  brt-verify \
  brt \
  brt-test-helpers \
  cachetable \
  fifo \
  fingerprint \
  key \
  leafentry \
  log \
  log_code \
  memarena \
  mempool \
  omt \
  recover \
  roll \
  threadpool \
  toku_assert \
  trace_mem \
  x1764 \
  ybt \
# keep this line so I can have a \ on the previous line

TEST_NEWBRT = brt-test-helpers.$(OEXT)

BRT_C_FILES = $(patsubst %,%.c,$(BRT_SOURCES))
BRT_O_FILES = $(patsubst %,%.$(OEXT),$(BRT_SOURCES))

newbrt.$(OEXT): $(BRT_C_FILES) $(DEPEND_COMPILE)
	$(MAYBEATSIGN)$(CC) $(BRT_C_FILES) $(COMBINE_C) $(O_FROM_C_FLAGS)


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

log_code.$(OEXT): log_header.h wbuf.h log-internal.h rbuf.h
log_header.h: log_code.c
	@echo generated log_code.c so log_header.c was also generated
log_code.c: logformat$(BINSUF)
	$(MAYBEATSIGN)./logformat
#Needs to be done manually since it does not include newbrt.
logformat$(BINSUF): logformat.c $(LIBPORTABILITY)
	$(MAYBEATSIGN)$(CC) $< $(BIN_FROM_O_FLAGS_NOLIB) $(LIBPORTABILITY)


libs: $(NEWBRT) $(NEWBRT_BUNDLE)
bins: $(BINS)

# Put the benchmarktest_256 first since it takes the longest (and we want to use parallelism in the make)

# Put check_benchmarktest_256 first because it is long-running (and therefore on the critical path, so get it started)
check: bins
	$(MAYBEATSIGN)cd tests;$(MAKE) check

%$(BINSUF): BIN_FROM_C_FLAGS+=$(LIBPORTABILITY)
%$(BINSUF): $(NEWBRT) $(LIBPORTABILITY)

checko2:
	@if [[ "$(OPTFLAGS)" =~ "-O([2-3])" ]] ; then \
		echo OPTFLAGS=$(OPTFLAGS) ok; \
	else \
		echo OPTFLAGS=$(OPTFLAGS) bad; exit 1; \
	fi

clean: clean-local clean-tests
clean-tests:
	$(MAYBEATSIGN)cd tests;$(MAKE) clean
clean-local:
	$(MAYBEATSIGN)rm -rf $(NEWBRT)
	$(MAYBEATSIGN)rm -rf test_oexcl.c.tmp *.brt
	$(MAYBEATSIGN)rm -rf log_code.c log_header.h logformat

# After doing (cd ../src/tests;make test_log5.recover), run these.  The files should have no differences.
testdump: brtdump$(BINSUF)
	$(MAYBEATSIGN)./brtdump  ../src/tests/dir.test_log5.c.tdb.recover/foo.db > dump.r && ./brtdump  ../src/tests/dir.test_log5.c.tdb/foo.db > dump.$(OEXT) && diff dump.$(OEXT) dump.r

