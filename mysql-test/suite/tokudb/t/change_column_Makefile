# generate tests from test generator python programs

S = $(wildcard *.py)
T = $(patsubst %.py,%.test,$(S))

default: $(T)

%.test: %.py
	python $< >$@

change_all.test: change_all.py change_all_1000_10.test change_all_1000_1.test change_all_10000_1.test
	true
change_all_1000_10.test: change_all.py
	python change_all.py --experiments=1000 --nrows=10 >change_all_1000_10.test
change_all_1000_1.test: change_all.py
	python change_all.py --experiments=1000 --nrows=1 >change_all_1000_1.test
change_all_10000_1.test: change_all.py
	python change_all.py --experiments=10000 --nrows=1 >change_all_10000_1.test

