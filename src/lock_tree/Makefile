# On OSX do:
#   make OSX=OSX

OPTFLAGS = -O2

ifneq ($(GCOV),)
 GCOV_FLAGS = -fprofile-arcs -ftest-coverage
else
 GCOV_FLAGS =
endif

VISIBILITY = -fvisibility=hidden

CFLAGS = -W -Wall -Wextra -Werror -fPIC $(OPTFLAGS)  -g3 -ggdb3 $(GCOV_FLAGS)
CFLAGS += -Wbad-function-cast -Wcast-align -Wconversion -Waggregate-return
CFLAGS += -Wmissing-noreturn -Wmissing-format-attribute
CFLAGS += $(VISIBILITY)
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
BINS=rth.o lth.o

.PHONY: install logformat range_tree
install: range_tree $(BINS) $(LT_BINS)

range_tree:
	cd ../range_tree && make

clean:
	cd ../range_tree && make clean
	rm -rf $(BINS) $(LT_BINS)
	rm -rf *.gcno *.gcda *.gcov
	cd tests && make clean

BINS = locktree.o rth.o lth.o

locktree.o: $(LT_TLOG)
	cp $< $@

locktree_global_readset.o: locktree.c locktree.h
	cc  $(CFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

locktree_no_global_readset.o: locktree.c locktree.h
	cc  $(CFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ -DTOKU_RT_NOOVERLAPS

rth.o: rth.c rth.h

lth.o: lth.c lth.h
