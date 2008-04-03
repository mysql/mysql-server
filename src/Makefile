# On OSX do:
#   make OSX=OSX

LIBNAME=libtokudb

EXPORTMAP = -Wl,--version-script=export.map
VISIBILITY = -fvisibility=hidden

# PROF_FLAGS=-pg
OPTFLAGS = -O2 -finline-functions
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = -W -Wall -Wextra -Werror -g3 -ggdb3 -fPIC $(OPTFLAGS) $(GCOV_FLAGS)
CFLAGS += -Wbad-function-cast -Wcast-align
CPPFLAGS = -I../include -I../newbrt -I./lock_tree/ -I./range_tree/
CPPFLAGS += -D_GNU_SOURCE -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CFLAGS+= $(VISIBILITY) $(PROF_FLAGS)


ifneq ($(OSX),)
LIBRARY=$(LIBNAME).dylib
SHARED=-dynamiclib
RPATHNAME=-install_name @rpath/$(LIBRARY)
CFLAGS+=-fno-common
else
LIBRARY=$(LIBNAME).so
SHARED=-shared $(EXPORTMAP)
RPATHNAME=
endif

build:
	cd range_tree;$(MAKE) build
	cd lock_tree;$(MAKE) build
	$(MAKE) $(LIBRARY) $(LIBNAME).a
	cd tests;$(MAKE) build
	if ! diff $(LIBNAME).a ../lib/$(LIBNAME).a >/dev/null 2>&1; then cp $(LIBNAME).a ../lib/; fi
	if ! diff $(LIBRARY)  ../lib/$(LIBRARY) > /dev/null 2>&1; then cp $(LIBRARY) ../lib/; fi

.PHONY: install
install: $(LIBRARY) $(LIBNAME).a
	cp $(LIBRARY) ../lib/
	cp $(LIBNAME).a ../lib

check_globals: $(LIBRARY)
	python tokuglobals.py $(LIBRARY)

check_tests:
	cd tests;$(MAKE) check
check: $(LIBRARY) check_globals check_tests

strip: $(LIBRARY)
	strip $(LIBRARY)

clean:
	rm -rf $(LIBRARY) $(LIBNAME).a *.o *.gcno *.gcda *.gcov
	cd tests && $(MAKE) clean
	cd lock_tree && $(MAKE) clean
	cd range_tree && $(MAKE) clean

ydb.o: ../include/db.h ../newbrt/cachetable.h ../newbrt/brt.h ../newbrt/log.c

DBBINS = ydb.o errors.o elocks.o ../newbrt/brt.o ../newbrt/brt-serialize.o ../newbrt/brt-verify.o ../newbrt/cachetable.o ../newbrt/fifo.o ../newbrt/key.o ../newbrt/memory.o ../newbrt/mempool.o ../newbrt/gpma.o ../newbrt/ybt.o ../newbrt/primes.o ../newbrt/log.o ../newbrt/fingerprint.o ../newbrt/log_code.o ../newbrt/roll.o ../newbrt/toku_assert.o ../newbrt/recover.o

RANGETREE_BINS = range_tree/rangetree.o range_tree/tokuredblack.o
LOCKTREE_BINS  = lock_tree/locktree.o lock_tree/rth.o lock_tree/lth.o lock_tree/idlth.o lock_tree/db_id.o $(RANGETREE_BINS)


$(LIBRARY): $(DBBINS) $(LOCKTREE_BINS)
	cc  $(CPPFLAGS) $^ $(SHARED) -o $@ $(CFLAGS) -lz $(RPATHNAME)

$(LIBNAME).a: $(DBBINS) $(LOCKTREE_BINS)
	$(AR) rv $@ $^

$(LIBNAME).a(ydb.o): ydb.o
