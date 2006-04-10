SUBDIRS      = mytap . mysys examples

noinst_SCRIPTS = unit

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

unit: unit.pl
	cp $< $@
	chmod +x $@
