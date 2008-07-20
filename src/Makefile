# On OSX do:
#   make OSX=OSX

LIBNAME=libtokudb
TLIBNAME=libtokudbtrace

EXPORTMAP = -Wl,--version-script=export.map
VISIBILITY = -fvisibility=hidden

# PROF_FLAGS=-pg
OPTFLAGS = -O3 -finline-functions
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = -W -Wall -Wextra -Werror -g3 -ggdb3 -fPIC $(OPTFLAGS) $(GCOV_FLAGS)
CFLAGS += -Wbad-function-cast -Wcast-align
CPPFLAGS = -I../include -I../newbrt -I./lock_tree/ -I./range_tree/
CPPFLAGS += -D_GNU_SOURCE -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CFLAGS+= $(VISIBILITY) $(PROF_FLAGS)


ifneq ($(OSX),)
LIBRARY=$(LIBNAME).dylib
TLIBRARY=$(TLIBNAME).dylib
SHARED=-dynamiclib
RPATHNAME=-install_name @rpath/$(LIBRARY)
CFLAGS+=-fno-common
else
LIBRARY=$(LIBNAME).so
TLIBRARY=$(TLIBNAME).so
SHARED=-shared $(EXPORTMAP)
RPATHNAME=
endif

build: buildlocktrees lib
	cd tests;$(MAKE) build
	if ! diff $(LIBNAME).a ../lib/$(LIBNAME).a >/dev/null 2>&1; then cp $(LIBNAME).a ../lib/; fi
	if ! diff $(LIBRARY)  ../lib/$(LIBRARY) > /dev/null 2>&1; then cp $(LIBRARY) ../lib/; fi
	if ! diff $(TLIBNAME).a ../lib/$(TLIBNAME).a >/dev/null 2>&1; then cp $(TLIBNAME).a ../lib/; fi
	if ! diff $(TLIBRARY)  ../lib/$(TLIBRARY) > /dev/null 2>&1; then cp $(TLIBRARY) ../lib/; fi

lib: $(LIBRARY) $(LIBNAME).a $(TLIBNAME).a $(TLIBRARY)

.PHONY: buildlocktrees
buildlocktrees:
	cd range_tree;$(MAKE) build
	cd lock_tree;$(MAKE) build

local: $(LIBRARY) $(LIBNAME).a $(TLIBNAME).a $(TLIBRARY)

.PHONY: install
install: $(LIBRARY) $(LIBNAME).a $(TLIBRARY) $(TLIBNAME).a
	cp $(LIBRARY) ../lib/
	cp $(LIBNAME).a ../lib
	cp $(TLIBRARY) ../lib/
	cp $(TLIBNAME).a ../lib

check_globals: $(LIBRARY)
	python tokuglobals.py $(LIBRARY)

check_tests:
	cd tests;$(MAKE) check
check: $(LIBRARY) check_globals check_tests

strip: $(LIBRARY)
	strip $(LIBRARY)

clean:
	rm -rf $(LIBRARY) $(LIBNAME).a $(TLIBRARY) $(TLIBNAME).a *.o *.gcno *.gcda *.gcov
	cd tests && $(MAKE) clean
	cd lock_tree && $(MAKE) clean
	cd range_tree && $(MAKE) clean

ydbtrace.o ydb.o: ../include/db.h ../newbrt/cachetable.h ../newbrt/brt.h ../newbrt/log.c
ydbtrace.o tdbtrace.o: tdbtrace.h
ydbtrace.o: ydb.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -DTOKUTRACE -c -o $@ $<

DBBINS = ydb.o errors.o elocks.o ../newbrt/newbrt.o

TDBBINS = tdbtrace.o $(patsubst ydb.o,ydbtrace.o,$(DBBINS))

RANGETREE_BINS = range_tree/rangetree.o
LOCKTREE_BINS  = lock_tree/locktree.o lock_tree/rth.o lock_tree/lth.o lock_tree/idlth.o lock_tree/db_id.o $(RANGETREE_BINS)

$(LIBRARY): $(DBBINS) | buildlocktrees
	$(CC)  $(CPPFLAGS) $^ $(LOCKTREE_BINS) $(SHARED) -o $@ $(CFLAGS) $(RPATHNAME)

$(LIBNAME).a: $(DBBINS) | buildlocktrees
	$(AR) cr $@ $^  $(LOCKTREE_BINS)

$(LIBNAME).a(ydb.o): ydb.o

$(TLIBRARY): $(TDBBINS) | buildlocktrees
	$(CC)  $(CPPFLAGS) $^ $(LOCKTREE_BINS) $(SHARED) -o $@ $(CFLAGS) $(RPATHNAME)

$(TLIBNAME).a: $(TDBBINS) | buildlocktrees
	$(AR) cr $@ $^  $(LOCKTREE_BINS)

$(TLIBNAME).a(ydbtrace.o): ydbtrace.o
