TAGS: */*.c */*.h
	etags */*.c */*.h

clean:
	cd newbrt;make clean
	cd src;make clean
	cd db-benchmark-test-cxx;make clean
