# On OSX do:
#   make OSX=OSX

LIBNAME=liblocktree

OPTFLAGS = #-O2
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = -W -Wall -Werror -g3 -ggdb3 -fPIC $(OPTFLAGS) $(GCOV_FLAGS)
CPPFLAGS = -I. -I.. -I../range_tree -I../../include -I../../newbrt
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
install: $(LIBNAME).$(LIBEXT) $(LIBNAME).a
	cp $(LIBNAME).$(LIBEXT) ../../lib/
	cp $(LIBNAME).a ../../lib

clean:
	rm -rf $(LIBNAMELINEAR).$(LIBEXT) $(LIBNAMELINEAR).a
	rm -rf $(LIBNAME).$(LIBEXT) $(LIBNAME).a *.o *.gcno *.gcda *.gcov
	cd tests;make clean


locktree.o: locktree.c locktree.h
	gcc $(CFLAGS) $(CPPFLAGS) -c -DTOKU_LINEAR_RANGE_TREE $< -o $@

$(LIBNAME).$(LIBEXT): locktree.o
	cc  $(CPPFLAGS) $< $(SHARED) -o $@ $(CFLAGS) -lz

$(LIBNAME).a: log.o
	$(AR) rv $@ $<
