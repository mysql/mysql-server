CFLAGS = -W -Wall -Wno-unused -g -fPIC
CPPFLAGS = -I../include -I../newbrt
install: libdb.so
	cp libdb.so ../src-bdbwrap/
ydb.o: ../include/db.h ../newbrt/cachetable.h
DBBINS = ydb.o ../newbrt/brt.o ../newbrt/brt-serialize.o ../newbrt/cachetable.o ../newbrt/hashtable.o ../newbrt/header-io.o ../newbrt/key.o ../newbrt/memory.o ../newbrt/pma.o ../newbrt/ybt.o
libdb.so: $(DBBINS)
	cc  $(CPPFLAGS) $(DBBINS) -shared -o libdb.so $(CFLAGS)
libdb.a(ydb.o): ydb.o
