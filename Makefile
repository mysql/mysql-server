# -*- Mode: Makefile -*-

.DEFAULT_GOAL= default
TOKUROOT=./

include $(TOKUROOT)toku_include/Makefile.include
default: build

ifeq ($(TOKU_SKIP_CXX),1)
    SRCDIRS_CXX =
else
    SRCDIRS_CXX = cxx db-benchmark-test-cxx
endif

SRCDIRS = $(OS_CHOICE) newbrt src/range_tree src/lock_tree src utils db-benchmark-test $(SRCDIRS_CXX)
BUILDDIRS = $(SRCDIRS) man/texi

ifeq ($(OS_CHOICE),windows)
.NOTPARALLEL:; #Windows/cygwin jobserver does not properly handle submakes.  Serialize
endif
newbrt.dir: $(OS_CHOICE).dir
src/range_tree.dir: $(OS_CHOICE).dir 
src/lock_tree.dir: src/range_tree.dir
src.dir: newbrt.dir src/lock_tree.dir
utils.dir: src.dir
db-benchmark-test.dir: src.dir
cxx.dir: src.dir
db-benchmark-test-cxx.dir: cxx.dir

%.dir:
	cd $(patsubst %.dir, %, $@) && $(MAKE) build

build: $(patsubst %,%.dir, $(BUILDDIRS))

%.build:
	cd $(patsubst %.build, %,$@) && $(MAKE) build

%.local:
	cd $(patsubst %.local, %,$@) && $(MAKE) local

%.check:
	cd $(patsubst %.check, %,$@) && $(MAKE) check

release: linux.local newbrt.local src.local release.local

CHECKS = $(patsubst %,%.checkdir,$(SRCDIRS))

# This is the original check rule
# The stuff below allows "make -j2 -k check" to work

#check:
#	for d in $(SRCDIRS); do (cd $$d; $(MAKE) -k check); done

%.checkdir:
	cd $* && $(MAKE) -k check

summarize: SUMMARIZE=1
summarize: VERBOSE=0
summarize: check

check: $(CHECKS)

.PHONY: fastbuild fastbuildtests fastcheck fastchecknewbrt fastcheckydb fastcheckonlyfail fastcheckonlyfailnewbrt fastcheckonlyfailydb
fastbuild:
	$(MAKE) -s -k -C linux
	$(MAKE) -s -k -C newbrt local
	$(MAKE) -s -k -C src local
	$(MAKE) -s -k -C utils

fastbuildtests: fastbuild
	$(MAKE) -s -k -C newbrt/tests
	$(MAKE) -s -k -C src/tests tests.tdb

fastcheck: fastchecknewbrt fastcheckydb
fastchecknewbrt: fastbuildtests
	$(MAKE) -s -k -C newbrt/tests fastcheck
fastcheckydb: fastbuildtests
	$(MAKE) -s -k -C src/tests fastcheck.tdb

fastcheckonlyfail: fastcheckonlyfailnewbrt fastcheckonlyfailydb
fastcheckonlyfailnewbrt: fastbuildtests
	$(MAKE) -s -k -C newbrt/tests fastcheckonlyfail
fastcheckonlyfailydb: fastbuildtests
	$(MAKE) -s -k -C src/tests fastcheckonlyfail.tdb

clean: $(patsubst %,%.dir.clean,$(SRCDIRS)) cleanlib
cleanlib:
	rm -rf lib/*.$(SOEXT) lib/*.$(AEXT) lib/*.bundle

# This does not work, and probably hasn't worked since revision ~2000
# install:
# ./install.bash

# Default to building locally in one's home directory
PREFIX = $(HOME)/local

# This is a quick hack for an install rule
install: release
	mkdir -p $(PREFIX)/lib $(PREFIX)/include
	/bin/cp release/lib/libtokudb.so $(PREFIX)/lib
	/bin/cp release/lib/libtokuportability.so $(PREFIX)/lib
	/bin/cp release/include/db.h $(PREFIX)/include/tokudb.h
	/bin/cp release/include/tdb-internal.h $(PREFIX)/include
	/bin/cp release/include/toku_list.h $(PREFIX)/include

uninstall:
	/bin/rm -f $(PREFIX)/lib/libtokudb.so $(PREFIX)/lib/libtokuportability.so
	/bin/rm -f $(PREFIX)/lib/libtokuportability.so
	/bin/rm -f $(PREFIX)/include/tokudb.h $(PREFIX)/include/tdb-internal.h
	/bin/rm -f $(PREFIX)/include/toku_list.h

# maybe we should have a coverage target in each makefile
build-coverage:
	$(MAKE) build OPTFLAGS= GCOV_FLAGS="-fprofile-arcs -ftest-coverage"

# this is messy now since we dont have consistent make targets
check-coverage: check-coverage-newbrt check-coverage-src-tests check-coverage-utils check-coverage-cxx-tests \
		check-coverage-db-benchmark-test check-coverage-db-benchmark-test-cxx \
		check-coverage-range-tree-tests check-coverage-lock-tree-tests

check-coverage-newbrt:
	(cd newbrt && $(MAKE) -k check VGRIND="")
check-coverage-src-tests:
	(cd src/tests && $(MAKE) -k check.tdb VGRIND="")
check-coverage-utils:
	(cd utils && $(MAKE) -k test-coverage)
check-coverage-cxx-tests:
	(cd cxx/tests && $(MAKE) -k check VGRIND="") 
check-coverage-db-benchmark-test:
	(cd db-benchmark-test && $(MAKE) -k check VGRIND="") 
check-coverage-db-benchmark-test-cxx:
	(cd db-benchmark-test-cxx && $(MAKE) -k check VGRIND="") 
check-coverage-range-tree-tests:
	(cd src/range_tree/tests && $(MAKE) clean && $(MAKE) -k check.lin VGRIND="" OPTFLAGS=-O0 GCOV_FLAGS="-fprofile-arcs -ftest-coverage")
check-coverage-lock-tree-tests:
	(cd src/lock_tree/tests && $(MAKE) clean && $(MAKE) -k check.lin VGRIND="" OPTFLAGS=-O0 GCOV_FLAGS="-fprofile-arcs -ftest-coverage")
