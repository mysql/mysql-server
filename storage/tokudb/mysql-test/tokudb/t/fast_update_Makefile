# generate tests from test generator python programs

S = $(wildcard *.py)
T = $(patsubst %.py,%.test,$(S))

default: $(T)

%.test: %.py
	python $< >$@

