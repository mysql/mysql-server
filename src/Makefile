CFLAGS = -W -Wall -Wno-unused -g -fPIC -O2
CPPFLAGS = -I../include -I../newbrt

install: libdb.so
	cp libdb.so ../lib/

clean:
	rm -rf *.so *.o
ydb.o: ../include/db.h ../newbrt/cachetable.h ../newbrt/brt.h
DBBINS = ydb.o ../newbrt/brt.o ../newbrt/brt-serialize.o ../newbrt/cachetable.o ../newbrt/hashtable.o ../newbrt/header-io.o ../newbrt/key.o ../newbrt/memory.o ../newbrt/pma.o ../newbrt/ybt.o ../newbrt/primes.o ../newbrt/log.o
libdb.so: $(DBBINS)
	cc  $(CPPFLAGS) $(DBBINS) -shared -o libdb.so $(CFLAGS)
libdb.a(ydb.o): ydb.o
