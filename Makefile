TAGS: */*.c */*.h
	etags */*.c */*.h

SRCDIRS = newbrt src src/tests cxx cxx/tests utils db-benchmark-test db-benchmark-test-cxx

clean:
	for d in $(SRCDIRS); do (cd $$d; $(MAKE) -k clean); done

build:
	for d in $(SRCDIRS); do (cd $$d; $(MAKE) -k); done

build-coverage:
	for d in $(SRCDIRS); do (cd $$d; $(MAKE) -k OPTFLAGS="-O0" GCOV_FLAGS="-fprofile-arcs -ftest-coverage"); done
	(cd utils; make clean; make coverage OPTFLAGS="-O0" GCOV_FLAGS="-fprofile-arcs -ftest-coverage")

test-coverage: test-coverage-newbrt test-coverage-src-tests test-coverage-utils test-coverage-cxx-tests
test-coverage-newbrt:
	(cd newbrt; $(MAKE) -k check DTOOL="")
test-coverage-src-tests:
	(cd src/tests; $(MAKE) -k check.tdb VGRIND="")
test-coverage-utils:
	(cd utils; $(MAKE) -k test-coverage)
test-coverage-cxx-tests:
	(cd cxx/tests; $(MAKE) -k check VGRIND="") 
