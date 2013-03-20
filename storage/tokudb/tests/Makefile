SRCS = $(wildcard *.cc)
TARGETS = $(patsubst %.cc,%,$(SRCS))
CHECKS = $(patsubst %,%.check,$(TARGETS))
CPPFLAGS = -I.. -D__STDC_FORMAT_MACROS
CXXFLAGS = -g -Wall -Wextra -Wno-missing-field-initializers -Wshadow

FRACTALTREE_BASE_DIR = ../../../../tokudb
FRACTALTREE_DIR = $(FRACTALTREE_BASE_DIR)/release
VALGRIND = valgrind -q --leak-check=full --show-reachable=yes --suppressions=$(FRACTALTREE_BASE_DIR)/build.debug/valgrind.suppressions

ifeq ($(GCOV),1)
CXXFLAGS += -fprofile-arcs -ftest-coverage
endif

all: $(TARGETS)

clean:
	rm -rf $(TARGETS) *.gcov *.gcno *.gcda *.testdir

check: $(CHECKS)
	true

%.check: % 
	LD_LIBRARY_PATH=$(FRACTALTREE_DIR)/lib $(VALGRIND) ./$<

max_test.check: max_test
	$(VALGRIND) ./$< 1 2

%: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g -o $@ $<

db_libtest: db_libtest.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g -o $@ $< -I.. -I$(FRACTALTREE_DIR)/include -L$(FRACTALTREE_DIR)/lib -ltokudb -ltokuportability

card_test: card_test.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g -o $@ $< -I.. -I$(FRACTALTREE_DIR)/include -L$(FRACTALTREE_DIR)/lib -ltokudb -ltokuportability

card_1: card_1.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g -o $@ $< -I.. -I$(FRACTALTREE_DIR)/include -L$(FRACTALTREE_DIR)/lib -ltokudb -ltokuportability

card_inf: card_inf.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g -o $@ $< -I.. -I$(FRACTALTREE_DIR)/include -L$(FRACTALTREE_DIR)/lib -ltokudb -ltokuportability

card_inf_1: card_inf_1.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g -o $@ $< -I.. -I$(FRACTALTREE_DIR)/include -L$(FRACTALTREE_DIR)/lib -ltokudb -ltokuportability

card_random_1: card_random_1.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g -o $@ $< -I.. -I$(FRACTALTREE_DIR)/include -L$(FRACTALTREE_DIR)/lib -ltokudb -ltokuportability
