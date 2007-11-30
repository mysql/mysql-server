# On OSX do:
#   make OSX=OSX

LIBNAME=libdb

OPTFLAGS = -O2
CFLAGS = -W -Wall -Werror -g -fPIC $(OPTFLAGS)
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

install: $(LIBNAME).$(LIBEXT) $(LIBNAME).a
	(cd ../newbrt;make)
	cp $(LIBNAME).$(LIBEXT) ../lib/
	cp $(LIBNAME).a ../lib

check: $(LIBNAME).$(LIBEXT)
	python tokuglobals.py $(LIBNAME).$(LIBEXT)

clean:
	rm -rf $(LIBNAME).$(LIBEXT) $(LIBNAME).a *.o

ydb.o: ../include/db.h ../newbrt/cachetable.h ../newbrt/brt.h ../newbrt/log.c
DBBINS = ydb.o ../newbrt/brt.o ../newbrt/brt-serialize.o ../newbrt/brt-verify.o ../newbrt/cachetable.o ../newbrt/hashtable.o ../newbrt/key.o ../newbrt/memory.o ../newbrt/mempool.o ../newbrt/pma.o ../newbrt/ybt.o ../newbrt/primes.o ../newbrt/log.o ../newbrt/fingerprint.o ../newbrt/log_code.o

$(LIBNAME).$(LIBEXT): $(DBBINS)
	cc  $(CPPFLAGS) $(DBBINS) $(SHARED) -o $@ $(CFLAGS) -lz

$(LIBNAME).a: $(DBBINS)
	$(AR) rv $@ $(DBBINS)

libdb.a(ydb.o): ydb.o
