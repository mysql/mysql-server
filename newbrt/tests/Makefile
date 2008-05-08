# On cygwin do:
#   make CYGWIN=cygwin check

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
# PROF_FLAGS = -pg
OPTFLAGS = -O3

ifeq ($(VERBOSE),2)
VERBVERBOSE=-v
MAYBEATSIGN= 
else
 ifeq ($(VERBOSE),1)
  VERBVERBOSE=-q
  MAYBEATSIGN=
 else
  VERBVERBOSE=-q
  MAYBEATSIGN=@
 endif
endif

ifeq ($(CYGWIN),cygwin)
else
FPICFLAGS = -fPIC
# valgrind is not present on cygwin
VGRIND = valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

CFLAGS = -Wall -W -Wcast-align -Wbad-function-cast -Wextra -Wmissing-noreturn -Wmissing-format-attribute $(OPTFLAGS) -g3 -ggdb3 $(GCOV_FLAGS) $(PROF_FLAGS) -Werror $(FPICFLAGS) -Wshadow -fvisibility=hidden
LDFLAGS = $(OPTFLAGS) -g $(GCOV_FLAGS) $(PROF_FLAGS) -lz
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_XOPEN_SOURCE=500 -I..

# Put these one-per-line so that if we insert a new one the svn diff can understand it better.
# Also keep them sorted.
REGRESSION_TESTS = \
	brt-serialize-test \
	brt-test \
	brt-test-cursor \
	brt-test-cursor-2 \
	brt-test-named-db \
	brt-test0 \
	brt-test1 \
	brt-test2 \
	brt-test3 \
	brt-test4 \
	brt-test5 \
	cachetable-test \
	cachetable-test2 \
	fifo-test \
	list-test \
	log-test \
	log-test2 \
	log-test3 \
	log-test4 \
	log-test5 \
	log-test6 \
	omt-test \
	test-assert \
	test-brt-delete-both \
	test-brt-overflow \
	test-del-inorder \
	test-inc-split \
	test-primes \
	test_oexcl \
	test_toku_malloc_plain_free \
	ybt-test \
# This line intentially kept commented so I can have a \ on the end of the previous line

# Add in the binaries that must be run in various ways.
BINS =  $(REGRESSION_TESTS) \
        benchmark-test \
# This line intentially kept commented so I can have a \ on the end of the previous line

CHECKS = \
 benchmarktest_256 \
 $(REGRESSION_TESTS) \
# This line intentially kept commented so I can have a \ on the previous line

build: $(BINS)
foo:
	echo $(BINS)
check: $(patsubst %,check_%,$(CHECKS))

check_benchmarktest_256: benchmark-test
	$(VGRIND) ./benchmark-test $(VERBVERBOSE) --valsize 256 --verify 1

check_test-assert: test-assert
	@# no arguments, should err
	$(VGRIND) ./test-assert > /dev/null 2>&1 ; test \($$?\)
	@# one argument, not "ok" should err
	@echo Expect an abort message:
	($(VGRIND) ./test-assert notok) > test-assert.out 2>&1 ; test \($$?\)
	@fgrep failed test-assert.out > /dev/null
	@rm test-assert.out
	@# one argument, "ok" should not error
	$(DVGRIND) ./test-assert ok
check_%: %
	$(VGRIND)  ./$< $(VERBVERBOSE) 

benchmark-test.o: ../brt.h ../brt-search.h ../../include/db.h
$(BINS): ../newbrt.o

test-inc-split test-del-inorder: ../brt-test-helpers.o

clean:
	rm -rf $(BINS) *.o *.bb *.bbg *.da *.gcov *.gcno *.gcda
	rm -rf test_oexcl.c.tmp *.brt

