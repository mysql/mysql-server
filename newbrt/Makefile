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
VGRIND = valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

CFLAGS = -Wall -W -Wcast-align -Wbad-function-cast -Wextra -Wmissing-noreturn -Wmissing-format-attribute $(OPTFLAGS) -g3 -ggdb3 $(GCOV_FLAGS) $(PROF_FLAGS) -Werror $(FPICFLAGS) -Wshadow -fvisibility=hidden
LDFLAGS = $(OPTFLAGS) -g $(GCOV_FLAGS) $(PROF_FLAGS) 
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

ifdef BRT_FANOUT
CPPFLAGS += -DBRT_FANOUT=$(BRT_FANOUT)
endif


# When debugging, try: valgrind --show-reachable=yes --leak-check=full ./brt-test


build default: bins libs tdb-recover tdb_logprint
# Put these one-per-line so that if we insert a new one the svn diff can understand it better.
# Also keep them sorted.
REGRESSION_TESTS = \
	ybt-test \
	test-gpma-glassbox \
	brt-serialize-test \
	cachetable-test \
	cachetable-test2 \
	fifo-test \
	fifo-test-exp \
	test-gpma-blackbox \
	test-gpma-glassbox \
	test-brt-delete-both \
	brt-test \
	brt-test0 \
	brt-test1 \
	brt-test2 \
	brt-test3 \
	brt-test4 \
	brt-test-named-db \
	brt-test-cursor \
	brt-test-cursor-2 \
        log-test \
	log-test2 \
	log-test3 \
	log-test4 \
	log-test5 \
	log-test6 \
	test_oexcl \
	test-assert \
	test-primes \
	list-test \
	test-inc-split \
	test-del-inorder \
# This line intentially kept commented so I can have a \ on the end of the previous line

BINS =  $(REGRESSION_TESTS) \
        benchmark-test \
        brtdump \
        randbrt \
        randdb4 \
        tdb_logprint \
	test_toku_malloc_plain_free \
# This line intentially kept commented so I can have a \ on the end of the previous line

tdb_logprint: LDFLAGS+=-lz
tdb_logprint.o: log-internal.h brttypes.h yerror.h log.h kv-pair.h
tdb_logprint: log_code.o memory.o log.o brt-serialize.o fifo.o gpma.o ybt.o fingerprint.o mempool.o primes.o toku_assert.o roll.o brt.o cachetable.o brt-verify.o key.o

tdb-recover: LDFLAGS+=-lz
recover.o: log_header.h log-internal.h log.h yerror.h brttypes.h kv-pair.h memory.h key.h
tdb-recover: tdb-recover.o recover.o log_code.o memory.o log.o brt-serialize.o fifo.o gpma.o ybt.o fingerprint.o mempool.o primes.o toku_assert.o cachetable.o brt.o brt-verify.o key.o roll.o

roll.o: log_header.h log-internal.h log.h yerror.h brttypes.h kv-pair.h memory.h key.h cachetable.h gpma.h

log_code.o: log_header.h wbuf.h log-internal.h
log_header.h: log_code.c
	@echo generated log_code.c so log_header.c was also generated
log_code.c: logformat
	./logformat

libs: log.o
bins: $(BINS)
CHECKS = \
 test_oexcl \
 ybt-test \
 cachetable-test \
 cachetable-test2 \
 brt-serialize-test \
 test-brt-delete-both \
 brt-test \
 brt-test-cursor \
 brt-test-cursor-2 \
 brt-test0 \
 brt-test1 \
 brt-test2 \
 brt-test3 \
 brt-test4 \
 brt-test-named-db \
 fifo-test \
 test_toku_malloc_plain_free \
 test-primes \
 list-test \
# This line intentially kept commented so I can have a \ on the previous line

# Put check_benchmarktest_256 first because it is long-running (and therefore on the critical path, so get it started)
check: bins check_benchmarktest_256 $(patsubst %,check_%,$(CHECKS))
check_benchmarktest_256: benchmark-test
	$(VGRIND) ./benchmark-test $(VERBVERBOSE) --valsize 256 --verify 1

