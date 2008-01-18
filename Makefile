TAGS: */*.c */*.h
	etags */*.c */*.h

SRCDIRS = newbrt src src/tests cxx cxx/tests utils db-benchmark-test db-benchmark-test-cxx

clean:
	for d in $(SRCDIRS); do $(MAKE) -k -C $$d clean; done

build:
	for d in $(SRCDIRS); do $(MAKE) -k -C $$d; done

build-coverage:
	for d in $(SRCDIRS); do $(MAKE) -k -C $$d -k OPTFLAGS="-O0" GCOV_FLAGS="-fprofile-arcs -ftest-coverage"; done
