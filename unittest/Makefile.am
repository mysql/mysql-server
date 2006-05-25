SUBDIRS      = mytap . mysys examples

noinst_SCRIPTS = unit

EXTRA_DIST = 		unit.pl

DISTCLEANFILES = unit

unittests = mysys examples

.PHONY: all mytap mysys examples test

test: unit all
	@./unit run $(unittests)

mytap:
	cd mytap && $(MAKE)

mysys:
	cd mysys && $(MAKE)

examples:
	cd examples && $(MAKE)

unit: $(srcdir)/unit.pl
	cp $(srcdir)/unit.pl $@
	chmod +x $@
