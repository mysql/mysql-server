SUBDIRS      = mytap . mysys examples

noinst_SCRIPTS = unit
EXTRA_DIST = unit.pl
CLEANFILES = unit

unittests = mytap mysys

test:
	perl unit.pl run $(unittests)

unit: $(srcdir)/unit.pl
	install $(srcdir)/unit.pl $@

