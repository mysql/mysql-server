# On OSX do:
#   make OSX=OSX

OPTFLAGS = -O3 -finline-functions

ifneq ($(GCOV),)
 GCOV_FLAGS = -fprofile-arcs -ftest-coverage
else
 GCOV_FLAGS =
endif

VISIBILITY = -fvisibility=hidden

CFLAGS = -W -Wall -Wextra -Werror -fPIC $(OPTFLAGS)  -g3 -ggdb3 $(GCOV_FLAGS)
CFLAGS += -Wbad-function-cast -Wcast-align -Wconversion -Waggregate-return
CFLAGS += -Wmissing-noreturn -Wmissing-format-attribute
CFLAGS += $(VISIBILITY) $(PROF_FLAGS)
CPPFLAGS = -I. -I.. -I../range_tree -I../../include -I../../newbrt -L../range_tree
CPPFLAGS += -D_GNU_SOURCE -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE


ifneq ($(OSX),)
CFLAGS+=-fno-common
endif

LT_OVERLAP     = locktree_global_readset.o
LT_NOOVERLAP   = locktree_no_global_readset.o
LT_LINEAR      = $(LT_OVERLAP)
LT_TLINEAR     = $(LT_NOOVERLAP)
LT_TLOG        = $(LT_NOOVERLAP)
LT_LOG         = $(LT_OVERLAP)

LT_BINS=$(LT_OVERLAP) $(LT_NOOVERLAP) locktree.o
BINS=rth.o lth.o idlth.o db_id.o

.PHONY: install logformat

build: $(LT_BINS) $(BINS)
	cd tests; $(MAKE) build

check:
	cd tests; $(MAKE) check

clean:
	rm -rf $(BINS) $(LT_BINS)
	rm -rf *.gcno *.gcda *.gcov
	cd tests && $(MAKE) clean

locktree.o: $(LT_TLOG)
	cp $< $@

locktree_global_readset.o: locktree.c locktree.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

locktree_no_global_readset.o: locktree.c locktree.h
	$(CC)  $(CFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ -DTOKU_RT_NOOVERLAPS

rth.o: rth.c rth.h

lth.o: lth.c lth.h