check_test-assert: test-assert
	@# no arguments, should err
	$(VGRIND) ./test-assert > /dev/null 2>&1 ; test \($$?\)
	@# one argument, not "ok" should err
	@echo Expect an abort message:
	($(VGRIND) ./test-assert notok) > test-assert.out 2>&1 ; test \($$?\)
	@fgrep failed test-assert.out > /dev/null
	@rm test-assert.out
	@# one argument, "ok" should not error
	$(DVGRIND) ./test-assert ok
check_%: %
	$(VGRIND)  ./$< $(VERBVERBOSE) 

check-fanout:
	let BRT_FANOUT=4; \
	while [ $$BRT_FANOUT -le 16 ] ;do \
		make clean; make check BRT_FANOUT=$$BRT_FANOUT; \
		let BRT_FANOUT=BRT_FANOUT+1; \
	done

log-test log-test2 log-test3 log-test4 log-test5 log-test6 benchmark-test brt-test brt-test0 brt-test1 brt-test2 brt-test3 brt-test4 brt-test-named-db brt-test-cursor brt-test-cursor-2 test-brt-delete-both brt-serialize-test brtdump test-inc-split test-del-inorder: LDFLAGS+=-lz
# pma: PROF_FLAGS=-fprofile-arcs -ftest-coverage

BRT_INTERNAL_H_INCLUDES = brt-internal.h cachetable.h fifo.h gpma.h brt.h brt-search.h brttypes.h yerror.h ybt.h log.h ../include/db.h kv-pair.h memory.h crc.h mempool.h
key.o: brttypes.h key.h
list-test: list-test.o toku_assert.o
test-brt-delete-both: ybt.o brt.o fifo.o gpma.o memory.o brt-serialize.o cachetable.o ybt.o key.o primes.o toku_assert.o log.o mempool.o brt-verify.o fingerprint.o log_code.o roll.o
test-inc-split: test-inc-split.o brt.o brt-test-helpers.o toku_assert.o key.o ybt.o cachetable.o memory.o log.o gpma.o log_code.o fifo.o fingerprint.o brt-serialize.o brt-verify.o roll.o primes.o mempool.o
test-del-inorder: test-del-inorder.o brt.o brt-test-helpers.o toku_assert.o key.o ybt.o cachetable.o memory.o log.o gpma.o log_code.o fifo.o fingerprint.o brt-serialize.o brt-verify.o roll.o primes.o mempool.o
# pma-test.o: $(BRT_INTERNAL_H_INCLUDES) pma-internal.h gpma.h list.h mempool.h
# pma-test: pma.o memory.o key.o ybt.o log.o mempool.o fingerprint.o brt-serialize.o fifo.o primes.o toku_assert.o log_code.o  roll.o brt.o cachetable.o brt-verify.o
pma.o: gpma.h yerror.h pma-internal.h memory.h key.h ybt.h brttypes.h log.h ../include/db.h log_header.h
test-gpma-glassbox.o: test-gpma-glassbox.c gpma.h gpma-internal.h toku_assert.h memory.h
test-gpma-glassbox: test-gpma-glassbox.o toku_assert.o memory-debug.o gpma.o
test-gpma-blackbox: test-gpma-blackbox.o toku_assert.o memory.o gpma.o
test-gpma-blackbox.o: test-gpma-blackbox.c gpma.h memory.h toku_assert.h
gpma.o: gpma.c gpma.h
ybt.o: ybt.h brttypes.h ../include/db.h
ybt-test: ybt-test.o ybt.o memory.o toku_assert.o
ybt-test.o: ybt.h ../include/db.h
cachetable.o: cachetable.h hashfun.h memory.h
brt-test0 brt-test1 brt-test2 brt-test3 brt-test4 brt-test-named-db brt-test-cursor brt-test-cursor-2 brt-test: ybt.o brt.o fifo.o gpma.o memory.o brt-serialize.o cachetable.o ybt.o key.o primes.o toku_assert.o log.o mempool.o brt-verify.o fingerprint.o log_code.o roll.o
log.o: log_header.h log-internal.h log.h wbuf.h crc.h brttypes.h $(BRT_INTERNAL_H_INCLUDES)
logformat: logformat.o toku_assert.o
brt-test0.o brt-test1.o brt-test2.o brt-test3.o brt-test4.o brt-test-named-db.o brt-test-cursor.o brt-test-cursor-2.o brt-test.o brt.o: brt.h brt-search.h ../include/db.h fifo.h gpma.h brttypes.h cachetable.h memory.h
brt-serialize-test.o: $(BRT_INTERNAL_H_INCLUDES)
brt.o: $(BRT_INTERNAL_H_INCLUDES) key.h log_header.h
fifo.o: fifo.h brttypes.h 
memory.o: memory.h
primes.o: primes.h toku_assert.h
fifo-test-exp fifo-test: fifo.o memory.o toku_assert.o ybt.o
brt-serialize.o: $(BRT_INTERNAL_H_INCLUDES) key.h wbuf.h rbuf.h
brt-bigtest: memory.o ybt.o brt.o gpma.o cachetable.o key.o fifo.o brt-serialize.o
brt-bigtest.o: brt.h brt-search.h ../include/db.h
log-test6 log-test5 log-test4 log-test3 log-test2 log-test: log.o memory.o toku_assert.o roll.o log_code.o brt-serialize.o brt.o cachetable.o gpma.o ybt.o fifo.o key.o fingerprint.o brt-verify.o mempool.o primes.o
brt-verify.o: $(BRT_INTERNAL_H_INCLUDES)
fingerprint.o: $(BRT_INTERNAL_H_INCLUDES)
mempool.o: toku_assert.h mempool.h
toku_assert.o: toku_assert.h

