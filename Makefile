TAGS: */*.c */*.h
	etags */*.c */*.h src/lock_tree/*.c src/lock_tree/*.h src/range_tree/*.c src/range_tree/*.h

SRCDIRS = newbrt src src/tests src/range_tree src/range_tree/tests src/lock_tree src/lock_tree/tests cxx cxx/tests \
		utils db-benchmark-test db-benchmark-test-cxx

build:
	for d in $(SRCDIRS); do (cd $$d; $(MAKE) -k); done

check:
	for d in $(SRCDIRS); do (cd $$d; $(MAKE) -k check); done

clean:
	for d in $(SRCDIRS); do (cd $$d; $(MAKE) -k clean); done

install:
	./install.bash

# maybe we should have a coverage target in each makefile
build-coverage:
	for d in $(SRCDIRS); do (cd $$d; $(MAKE) -k OPTFLAGS=-O0 GCOV_FLAGS="-fprofile-arcs -ftest-coverage"); done
	(cd utils; $(MAKE) clean; $(MAKE) coverage OPTFLAGS=-O0 GCOV_FLAGS="-fprofile-arcs -ftest-coverage")

# this is messy now since we dont have consistent make targets
check-coverage: check-coverage-newbrt check-coverage-src-tests check-coverage-utils check-coverage-cxx-tests \
		check-coverage-db-benchmark-test check-coverage-db-benchmark-test-cxx \
		check-coverage-range-tree-tests check-coverage-lock-tree-tests

check-coverage-newbrt:
	(cd newbrt; $(MAKE) -k check DTOOL="")
check-coverage-src-tests:
	(cd src/tests; $(MAKE) -k check.tdb VGRIND="")
check-coverage-utils:
	(cd utils; $(MAKE) -k test-coverage)
check-coverage-cxx-tests:
	(cd cxx/tests; $(MAKE) -k check VGRIND="") 
check-coverage-db-benchmark-test:
	(cd db-benchmark-test; $(MAKE) -k check VGRIND="") 
check-coverage-db-benchmark-test-cxx:
	(cd db-benchmark-test-cxx; $(MAKE) -k check VGRIND="") 
check-coverage-range-tree-tests:
	(cd src/range_tree/tests; $(MAKE) clean; $(MAKE) -k check.lin VGRIND="" OPTFLAGS=-O0 GCOV_FLAGS="-fprofile-arcs -ftest-coverage")
check-coverage-lock-tree-tests:
	(cd src/lock_tree/tests; $(MAKE) clean; $(MAKE) -k check.lin VGRIND="" OPTFLAGS=-O0 GCOV_FLAGS="-fprofile-arcs -ftest-coverage")
