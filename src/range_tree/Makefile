# On OSX do:
#   make OSX=OSX

OPTFLAGS = -O3 -finline-functions

LIBNAME=libtokurange

VISIBILITY = #-fvisibility=hidden

ifneq ($(GCOV),)
 GCOV_FLAGS = -fprofile-arcs -ftest-coverage
else
 GCOV_FLAGS =
endif

CFLAGS = -W -Wall -Wextra -Werror -g3 -ggdb3 -fPIC $(OPTFLAGS) $(GCOV_FLAGS)
CFLAGS += -Wbad-function-cast -Wcast-align -Wconversion -Waggregate-return
CFLAGS += -Wmissing-noreturn -Wmissing-format-attribute
CPPFLAGS = -I. -I../../include -I../../newbrt
CPPFLAGS += -D_GNU_SOURCE -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CFLAGS += $(VISIBILITY) $(PROF_FLAGS)

ifneq ($(OSX),)
CFLAGS+=-fno-common
endif

BINS = linear.o log_nooverlap.o rangetree.o #log.o 

build: $(BINS)
	cd tests;$(MAKE) build

check:
	cd tests;$(MAKE) check

.PHONY: install
install: $(BINS)

clean:
	rm -rf *.o *.gcno *.gcda *.gcov
	cd tests && $(MAKE) clean

rangetree.o: log_nooverlap.o
	cp $< $@

HEADERS=rangetree.h rangetree-internal.h

linear.o: linear.c                  $(HEADERS)

log.o: log.c                        $(HEADERS)

log_nooverlap.o: log_nooverlap.c    $(HEADERS)

