SRCS = $(wildcard *.cc)
TARGETS = $(patsubst %.cc,%,$(SRCS))
CHECKS = $(patsubst %,%.check,$(TARGETS))
CPPFLAGS = -I.. -D__STDC_FORMAT_MACROS
CXXFLAGS = -g -Wall -Wextra -Wno-missing-field-initializers -Wshadow
ifdef USE_OPENMP
CPPFLAGS += -DUSE_OPENMP
CXXFLAGS += -fopenmp
endif

FRACTALTREE_BASE_DIR = ../ft-index
FRACTALTREE_INSTALL_DIR = $(FRACTALTREE_BASE_DIR)/install.debug
VALGRIND = valgrind -q --leak-check=full --show-reachable=yes --suppressions=$(FRACTALTREE_BASE_DIR)/ft/valgrind.suppressions --soname-synonyms=somalloc=*tokuportability*

ifeq ($(GCOV),1)
CXXFLAGS += -fprofile-arcs -ftest-coverage
else
CXXFLAGS += -O3
endif

all: $(TARGETS)

clean:
	rm -rf $(TARGETS) *.gcov *.gcno *.gcda *.testdir *.dSYM

%.check: %
	LD_LIBRARY_PATH=$(FRACTALTREE_INSTALL_DIR)/lib $(VALGRIND) ./$<

card.check: $(patsubst %.cc,%.check,$(wildcard card*.cc))
	true

ifndef USE_OPENMP
# unravel vlq_test_uint64 8 times
vlq_test_uint64_%.check:
	LD_LIBRARY_PATH=$(FRACTALTREE_INSTALL_DIR)/lib $(VALGRIND) ./vlq_test_uint64 $(patsubst vlq_test_uint64_%.check,%,$@) 8
vlq_test_uint64.check: $(foreach i,0 1 2 3 4 5 6 7,vlq_test_uint64_$(i).check)
	true
endif

vlq.check: $(patsubst %.cc,%.check,$(wildcard vlq*.cc))
	true

max_test.check: max_test
	$(VALGRIND) ./$< 1 2

check: $(CHECKS)
	true

%: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g -o $@ $<

card_%: card_%.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g -o $@ $< -I.. -I$(FRACTALTREE_INSTALL_DIR)/include -L$(FRACTALTREE_INSTALL_DIR)/lib -ltokufractaltree -ltokuportability
