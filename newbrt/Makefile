# On cygwin do:
#   make CYGWIN=cygwin check

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
# PROF_FLAGS = -pg
OPTFLAGS = -O2

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
DTOOL = valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

CFLAGS = -Wall -W $(OPTFLAGS) -g $(GCOV_FLAGS) $(PROF_FLAGS) -Werror $(FPICFLAGS) -Wshadow
LDFLAGS = $(OPTFLAGS) -g $(GCOV_FLAGS) $(PROF_FLAGS) 
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

ifdef BRT_FANOUT
CPPFLAGS += -DBRT_FANOUT=$(BRT_FANOUT)
endif


# When debugging, try: valgrind --show-reachable=yes --leak-check=full ./brt-test


default: bins libs recover tdb_logprint
# Put these one-per-line so that if we insert a new one the svn diff can understand it better.
# Also keep them sorted.
REGRESSION_TESTS = \
        ybt-test \
        pma-test \
        brt-serialize-test \
        cachetable-test \
        cachetable-test2 \
        hashtest \
        brt-test \
	test_oexcl \
# This line intentially kept commented so I can have a \ on the end of the previous line

BINS =  $(REGRESSION_TESTS) \
        benchmark-test \
        randbrt \
        randdb4 \
        tdb_logprint \
	test_toku_malloc_plain_free \
# This line intentially kept commented so I can have a \ on the end of the previous line

tdb_logprint: LDFLAGS+=-lz
tdb_logprint.o: log-internal.h brttypes.h yerror.h log.h kv-pair.h
tdb_logprint: log_code.o memory.o log.o brt-serialize.o hashtable.o pma.o ybt.o fingerprint.o mempool.o primes.o

recover: LDFLAGS+=-lz
recover.o: log_header.h log-internal.h log.h yerror.h brttypes.h kv-pair.h memory.h key.h
recover: recover.o log_code.o memory.o log.o brt-serialize.o hashtable.o pma.o ybt.o fingerprint.o mempool.o primes.o cachetable.o brt.o brt-verify.o key.o

log_code.o: log_header.h wbuf.h log-internal.h
log_code.c log_header.h: logformat
	./logformat

libs: log.o
bins: $(BINS)
CHECKS = \
 test_oexcl \
 ybt-test \
 pma-test \
 cachetable-test \
 cachetable-test2 \
 brt-serialize-test \
 brt-test hashtest \
 test_toku_malloc_plain_free
check: bins $(patsubst %,check_%,$(CHECKS)) check_benchmarktest_256
check_benchmarktest_256: benchmark-test
	$(DTOOL) ./benchmark-test $(VERBVERBOSE) --valsize 256 --verify 1

check_%: %
	$(DTOOL)  ./$< $(VERBVERBOSE) 

check-fanout:
	let BRT_FANOUT=4; \
	while [ $$BRT_FANOUT -le 16 ] ;do \
		make clean; make check BRT_FANOUT=$$BRT_FANOUT; \
		let BRT_FANOUT=BRT_FANOUT+1; \
	done

pma-test benchmark-test brt-test brt-serialize-test: LDFLAGS+=-lz
# pma: PROF_FLAGS=-fprofile-arcs -ftest-coverage

BRT_INTERNAL_H_INCLUDES = brt-internal.h cachetable.h hashtable.h pma.h brt.h brttypes.h yerror.h ybt.h log.h ../include/db.h kv-pair.h memory.h crc.h
key.o: brttypes.h key.h
pma-test.o: $(BRT_INTERNAL_H_INCLUDES) pma-internal.h pma.h list.h mempool.h
pma-test: pma.o memory.o key.o ybt.o log.o mempool.o fingerprint.o brt-serialize.o hashtable.o primes.o log_code.o
pma.o: pma.h yerror.h pma-internal.h memory.h key.h ybt.h brttypes.h log.h ../include/db.h log_header.h
ybt.o: ybt.h brttypes.h ../include/db.h
ybt-test: ybt-test.o ybt.o memory.o
ybt-test.o: ybt.h ../include/db.h
cachetable.o: cachetable.h hashfun.h memory.h
brt-test: ybt.o brt.o hashtable.o pma.o memory.o brt-serialize.o cachetable.o ybt.o key.o primes.o log.o mempool.o brt-verify.o fingerprint.o log_code.o
log.o: log_header.h log-internal.h log.h wbuf.h crc.h brttypes.h $(BRT_INTERNAL_H_INCLUDES)
brt-test.o brt.o: brt.h ../include/db.h hashtable.h pma.h brttypes.h cachetable.h memory.h
brt-serialize-test.o: $(BRT_INTERNAL_H_INCLUDES)
brt.o: $(BRT_INTERNAL_H_INCLUDES) key.h log_header.h
hashtable.o: hashtable.h brttypes.h memory.h key.h yerror.h ../include/db.h hashfun.h
memory.o: memory.h
primes.o: primes.h
hashtest: hashtable.o memory.o primes.o
brt-serialize.o: $(BRT_INTERNAL_H_INCLUDES) key.h wbuf.h rbuf.h
brt-bigtest: memory.o ybt.o brt.o pma.o cachetable.o key.o hashtable.o brt-serialize.o
brt-bigtest.o: brt.h ../include/db.h
log-test: log.o memory.o
brt-verify.o: $(BRT_INTERNAL_H_INCLUDES)
fingerprint.o: $(BRT_INTERNAL_H_INCLUDES)

brt-serialize-test: brt-serialize-test.o brt-serialize.o memory.o hashtable.o pma.o key.o ybt.o brt.o cachetable.o primes.o log.o mempool.o brt-verify.o fingerprint.o log_code.o
test_toku_malloc_plain_free: memory.o


cachetable-test.o: cachetable.h memory.h 
cachetable-test: cachetable.o memory.o cachetable-test.o primes.o

cachetable-test2.o: cachetable.h memory.h
cachetable-test2: cachetable.o memory.o cachetable-test2.o primes.o

benchmark-test: benchmark-test.o ybt.o memory.o brt.o pma.o cachetable.o key.o hashtable.o brt-serialize.o primes.o log.o mempool.o brt-verify.o fingerprint.o log_code.o
benchmark-test.o: brt.h ../include/db.h

checko2:
ifeq ($(OPTFLAGS),-O2)
	@echo OPTFLAGS=$(OPTFLAGS) ok
else
	@echo OPTFLAGS=$(OPTFLAGS) bad; exit 1
endif

clean:
	rm -rf $(REGRESSION_TESTS) $(BINS) *.o *.bb *.bbg *.da
	rm -rf test_oexcl.c.tmp *.brt

randdb4: LOADLIBES=-ldb
randbrt: brt.o hashtable.o cachetable.o memory.o brt-serialize.o
TAGS: ../*/*.c ../*/*.h
	etags ../*/*.c ../*/*.h
