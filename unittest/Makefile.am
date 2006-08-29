SUBDIRS      = mytap . mysys examples

EXTRA_DIST = unit.pl
CLEANFILES = unit

unittests = mytap mysys

test:
	perl unit.pl run $(unittests)

test-verbose:
	HARNESS_VERBOSE=1 perl unit.pl run $(unittests)
