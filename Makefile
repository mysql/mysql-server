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

test-coverage:
	(cd newbrt; $(MAKE) -k check DTOOL="")
	(cd src/tests; $(MAKE) -k check.tdb VGRIND="")
	(cd utils; $(MAKE) -k test-coverage)
	(cd cxx/tests; $(MAKE) -k check VGRIND="") 
