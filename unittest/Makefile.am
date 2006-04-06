SUBDIRS      = mytap . mysys examples

.PHONY: mytap mysys examples test

noinst_SCRIPTS = unit

test: mytap mysys examples
	./unit run $^

mytap:
	cd mytap && $(MAKE)

mysys:
	cd mysys && $(MAKE)

examples:
	cd examples && $(MAKE)

unit: unit.pl
	cp $< $@
	chmod +x $@
