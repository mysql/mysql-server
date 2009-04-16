# -*- Mode: Makefile -*-

.DEFAULT_GOAL=build
TOKUROOT=../
INCLUDEDIRS=-I$(TOKUROOT)newbrt -I../include -I./lock_tree -I./range_tree
DEPEND_COMPILE += \
	ydb-internal.h \
	$(TOKUROOT)include/db.h \
	$(TOKUROOT)newbrt/cachetable.h \
	$(TOKUROOT)newbrt/brt.h \
	$(TOKUROOT)newbrt/log.c \
#End of line
include $(TOKUROOT)toku_include/Makefile.include
CPPFLAGS+=-D_GNU_SOURCE -D_THREAD_SAFE

ifneq ($(TOKUDB_REVISION),)
CPPFLAGS += -DTOKUDB_REVISION=$(TOKUDB_REVISION)
endif

YDB=ydb.$(AEXT)
YDB_BUNDLE=ydb.bundle
TYDB=tydb.$(AEXT)
TYDB_BUNDLE=tydb.bundle

IPO_YDB       = ipo_libtokudb.$(AEXT)
NOIPO_YDB     = static_libtokudb.$(AEXT)

LIBNAME=libtokudb

LIBRARY=$(LIBNAME).$(SOEXT)
LIBRARY_S=$(LIBNAME).$(AEXT)
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
	$(LIBRARY_S) \
#Purposely here for \ at end of prev
ifeq ($(OS_CHOICE),windows)
    ifeq ($(DEBUG),0)
	LIBRARIES += $(IPO_YDB)
    else
    	LIBRARIES += $(NOIPO_YDB)
    endif
endif

INSTALL_LIBRARIES= $(patsubst %,%.install,$(LIBRARIES))

.PHONY:install_libs install.% build install local build_tests

#Half Build Half Install???
build: local build_tests ;

local: buildlocktrees libs install_libs ;

build_tests: | local
	cd tests && $(MAKE) build

install_libs: $(INSTALL_LIBRARIES)
%.install: %
	if ! diff $* ../lib/$* > /dev/null 2>&1; then cp $* ../lib/; fi

.PHONY: install
install: libs install_libs ;

.PHONY: local libs buildlocktrees
libs: $(LIBRARIES) ;
$(IPO_YDB) $(NOIPO_YDB) $(LIBRARIES): | export.def
export.def: export.map
	echo "EXPORTS" > $@
	cat export.map |sed -n "s/\([a-zA-Z_0-9][a-zA-Z_0-9]*\);/\t\1 @/g;/@/ P" |sed -n 'P;=' | sed '{;N;s/\n//;}' >> $@
	echo "" >> $@

buildlocktrees: $(LOCKTREE) $(RANGETREE) ;

$(YDB):  $(OBJS) 

$(LIBRARY):  $(YDB)  $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(DEPEND_COMPILE) $(DEPEND_LINK) 

$(LIBRARY):  LINK_FILES=ydb_lib.$(OEXT) $(YDB)  $(LOCKTREE) $(RANGETREE) $(NEWBRT)

#Skip all BDB tests for CYGWIN+ICC
ifeq ($(CYGWIN),)
$(LIBRARY_S):  $(patsubst %.$(AEXT),%.bundle, $(YDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY))
else ifneq ($(CC),icc)
$(LIBRARY_S):  $(YDB)  $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY)
else
$(LIBRARY_S):  $(LIBRARY)  ;
#$(LIBRARY_S):  $(YDB)  $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY)
#$(TLIBRARY_S): $(TYDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(LIBPORTABILITY)
endif

$(NOIPO_YDB): $(YDB) $(LOCKTREE) $(RANGETREE) $(NEWBRT)
	xilib /out:$@ $^

ipo_libtokudb.obj: $(YDB_BUNDLE) $(LOCKTREE_BUNDLE) $(RANGETREE_BUNDLE) $(NEWBRT_BUNDLE)
	$(CC) $(CFLAGS) $(CPPFLAGS) -Qipo-c $(filter %.$(OEXT),$^) $(patsubst %.bundle, %.bundle/*.$(OEXT), $(filter-out %.$(OEXT),$^))
	mv ipo_out.obj $@

$(IPO_YDB): ipo_libtokudb.obj
	xilib /out:$@ $^


check_globals: $(LIBNAME).$(SOEXT)
	python tokuglobals.py $<

.PHONY: check_tests
check_tests: build_tests
	cd tests && $(MAKE) check

.PHONY: check
check: local check_globals check_tests

.PHONY: strip
strip: $(LIBRARY)
	strip $<

clean:
	cd tests && $(MAKE) clean
	cd lock_tree && $(MAKE) clean
	cd range_tree && $(MAKE) clean
