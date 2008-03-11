# On OSX do:
#   make OSX=OSX

OPTFLAGS = -O2

LIBNAME=libtokurange

EXPORTMAP = -Wl,--version-script=rangeexport.map
VISIBILITY = -fvisibility=hidden

ifneq ($(OSX),)
LIBRARY=$(LIBNAME).dylib
SHARED=-dynamiclib
RPATHNAME=-install_name @rpath/$(LIBRARY)
CFLAGS+=-fno-common
else
LIBRARY=$(LIBNAME).so
SHARED=-shared $(EXPORTMAP)
RPATHNAME=
endif

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
CFLAGS += $(VISIBILITY)

ifneq ($(OSX),)
CFLAGS+=-fno-common
endif

.PHONY: install
install: linear.o rangetree.o log_nooverlap.o tokuredblack.o $(LIBRARY) #log.o 
	cp $(LIBRARY) ../../lib/

clean:
	rm -rf *.o *.gcno *.gcda *.gcov
	cd tests && make clean

rangetree.o: linear.o
	cp $< $@

HEADERS=rangetree.h rangetree-internal.h

linear.o: linear.c                  $(HEADERS)

log.o: log.c                        $(HEADERS)

log_nooverlap.o: log_nooverlap.c    $(HEADERS)

tokuredblack.o:  tokuredblack.c     $(HEADERS) tokuredblack.h

$(LIBRARY): tokuredblack.o
	cc  $(CPPFLAGS) $^ $(SHARED) -o $@ $(CFLAGS) $(RPATHNAME)
