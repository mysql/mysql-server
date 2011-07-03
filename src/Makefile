# -*- Mode: Makefile -*-

.DEFAULT_GOAL=build
TOKUROOT=../
INCLUDEDIRS=-I$(TOKUROOT)newbrt -I../include -I./lock_tree -I./range_tree
DEPEND_COMPILE += \
	ydb-internal.h \
	$(TOKUROOT)include/db.h \
	$(TOKUROOT)newbrt/cachetable.h \
	$(TOKUROOT)newbrt/brt.h \
#End of line
include $(TOKUROOT)toku_include/Makefile.include
CPPFLAGS+=-D_GNU_SOURCE -D_THREAD_SAFE

LIBNAME=libtokudb

LIBRARY=../lib/$(LIBNAME).$(SOEXT)

OBJS_RAW = \
	ydb_lib \
	ydb \
	errors \
	dlmalloc \
	loader \
	indexer \
	indexer-undo-do \
	elocks \
#\end
#OBJS automatically defined.
loader.$(OEXT): $(LOG_HEADER)
ydb.$(OEXT): $(LOG_HEADER)

LIBRARIES=

LIBRARIES+= \
	$(LIBRARY) \
#Purposely here for \ at end of prev

ifeq ($(OS_CHOICE),windows)
#Does not support parallel builds for files in src.
.NOTPARALLEL:;
#Do not build the dynamic libraries.
LIBRARIES=
ifeq ($(DEBUG),0)
WINYDB = $(IPO_YDB)
else
WINYDB = $(NOIPO_YDB)
endif
LIBRARIES += $(WINYDB)
libtokudb.pdb :  $(WINYDB);
endif

.PHONY: build local build_tests

build: local build_tests ;

local: buildlocktrees libs ;

build_tests: | local
	cd tests && $(MAKE) build

.PHONY: local libs buildlocktrees
libs: $(LIBRARIES) ;

#Generate export.def
#Take everything from export.map, add snprintf and vsnprintf, and then convert into export.def format.
export.def: export.map Makefile
	echo "EXPORTS" > $@
	cat export.map | sed 's/global:$$/global:\n   snprintf;\n   usleep;/' | sed -n "s/\([a-zA-Z_0-9][a-zA-Z_0-9]*\);/\t\1 @/g;/@/ P" |sort | sed -n 'P;=' | sed '{;N;s/\n//;}' >> $@
	echo "" >> $@

buildlocktrees: $(LOCKTREE) $(RANGETREE) ;

$(LIBRARY):  $(OBJS) $(LOCKTREE) $(RANGETREE) $(NEWBRT) $(DEPEND_COMPILE) $(DEPEND_LINK) 
NEWBRT_OFILES = $(patsubst %,../%,$(shell cat ../lib/newbrt.olist))
$(LIBRARY): LINK_FILES=$(OBJS) $(LOCKTREE) $(RANGETREE) $(NEWBRT_OFILES)
ifeq ($(CC),icc)
 ifeq ($(HAVE_CILK),1)
  ifeq (0,0)
   $(LIBRARY): LINK_FILES+=$(wildcard ../cilk_icc/*.o)
  else
   $(LIBRARY): LINK_FILES+=-lcilkrts
  endif
 endif
endif

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
	rm -f export.def
	cd tests && $(MAKE) clean
	cd lock_tree && $(MAKE) clean
	cd range_tree && $(MAKE) clean
