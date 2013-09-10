default: build
TARGETS = get.parallel get.serial
CPPFLAGS = -I../../../include -I../../../toku_include -I../../../linux
CFLAGS = -Wall -W -Werror -g $(OPTFLAGS)
LOADLIBES = -L../../../lib -ltokuportability -ltokudb
LDFLAGS = -Wl,-rpath,../../../lib
build: $(TARGETS)
CILKPP=/home/bradley/cilkarts/8503/cilk/bin/cilk++
CXX=/home/bradley/cilkarts/8503/cilk/bin/c++
CILKSCREEN=/home/bradley/cilkarts/8503/cilk/bin/cilkscreen

%.parallel: %.cilk
	$(CILKPP) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LOADLIBES)
%.serial: %.cilk
	$(CILKPP) -fcilk-stub $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LOADLIBES)
check: check_get.parallel check_get.serial
check_%.parallel: %.parallel
	$(CILKSCREEN) ./$<
check_%.serial: %.serial
	valgrind ./$<

foo.serial foo.parallel: CPPFLAGS=
foo.serial foo.parallel: LOADLIBES=
foo.serial foo.parallel: LDFLAGS=
foo.serial: CFLAGS=-lpthread

%.o: %.cilk
	$(CILKPP) $(CPPFLAGS) $(CFLAGS) $< -c -o  $@

foo2: foo2.o bar2.o
	$(CILKPP) foo2.o bar2.o -o $@ -lpthread -L/home/bradley/cilkarts/8503/cilk/lib64 -lcilkrts -Wl,-rpath=/home/bradley/cilkarts/8503/cilk/lib64

broken:
	$(CXX) foo2.o bar2.o -o $@ -lpthread -L/home/bradley/cilkarts/8503/cilk/lib64 -lcilkrts -Wl,-rpath=/home/bradley/cilkarts/8503/cilk/lib64

measurecilkrun.parallel: OPTFLAGS=-O2
measurecilkrun.parallel: LDFLAGS=
measurecilkrun.parallel: LOADLIBES=
measurecilkrun.parallel: CPPFLAGS=
check_measurecilkrun.parallel: CILKSCREEN=
