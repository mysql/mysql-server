# On OSX do:
#   make OSX=OSX

LIBNAME=liblocktree

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

.PHONY: install logformat
install: $(LIBNAME).$(LIBEXT) $(LIBNAME).a
	cp $(LIBNAME).$(LIBEXT) ../../lib/
	cp $(LIBNAME).a ../../lib

clean:
	rm -rf $(LIBNAME_LINEAR).$(LIBEXT) $(LIBNAME_LINEAR).a
	rm -rf $(LIBNAME_TLOG).$(LIBEXT)   $(LIBNAME_TLOG).a
	rm -rf $(LIBNAME_LOG).$(LIBEXT)    $(LIBNAME_LOG).a
	rm -rf *.gcno *.gcda *.gcov
	cd tests;make clean


locktree.o: locktree.c locktree.h
	gcc $(CFLAGS) $(CPPFLAGS) -DTOKU_LT_LINEAR -c $< -o $@

rth.o: rth.c rth.h
	gcc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LIBNAME).$(LIBEXT): locktree.o rth.o
	cc  $(CPPFLAGS) $^ $(SHARED) -o $@ $(CFLAGS) -lrangetree_linear

$(LIBNAME).a: locktree.o rth.o
	$(AR) rv $@ $^
