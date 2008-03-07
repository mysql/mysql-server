# On OSX do:
#   make OSX=OSX

OPTFLAGS = -O2

ifneq ($(GCOV),)
 GCOV_FLAGS = -fprofile-arcs -ftest-coverage
else
 GCOV_FLAGS =
endif

VISIBILITY = -fvisibility=hidden

CFLAGS = -W -Wall -Wextra -Werror -g3 -ggdb3 -fPIC $(OPTFLAGS) $(GCOV_FLAGS)
CFLAGS += -Wbad-function-cast -Wcast-align -Wconversion -Waggregate-return
CFLAGS += -Wmissing-noreturn -Wmissing-format-attribute
CPPFLAGS = -I../../include -I../../newbrt
CPPFLAGS += -D_GNU_SOURCE -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CFLAGS += $(VISIBILITY)

ifneq ($(OSX),)
CFLAGS+=-fno-common
endif

.PHONY: install logformat
install: linear.o rangetree.o #log_nooverlap.o log.o 

clean:
	rm -rf *.o *.gcno *.gcda *.gcov
	cd tests && make clean

rangetree.o: linear.o
	cp $< $@

HEADERS=rangetree.h rangetree-internal.h

linear.o: linear.c                  $(HEADERS)

log.o: log.c                        $(HEADERS)

log_nooverlap.o: log_nooverlap.c    $(HEADERS)
