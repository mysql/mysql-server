# -*- Mode: Makefile -*-

.DEFAULT_GOAL=build
TOKUROOT=../
INCLUDEDIRS=-I$(TOKUROOT)newbrt -I./lock_tree -I./range_tree
DEPEND_COMPILE += \
	ydb-internal.h \
	$(TOKUROOT)include/db.h \
	$(TOKUROOT)newbrt/cachetable.h \
	$(TOKUROOT)newbrt/brt.h \
	$(TOKUROOT)newbrt/log.c \
#End of line
ydbtrace.o tdbtrace.o: tdbtrace.h
include $(TOKUROOT)include/Makefile.include
CPPFLAGS += -D_GNU_SOURCE -D_THREAD_SAFE

YDB=ydb.$(AEXT)
TYDB=tydb.$(AEXT)

LIBNAME=libtokudb
TLIBNAME=libtokudbtrace

LIBRARY=$(LIBNAME).$(SOEXT)
TLIBRARY=$(TLIBNAME).$(SOEXT)
LIBRARY_S=$(LIBNAME).$(AEXT)
TLIBRARY_S=$(TLIBNAME).$(AEXT)
OBJS_RAW = \
	ydb_lib \
	ydb \
	errors \
	dlmalloc \
	elocks \
#\end
#OBJS automatically defined.

LIBRARIES= \
	$(LIBRARY) \
	$(TLIBRARY) \
	$(LIBRARY_S) \
	$(TLIBRARY_S) \
#Purposely here for \ at end of prev

INSTALL_LIBRARIES= $(patsubst %,%.install,$(LIBRARIES))

.PHONY:install_libs install.% build install local build_tests

#Half Build Half Install???
build: local build_tests ;

local: buildlocktrees libs install_libs ;

build_tests: | local
	$(MAYBEATSIGN)cd tests && $(MAKE) build


TOBJS= tdbtrace.$(OEXT) $(patsubst ydb.$(OEXT),ydbtrace.$(OEXT),$(OBJS))


install_libs: $(INSTALL_LIBRARIES)
%.install: %
	$(MAYBEATSIGN)if ! diff $* ../lib/$* > /dev/null 2>&1; then cp $* ../lib/; fi

.PHONY: install
install: libs install_libs ;

.PHONY: local libs buildlocktrees
libs: $(LIBRARIES) ;

buildlocktrees: $(LOCKTREE) $(RANGETREE) ;

$(YDB):  $(OBJS) 
$(TYDB): $(TOBJS)


$(LIBRARY):  $(YDB)  $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(DEPEND_COMPILE) $(DEPEND_LINK) 
$(TLIBRARY): $(TYDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(DEPEND_COMPILE) $(DEPEND_LINK)

$(LIBRARY):  LINK_FILES=ydb_lib.$(OEXT) $(YDB)  $(LOCKTREE) $(RANGETREE) $(NEWBRT)
$(TLIBRARY): LINK_FILES=$(TYDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT)

#Skip all BDB tests for CYGWIN+ICC
ifeq ($(CYGWIN),)
$(LIBRARY_S):  $(patsubst %.$(AEXT),%.bundle, $(YDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY))
$(TLIBRARY_S): $(TYDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT_BUNDLE) $(LIBPORTABILITY)
else ifneq ($(CC),icc)
$(LIBRARY_S):  $(YDB)  $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY)
$(TLIBRARY_S): $(TYDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY)
else
$(LIBRARY_S):  $(LIBRARY)  ;
$(TLIBRARY_S): $(TLIBRARY) ;
#$(LIBRARY_S):  $(YDB)  $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY)
#$(TLIBRARY_S): $(TYDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY)
endif

static_libtokudb.lib: $(YDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY)
	xilib /out:$@ $^

ipo_libtokudb.obj: $(YDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY)
	$(CC) -Qipo-c *.obj lock_tree/*.obj range_tree/*.obj ../newbrt/*.obj
	mv ipo_out.obj $@

ipo_libtokudb.lib: ipo_libtokudb.obj
	xilib /out:$@ $^


check_globals: $(LIBNAME).$(SOEXT)
	$(MAYBEATSIGN)python tokuglobals.py $<

.PHONY: check_tests
check_tests: build_tests
	$(MAYBEATSIGN)cd tests && $(MAKE) check

.PHONY: check
check: local check_globals check_tests

.PHONY: strip
strip: $(LIBRARY)
	$(MAYBEATSIGN)strip $<

clean:
	$(MAYBEATSIGN)cd tests && $(MAKE) clean
	$(MAYBEATSIGN)cd lock_tree && $(MAKE) clean
	$(MAYBEATSIGN)cd range_tree && $(MAKE) clean

ydbtrace.$(OEXT): ydb.c $(DEPEND_COMPILE)
	$(MAYBEATSIGN)$(CC) $< $(O_FROM_C_FLAGS) -DTOKUTRACE

