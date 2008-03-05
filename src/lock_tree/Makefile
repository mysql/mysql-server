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

BINS=locktree.o rth.o lth.o

.PHONY: install logformat range_tree
install: range_tree $(BINS)

range_tree:
	cd ../range_tree && make

clean:
	cd ../range_tree && make clean
	rm -rf *.o
	rm -rf *.gcno *.gcda *.gcov
	cd tests && make clean

BINS = locktree.o rth.o lth.o

locktree.o: locktree.c locktree.h

rth.o: rth.c rth.h

lth.o: lth.c lth.h
