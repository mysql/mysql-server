SUBDIRS = src $(ndb_opt_test_subdirs)
DIST_SUBDIRS = src tools ndbapi run-test

EXTRA_DIST = include

dist-hook:
	-rm -rf `find $(distdir) -type d -name SCCS`
