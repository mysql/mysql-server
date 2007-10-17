# On cygwin do:
#   make CYGWIN=cygwin check

# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
# PROF_FLAGS = -pg
OPTFLAGS = -O2

ifeq ($(CYGWIN),cygwin)
else
FPICFLAGS = -fPIC
# valgrind is not present on cygwin
DTOOL = valgrind --quiet --error-exitcode=1
endif

CFLAGS = -Wall -W $(OPTFLAGS) -g $(GCOV_FLAGS) $(PROF_FLAGS) -Werror $(FPICFLAGS)
LDFLAGS = $(OPTFLAGS) -g $(GCOV_FLAGS) $(PROF_FLAGS) 

ifdef BRT_FANOUT
CPPFLAGS += -DBRT_FANOUT=$(BRT_FANOUT)
endif


# When debugging, try: valgrind --show-reachable=yes --leak-check=full ./brt-test


default: bins libs
# Put these one-per-line so that if we insert a new one the svn diff can understand it better.
# Also keep them sorted.
REGRESSION_TESTS = \
        ybt-test \
        pma-test \
        brt-serialize-test \
        brt-test \
        cachetable-test \
        cachetable-test2 \
        hashtest \
# This line intentially kept commented so I can have a \ on the end of the previous line

BINS =  $(REGRESSION_TESTS) \
        benchmark-test \
        randbrt \
        randdb4 \
# This line intentially kept commented so I can have a \ on the end of the previous line

libs: log.o
bins: $(BINS)
check: bins
	./benchmark-test --valsize 256 --verify 1
	$(DTOOL) ./ybt-test
	$(DTOOL) ./pma-test
	$(DTOOL) ./cachetable-test
	$(DTOOL) ./cachetable-test2
	$(DTOOL) ./brt-serialize-test
	$(DTOOL) ./brt-test
	$(DTOOL) ./hashtest
#	./mdict-test

check-fanout:
	let BRT_FANOUT=4; \
	while [ $$BRT_FANOUT -le 16 ] ;do \
		make clean; make check BRT_FANOUT=$$BRT_FANOUT; \
		let BRT_FANOUT=BRT_FANOUT+1; \
	done

# pma: PROF_FLAGS=-fprofile-arcs -ftest-coverage
key.o: brttypes.h key.h
pma-test.o: pma-internal.h pma.h yerror.h memory.h  ../include/db.h list.h kv-pair.h brttypes.h ybt.h yerror.h
pma-test: pma.o memory.o key.o ybt.o log.o mempool.o
pma.o: pma.h yerror.h pma-internal.h memory.h key.h ybt.h brttypes.h  ../include/db.h
ybt.o: ybt.h brttypes.h ../include/db.h
ybt-test: ybt-test.o ybt.o memory.o
ybt-test.o: ybt.h ../include/db.h
cachetable.o: cachetable.h hashfun.h
brt-test: ybt.o brt.o hashtable.o pma.o memory.o brt-serialize.o cachetable.o header-io.o ybt.o key.o primes.o log.o mempool.o
brt-test.o brt.o: brt.h ../include/db.h hashtable.h pma.h brttypes.h cachetable.h
brt-serialize-test.o: pma.h yerror.h brt.h ../include/db.h memory.h hashtable.h brttypes.h brt-internal.h
brt.o: brt.h ../include/db.h mdict.h pma.h brttypes.h memory.h brt-internal.h cachetable.h hashtable.h
mdict.o: pma.h
hashtable.o: hashtable.h brttypes.h memory.h key.h yerror.h ../include/db.h hashfun.h
memory.o: memory.h
primes.o: primes.h
hashtest: hashtable.o memory.o primes.o
brt-serialize.o: brt.h ../include/db.h cachetable.h memory.h mdict.h pma.h brttypes.h brt-internal.h hashtable.h wbuf.h rbuf.h
header-io.o: brttypes.h brt-internal.h brt.h ../include/db.h memory.h
mdict-test: hashtable.o pma.o memory.o
brt-bigtest: memory.o ybt.o brt.o pma.o cachetable.o key.o hashtable.o brt-serialize.o
brt-bigtest.o: brt.h ../include/db.h
log-test: log.o memory.o

brt-serialize-test: brt-serialize-test.o brt-serialize.o memory.o hashtable.o pma.o key.o ybt.o brt.o cachetable.o primes.o log.o mempool.o

cachetable-test.o: cachetable.h memory.h 
cachetable-test: cachetable.o memory.o cachetable-test.o primes.o

cachetable-test2.o: cachetable.h memory.h
cachetable-test2: cachetable.o memory.o cachetable-test2.o primes.o

benchmark-test: benchmark-test.o ybt.o memory.o brt.o pma.o cachetable.o key.o hashtable.o brt-serialize.o primes.o log.o mempool.o
benchmark-test.o: brt.h ../include/db.h

clean:
	rm -rf $(REGRESSION_TESTS) $(BINS) *.o *.bb *.bbg *.da

randdb4: LOADLIBES=-ldb
randbrt: brt.o hashtable.o cachetable.o memory.o brt-serialize.o
TAGS: ../*/*.c ../*/*.h
	etags ../*/*.c ../*/*.h
