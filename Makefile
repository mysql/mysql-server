# -*- Mode: Makefile -*-

.DEFAULT_GOAL= default
TOKUROOT=./

include $(TOKUROOT)include/Makefile.include
default: build

SRCDIRS = $(OS_CHOICE) newbrt src cxx utils db-benchmark-test db-benchmark-test-cxx
BUILDDIRS = $(SRCDIRS) man/texi

newbrt.dir: $(OS_CHOICE).dir
src.dir: newbrt.dir
cxx.dir: src.dir
db-benchmark-test.dir: src.dir
db-benchmark-test-cxx.dir: cxx.dir
utils.dir: src.dir

%.dir:
	cd $(patsubst %.dir, %, $@) && $(MAKE) build

build: $(patsubst %,%.dir, $(BUILDDIRS))

CHECKS = $(patsubst %,%.checkdir,$(filter-out linux, $(SRCDIRS)))

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

clean: 
	$(MAYBEATSIGN)for d in $(SRCDIRS); do (cd $$d && $(MAKE) -k clean); done
	$(MAYBEATSIGN)rm -rf lib/*.$(SOEXT) lib/*.$(AEXT)

install:
	./install.bash

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

