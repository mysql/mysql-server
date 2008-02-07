# On OSX do:
#   make OSX=OSX

LIBNAME=libdb

OPTFLAGS = -O2
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = -W -Wall -Werror -g3 -ggdb3 -fPIC $(OPTFLAGS) $(GCOV_FLAGS)
CPPFLAGS = -I../include -I../newbrt -I./lock_tree/ -I./range_tree/
CPPFLAGS += -D_GNU_SOURCE -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE


ifeq ($(OSX),OSX)
LIBEXT=dylib
SHARED=-dynamiclib
CFLAGS+=-fno-common
else
LIBEXT=so
SHARED=-shared
endif

.PHONY: install logformat
install: logformat locktree $(LIBNAME).$(LIBEXT) $(LIBNAME).a
	cp $(LIBNAME).$(LIBEXT) ../lib/
	cp $(LIBNAME).a ../lib

logformat:
	(cd ../newbrt && make)

locktree:
	cd lock_tree && make

check: $(LIBNAME).$(LIBEXT)
	python tokuglobals.py $(LIBNAME).$(LIBEXT)

strip: $(LIBNAME).$(LIBEXT)
	strip $(LIBNAME).$(LIBEXT)

clean:
	rm -rf $(LIBNAME).$(LIBEXT) $(LIBNAME).a *.o *.gcno *.gcda *.gcov
	cd tests && make clean
	cd lock_tree && make clean

ydb.o: ../include/db.h ../newbrt/cachetable.h ../newbrt/brt.h ../newbrt/log.c

DBBINS = ydb.o ../newbrt/brt.o ../newbrt/brt-serialize.o ../newbrt/brt-verify.o ../newbrt/cachetable.o ../newbrt/fifo.o ../newbrt/key.o ../newbrt/memory.o ../newbrt/mempool.o ../newbrt/pma.o ../newbrt/ybt.o ../newbrt/primes.o ../newbrt/log.o ../newbrt/fingerprint.o ../newbrt/log_code.o ../newbrt/roll.o ../newbrt/toku_assert.o

RANGETREE_BINS = range_tree/linear.o
LOCKTREE_BINS  = lock_tree/locktree.o lock_tree/rth.o lock_tree/lth.o $(RANGETREE_BINS)


$(LIBNAME).$(LIBEXT): $(DBBINS) $(LOCKTREE_BINS)
	cc  $(CPPFLAGS) $^ $(SHARED) -o $@ $(CFLAGS) -lz

$(LIBNAME).a: $(DBBINS) $(LOCKTREE_BINS)
	$(AR) rv $@ $^

libdb.a(ydb.o): ydb.o
