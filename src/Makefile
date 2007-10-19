# On OSX do:
#   make OSX=OSX

CFLAGS = -W -Wall -Wno-unused -g -fPIC -O2
CPPFLAGS = -I../include -I../newbrt
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE


ifeq ($(OSX),OSX)
LIBEXT=dylib
SHARED=-dynamiclib
CFLAGS+=-fno-common
else
LIBEXT=so
SHARED=-shared
endif

LIBNAME=libdb.$(LIBEXT)

install: $(LIBNAME)
	cp $(LIBNAME) ../lib/

clean:
	rm -rf *.$(LIBEXT) *.o
ydb.o: ../include/db.h ../newbrt/cachetable.h ../newbrt/brt.h ../newbrt/log.c
DBBINS = ydb.o ../newbrt/brt.o ../newbrt/brt-serialize.o ../newbrt/cachetable.o ../newbrt/hashtable.o ../newbrt/header-io.o ../newbrt/key.o ../newbrt/memory.o ../newbrt/pma.o ../newbrt/ybt.o ../newbrt/primes.o ../newbrt/log.o ../newbrt/mempool.o
$(LIBNAME): $(DBBINS)
	cc  $(CPPFLAGS) $(DBBINS) $(SHARED) -o $@ $(CFLAGS)
libdb.a(ydb.o): ydb.o
