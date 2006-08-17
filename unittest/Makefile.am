SUBDIRS      = mytap mysys maria examples

noinst_SCRIPTS = unit
EXTRA_DIST = unit.pl
CLEANFILES = unit

unittests = mytap mysys maria

test: unit
	./unit run $(unittests)

unit: $(srcdir)/unit.pl
	cp $(srcdir)/unit.pl $@
	chmod 700 $@

