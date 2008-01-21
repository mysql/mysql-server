# On OSX do:
#   make OSX=OSX

LIBNAMELINEAR=librangetreelinear
LIBNAME=librangetree

OPTFLAGS = #-O2
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = -W -Wall -Werror -g -fPIC $(OPTFLAGS) $(GCOV_FLAGS)
CPPFLAGS = -I../../include -I../../newbrt
#CPPFLAGS += -D_GNU_SOURCE -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

ifeq ($(OSX),OSX)
LIBEXT=dylib
SHARED=-dynamiclib
CFLAGS+=-fno-common
else
LIBEXT=so
SHARED=-shared
endif

.PHONY: install logformat
install: $(LIBNAMELINEAR).$(LIBEXT) $(LIBNAMELINEAR).a #$(LIBNAME).$(LIBEXT) $(LIBNAME).a
	#cp $(LIBNAME).$(LIBEXT) ../../lib/
	#cp $(LIBNAME).a ../../lib
	cp $(LIBNAMELINEAR).$(LIBEXT) ../../lib/
	cp $(LIBNAMELINEAR).a ../../lib

clean:
	rm -rf $(LIBNAMELINEAR).$(LIBEXT) $(LIBNAMELINEAR)
	rm -rf $(LIBNAME).$(LIBEXT) $(LIBNAME).a *.o *.gcno *.gcda *.gcov
	cd tests;make clean


linear.o: linear.c rangetree.h
	gcc $(CFLAGS) $(CPPFLAGS) -DTOKU_LINEAR_RANGE_TREE -c $< -o $@

log.o: log.c rangetree.h
	gcc $(CFLAGS) $(CPPFLAGS) -DTOKU_LOG_RANGE_TREE    -c $< -o $@

$(LIBNAME).$(LIBEXT): log.o
	cc  $(CPPFLAGS) $< $(SHARED) -o $@ $(CFLAGS) -lz

$(LIBNAMELINEAR).$(LIBEXT): linear.o
	cc  $(CPPFLAGS) $< $(SHARED) -o $@ $(CFLAGS) -lz

$(LIBNAME).a: log.o
	$(AR) rv $@ $<

$(LIBNAMELINEAR).a: linear.o
	$(AR) rv $@ $<