brt-serialize-test: brt-serialize-test.o brt-serialize.o memory.o fifo.o gpma.o key.o ybt.o brt.o cachetable.o primes.o toku_assert.o log.o mempool.o brt-verify.o fingerprint.o log_code.o roll.o
test_toku_malloc_plain_free: memory.o toku_assert.o


cachetable-test.o: cachetable.h memory.h 
cachetable-test: cachetable.o memory.o cachetable-test.o primes.o toku_assert.o

cachetable-test2.o: cachetable.h memory.h
cachetable-test2: cachetable.o memory.o cachetable-test2.o primes.o toku_assert.o

benchmark-test: benchmark-test.o ybt.o memory.o brt.o gpma.o cachetable.o key.o fifo.o brt-serialize.o primes.o toku_assert.o log.o mempool.o brt-verify.o fingerprint.o log_code.o roll.o
benchmark-test.o: brt.h brt-search.h ../include/db.h

test-primes: test-primes.o toku_assert.o primes.o toku_assert.o toku_assert.o

test-assert: test-assert.o toku_assert.o

brtdump: brtdump.o brt-serialize.o memory.o gpma.o key.o fingerprint.o log.o log_code.o roll.o cachetable.o primes.o toku_assert.o brt.o ybt.o fifo.o mempool.o brt-verify.o

test_oexcl: test_oexcl.o toku_assert.o


checko2:
ifeq ($(OPTFLAGS),-O2)
	@echo OPTFLAGS=$(OPTFLAGS) ok
else
	@echo OPTFLAGS=$(OPTFLAGS) bad; exit 1
endif

clean:
	rm -rf $(REGRESSION_TESTS) $(BINS) *.o *.bb *.bbg *.da *.gcov *.gcno *.gcda
	rm -rf test_oexcl.c.tmp *.brt

randdb4: LOADLIBES=-ldb
randdb4: randdb4.o toku_assert.o
randbrt: brt.o fifo.o cachetable.o memory.o brt-serialize.o

# After doing (cd ../src/tests;make test_log5.recover), run these.  The files should have no differences.
testdump: brtdump
	./brtdump  ../src/tests/dir.test_log5.c.tdb.recover/foo.db > dump.r && ./brtdump  ../src/tests/dir.test_log5.c.tdb/foo.db > dump.o && diff dump.o dump.r

TAGS: ../*/*.c ../*/*.h
	etags ../*/*.c ../*/*.h
