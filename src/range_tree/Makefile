# On OSX do:
#   make OSX=OSX

LIBNAME_LINEAR = librangetree_linear
LIBNAME_TLOG   = librangetree_tlog
LIBNAME_LOG    = librangetree_log

ifneq ($(OPT),)
 OPTFLAGS = -O3
else
 OPTFLAGS = -O2 -g3 -ggdb3
endif

ifneq ($(GCOV),)
 GCOV_FLAGS = -fprofile-arcs -ftest-coverage
else
 GCOV_FLAGS =
endif

CFLAGS = -W -Wall -Wextra -Werror -fPIC $(OPTFLAGS) $(GCOV_FLAGS)
CFLAGS += -Wbad-function-cast -Wcast-align -Wconversion -Waggregate-return
CFLAGS += -Wmissing-noreturn -Wmissing-format-attribute
CPPFLAGS = -I../../include -I../../newbrt
CPPFLAGS += -D_GNU_SOURCE -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE



ifneq ($(OSX),)
LIBEXT=dylib
SHARED=-dynamiclib
CFLAGS+=-fno-common
else
LIBEXT=so
SHARED=-shared
endif

.PHONY: install logformat
install: $(LIBNAME_LINEAR).$(LIBEXT) $(LIBNAME_LINEAR).a #$(LIBNAME_TLOG).$(LIBEXT) $(LIBNAME_TLOG).a $(LIBNAME_LOG).$(LIBEXT) $(LIBNAME_LOG).a
	#cp $(LIBNAME_LOG).$(LIBEXT) ../../lib/
	#cp $(LIBNAME_LOG).a ../../lib
	#cp $(LIBNAME_TLOG).$(LIBEXT) ../../lib/
	#cp $(LIBNAME_TLOG).a ../../lib
	cp $(LIBNAME_LINEAR).$(LIBEXT) ../../lib/
	cp $(LIBNAME_LINEAR).a ../../lib

clean:
	rm -rf $(LIBNAME_LINEAR).$(LIBEXT) $(LIBNAME_LINEAR).a
	rm -rf $(LIBNAME_TLOG).$(LIBEXT)   $(LIBNAME_TLOG).a
	rm -rf $(LIBNAME_LOG).$(LIBEXT)    $(LIBNAME_LOG).a
	cd tests;make clean


linear.o: linear.c rangetree.h
	gcc $(CFLAGS) $(CPPFLAGS) -DTOKU_RT_LINEAR -c $< -o $@

log.o: log.c rangetree.h
	gcc $(CFLAGS) $(CPPFLAGS) -DTOKU_RT_LOG    -c $< -o $@

tlog.o: tlog.c rangetree.h
	gcc $(CFLAGS) $(CPPFLAGS) -DTOKU_RT_TLOG    -c $< -o $@

$(LIBNAME_TLOG).$(LIBEXT): tlog.o
	cc  $(CPPFLAGS) $< $(SHARED) -o $@ $(CFLAGS)

$(LIBNAME_LOG).$(LIBEXT): log.o
	cc  $(CPPFLAGS) $< $(SHARED) -o $@ $(CFLAGS)

$(LIBNAME_LINEAR).$(LIBEXT): linear.o
	cc  $(CPPFLAGS) $< $(SHARED) -o $@ $(CFLAGS)

$(LIBNAME_TLOG).a: tlog.o
	$(AR) rv $@ $<

$(LIBNAME_LOG).a: log.o
	$(AR) rv $@ $<

$(LIBNAME_LINEAR).a: linear.o
	$(AR) rv $@ $<
