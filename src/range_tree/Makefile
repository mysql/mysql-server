# On OSX do:
#   make OSX=OSX

LIBNAME=librangetree

OPTFLAGS = -O2
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = -W -Wall -Werror -g -fPIC $(OPTFLAGS) $(GCOV_FLAGS)
CPPFLAGS = -I../../include -I../../newbrt
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

check: $(LIBNAME).$(LIBEXT)
	python tokuglobals.py $(LIBNAME).$(LIBEXT)

strip: $(LIBNAME).$(LIBEXT)
	strip $(LIBNAME).$(LIBEXT)

clean:
	rm -rf $(LIBNAME).$(LIBEXT) $(LIBNAME).a *.o *.gcno *.gcda *.gcov
	cd tests;make clean


linear.o: rangetree.h
DBBINS = linear.o

$(LIBNAME).$(LIBEXT): $(DBBINS)
	cc  $(CPPFLAGS) $(DBBINS) $(SHARED) -o $@ $(CFLAGS) -lz

$(LIBNAME).a: $(DBBINS)
	$(AR) rv $@ $(DBBINS)

libdb.a(ydb.o): ydb.o
