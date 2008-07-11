# On cygwin do:
#   make CYGWIN=cygwin check

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
# PROF_FLAGS = -pg
OPTFLAGS = -O3

ifeq ($(VERBOSE),2)
VERBVERBOSE=-v
MAYBEATSIGN= 
else
 ifeq ($(VERBOSE),1)
  VERBVERBOSE=-q
  MAYBEATSIGN=
 else
  VERBVERBOSE=-q
  MAYBEATSIGN=@
 endif
endif

ifeq ($(CYGWIN),cygwin)
else
FPICFLAGS = -fPIC
# valgrind is not present on cygwin
VGRIND = valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

CFLAGS = -Wall -W -Wcast-align -Wbad-function-cast -Wextra -Wmissing-noreturn -Wmissing-format-attribute $(OPTFLAGS) -g3 -ggdb3 $(GCOV_FLAGS) $(PROF_FLAGS) -Werror $(FPICFLAGS) -Wshadow -fvisibility=hidden
LDFLAGS = $(OPTFLAGS) -g $(GCOV_FLAGS) $(PROF_FLAGS) -lz
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_XOPEN_SOURCE=500

# Add -Wconversion

ifdef BRT_FANOUT
CPPFLAGS += -DBRT_FANOUT=$(BRT_FANOUT)
endif


# When debugging, try: valgrind --show-reachable=yes --leak-check=full ./brt-test

BINS= brtdump \
      tdb_logprint \
# Intentionally left blank

build default: bins libs tdb-recover tdb_logprint $(TEST_OFILES)
	cd tests;$(MAKE) build

# Put crc first to make -combine work right
BRT_SOURCES = \
  crc \
  bread \
  brt-serialize \
  brt-verify \
  brt \
  cachetable \
  fifo \
  fingerprint \
  key \
  leafentry \
  log \
  log_code \
  memory \
  memarena \
  mempool \
  omt \
  recover \
  roll \
  toku_assert \
  ybt \
# keep this line so I can ha vea \ on the previous line

OFILES = newbrt.o
TEST_OFILES = brt-test-helpers.o

HFILES = $(wildcard *.h)

BRT_C_FILES = $(patsubst %,%.c,$(BRT_SOURCES))

newbrt.o: $(BRT_C_FILES) $(HFILES)
	$(CC) -combine $(CFLAGS) $(CPPFLAGS) $(BRT_C_FILES) -c -o $@

tdb_logprint.o: log-internal.h brttypes.h log.h kv-pair.h log_header.h
tdb_logprint: $(OFILES)

recover.o: log_header.h log-internal.h log.h brttypes.h kv-pair.h memory.h key.h cachetable.h
tdb-recover: $(OFILES)

roll.o: log_header.h log-internal.h log.h brttypes.h kv-pair.h memory.h key.h cachetable.h omt.h bread.h

log_code.o: log_header.h wbuf.h log-internal.h rbuf.h
log_header.h: log_code.c
	@echo generated log_code.c so log_header.c was also generated
log_code.c: logformat
	./logformat

libs: newbrt.o
bins: $(BINS)
# Put the benchmarktest_256 first since it takes the longest (and we want to use parallelism in the make)

# Put check_benchmarktest_256 first because it is long-running (and therefore on the critical path, so get it started)
check: bins
	cd tests;$(MAKE) check

check-fanout:
	let BRT_FANOUT=4; \
	while [ $$BRT_FANOUT -le 16 ] ;do \
		make clean; make check BRT_FANOUT=$$BRT_FANOUT; \
		let BRT_FANOUT=BRT_FANOUT+1; \
	done

BRT_INTERNAL_H_INCLUDES = brt-internal.h cachetable.h fifo.h omt.h brt.h brt-search.h brttypes.h ybt.h log.h ../include/db.h kv-pair.h memory.h crc.h mempool.h leafentry.h
brt-test-helpers.o: $(BRT_INTERNAL_H_INCLUDES) toku_assert.h
logformat: logformat.c toku_assert.c
brt-serialize-test.o: $(BRT_INTERNAL_H_INCLUDES)
fifo-test: newbrt.o
log-test6 log-test5 log-test4 log-test3 log-test2 log-test: $(OFILES)
omt-test.o: toku_assert.h memory.h toku_assert.h ../include/db.h brttypes.h
omt-test: omt-test.o newbrt.o

test_toku_malloc_plain_free: newbrt.o


cachetable-test.o: cachetable.h memory.h 
cachetable-test: $(OFILES)

cachetable-test2.o: cachetable.h memory.h
cachetable-test2: $(OFILES)

test-assert: newbrt.o

brtdump: $(OFILES)

test_oexcl: test_oexcl.o newbrt.o


checko2:
ifeq ($(OPTFLAGS),-O3)
	@echo OPTFLAGS=$(OPTFLAGS) ok
else
	@echo OPTFLAGS=$(OPTFLAGS) bad; exit 1
endif

clean: clean-local clean-tests
clean-tests:
	cd tests;$(MAKE) clean
clean-local:
	rm -rf $(BINS) *.o *.bb *.bbg *.da *.gcov *.gcno *.gcda
	rm -rf test_oexcl.c.tmp *.brt

# After doing (cd ../src/tests;make test_log5.recover), run these.  The files should have no differences.
testdump: brtdump
	./brtdump  ../src/tests/dir.test_log5.c.tdb.recover/foo.db > dump.r && ./brtdump  ../src/tests/dir.test_log5.c.tdb/foo.db > dump.o && diff dump.o dump.r

TAGS: ../*/*.c ../*/*.h
	etags ../*/*.c ../*/*.h
