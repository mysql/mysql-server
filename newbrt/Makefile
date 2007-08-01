# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
# PROF_FLAGS = -pg
OPTFLAGS = -O2
CFLAGS = -Wall -W $(OPTFLAGS) -g $(GCOV_FLAGS) $(PROF_FLAGS) -Werror -fPIC
LDFLAGS = $(OPTFLAGS) -g $(GCOV_FLAGS) $(PROF_FLAGS) 
DTOOL = valgrind --quiet --error-exitcode=1

# When debugging, try: valgrind --show-reachable=yes --leak-check=full ./brt-test


default: bins
BINS =  pma-test brt-test cachetable-test brt-serialize-test randbrt randdb4 hashtest ybt-test
bins: $(BINS)
check: bins
	$(DTOOL) ./ybt-test
	$(DTOOL) ./pma-test
	$(DTOOL) ./cachetable-test
	$(DTOOL) ./brt-serialize-test
	$(DTOOL) ./brt-test
	$(DTOOL) ./hashtest
#	./mdict-test

# pma: PROF_FLAGS=-fprofile-arcs -ftest-coverage
key.o: brttypes.h key.h
pma-test.o: pma-internal.h pma.h yerror.h memory.h  ../include/ydb-constants.h
pma-test: pma.o memory.o key.o ybt.o
pma.o: pma.h yerror.h pma-internal.h memory.h key.h ybt.h brttypes.h  ../include/ydb-constants.h
ybt.o: ybt.h brttypes.h
ybt-test: ybt-test.o ybt.o memory.o
cachetable.o: cachetable.h
brt-test: ybt.o brt.o hashtable.o pma.o memory.o brt-serialize.o cachetable.o header-io.o ybt.o key.o
brt-test.o brt.o: brt.h hashtable.h pma.h brttypes.h cachetable.h
brt-serialize-test.o: pma.h yerror.h brt.h memory.h hashtable.h brttypes.h brt-internal.h
brt.o: brt.h mdict.h pma.h brttypes.h memory.h brt-internal.h cachetable.h hashtable.h
mdict.o: pma.h
hashtable.o: hashtable.h brttypes.h memory.h key.h yerror.h ../include/ydb-constants.h
memory.o: memory.h
hashtest: hashtable.o memory.o
brt-serialize.o: brt.h cachetable.h memory.h mdict.h pma.h brttypes.h brt-internal.h hashtable.h
header-io.o: brttypes.h brt-internal.h memory.h
mdict-test: hashtable.o pma.o memory.o
brt-bigtest: memory.o ybt.o brt.o pma.o cachetable.o key.o hashtable.o brt-serialize.o

brt-serialize-test: brt-serialize-test.o brt-serialize.o memory.o hashtable.o pma.o key.o ybt.o brt.o cachetable.o

cachetable-test.o: cachetable.h memory.h
cachetable-test: cachetable.o memory.o cachetable-test.o

benchmark-test: benchmark-test.o ybt.o memory.o brt.o pma.o cachetable.o key.o hashtable.o brt-serialize.o

clean:
	rm -rf *.o hashtest brt-test cachetable-test randbrt randdb4 benchmark-test *.bb *.bbg *.da

randdb4: LOADLIBES=-ldb
randbrt: brt.o hashtable.o cachetable.o memory.o brt-serialize.o
TAGS: ../*/*.c ../*/*.h
	etags ../*/*.c ../*/*.h
