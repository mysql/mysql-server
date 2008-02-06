# On OSX do:
#   make OSX=OSX

LIBNAME_LINEAR  = liblocktree_linear
LIBNAME_TLOG    = liblocktree_tlog
LIBNAME_LOG     = liblocktree_log

ifneq ($(OPT),)
 OPTFLAGS = -O0 -g3 -ggdb3
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
CPPFLAGS = -I. -I.. -I../range_tree -I../../include -I../../newbrt -L../range_tree
CPPFLAGS += -D_GNU_SOURCE -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE


ifneq ($(OSX),)
LIBEXT=dylib
SHARED=-dynamiclib
CFLAGS+=-fno-common
else
LIBEXT=so
SHARED=-shared
endif

.PHONY: install logformat range_tree
install: range_tree $(LIBNAME_LINEAR).$(LIBEXT) $(LIBNAME_LINEAR).a #$(LIBNAME_TLOG).$(LIBEXT) $(LIBNAME_TLOG).a $(LIBNAME_LOG).$(LIBEXT) $(LIBNAME_LOG).a
	cp $(LIBNAME_LINEAR).$(LIBEXT) ../../lib/
	cp $(LIBNAME_LINEAR).a ../../lib
	#cp $(LIBNAME_TLOG).$(LIBEXT) ../../lib/
	#cp $(LIBNAME_TLOG).a ../../lib
	#cp $(LIBNAME_LOG).$(LIBEXT) ../../lib/
	#cp $(LIBNAME_LOG).a ../../lib

range_tree:
	cd ../range_tree; make

clean:
	cd ../range_tree; make clean
	rm -rf *.o
	rm -rf $(LIBNAME_LINEAR).$(LIBEXT) $(LIBNAME_LINEAR).a
	rm -rf $(LIBNAME_TLOG).$(LIBEXT)   $(LIBNAME_TLOG).a
	rm -rf $(LIBNAME_LOG).$(LIBEXT)    $(LIBNAME_LOG).a
	rm -rf *.gcno *.gcda *.gcov
	cd tests;make clean


locktree.o: locktree.c locktree.h
	gcc $(CFLAGS) $(CPPFLAGS) -DTOKU_LT_LINEAR -c $< -o $@

rth.o: rth.c rth.h
	gcc $(CFLAGS) $(CPPFLAGS) -DTOKU_RT_LINEAR -c $< -o $@

$(LIBNAME_LINEAR).$(LIBEXT): locktree.o rth.o
	cc  $(CPPFLAGS) $^ $(SHARED) -o $@ $(CFLAGS) -lrangetree_linear

$(LIBNAME_LINEAR).a: locktree.o rth.o ../range_tree/linear.o
	$(AR) rv $@ $^

$(LIBNAME_TLOG).$(LIBEXT): locktree.o rth.o
	cc  $(CPPFLAGS) $^ $(SHARED) -o $@ $(CFLAGS) -lrangetree_tlog

$(LIBNAME_TLOG).a: locktree.o rth.o ../range_tree/tlog.o
	$(AR) rv $@ $^

$(LIBNAME_LOG).$(LIBEXT): locktree.o rth.o
	cc  $(CPPFLAGS) $^ $(SHARED) -o $@ $(CFLAGS) -lrangetree_log

$(LIBNAME_LOG).a: locktree.o rth.o ../range_tree/log.o
	$(AR) rv $@ $^

