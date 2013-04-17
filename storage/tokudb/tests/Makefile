SRCS = $(wildcard *.cc)
TARGETS = $(patsubst %.cc,%,$(SRCS))
CHECKS = $(patsubst %,%.check,$(TARGETS))
CPPFLAGS = -I..
CXXFLAGS = -g -fno-implicit-templates

ifeq ($(GCOV),1)
CXXFLAGS += -fprofile-arcs -ftest-coverage
endif

all: $(TARGETS)

clean:
	rm -rf $(TARGETS) *.gcov *.gcno *.gcda

check: $(CHECKS)
	true

%.check: %
	valgrind -q ./$<

max_test.check: max_test
	valgrind -q ./$< 1 2


