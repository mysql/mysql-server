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
else
 ifeq ($(VERBOSE),1)
  VERBVERBOSE=-q
 else
  VERBVERBOSE=-q
 endif
endif

ifneq ($(CYGWIN),)
FORMAT=-Wno-format
CYG_ADD_LIBZ=/usr/lib/libz.a
else
FPICFLAGS = -fPIC
# valgrind is not present on cygwin
VGRIND = valgrind --quiet --error-exitcode=1 --leak-check=yes
VISIBILITY=-fvisibility=hidden
SHADOW=-Wshadow
FORMAT=-Wmissing-format-attribute
endif

CFLAGS = -Wall -Wextra -Wcast-align -Wbad-function-cast -Wmissing-noreturn $(FORMAT) $(OPTFLAGS) -g3 -ggdb3 $(GCOV_FLAGS) $(PROF_FLAGS) -Werror $(FPICFLAGS) $(SHADOW) $(VISIBILITY)
LDFLAGS = $(OPTFLAGS) -g $(GCOV_FLAGS) $(PROF_FLAGS) -lpthread
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -I..

# Add -Wconversion

HERE=newbrt/tests

ifeq ($(SUMMARIZE),1)
SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sPASS\n" $(HERE)/$@; else printf "%-60sFAIL\n" $(HERE)/$@ ; test 0 = 1; fi
else
SUMMARIZE_CMD =
endif

# Put these one-per-line so that if we insert a new one the svn diff can understand it better.
# Also keep them sorted.
REGRESSION_TESTS = \
	bread-test \
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
	cachetable-put-test \
	cachetable-unpin-test \
	cachetable-rename-test \
	cachetable-fd-test \
	cachetable-count-pinned-test \
	cachetable-checkpoint-test \
	fifo-test \
	list-test \
	keyrange \
	keyrange-unflat \
	keyrange-dupsort \
	keyrange-dupsort-unflat \
	log-test \
	log-test2 \
	log-test3 \
	log-test4 \
	log-test5 \
	log-test6 \
	memtest \
	omt-cursor-test \
	omt-test \
	shortcut \
	test-assert \
	test-brt-delete-both \
	test-brt-overflow \
	test-del-inorder \
	test-inc-split \
	test-leafentry \
	test_oexcl \
	test_toku_malloc_plain_free \
        x1764-test \
	ybt-test \
# This line intentially kept commented so I can have a \ on the end of the previous line

# Add in the binaries that must be run in various ways.
BINS =  $(REGRESSION_TESTS) \
        benchmark-test \
	cachetable-scan \
# This line intentially kept commented so I can have a \ on the end of the previous line

CHECKS = \
 benchmarktest_256 \
 test-assertA test-assertB \
 $(REGRESSION_TESTS) \
# This line intentially kept commented so I can have a \ on the previous line

build: $(BINS)
foo:
	echo $(BINS)
check: $(patsubst %,check_%,$(CHECKS))

check_fail:
	test 0 = 1 $(SUMMARIZE_CMD)
check_ok:
	test 0 = 0 $(SUMMARIZE_CMD)

check_benchmarktest_256: benchmark-test
	$(VGRIND) ./benchmark-test $(VERBVERBOSE) --valsize 256 --verify 1 $(SUMMARIZE_CMD)

check_test-assertA: test-assert
	@# no arguments, should err
	$(VGRIND) ./test-assert > /dev/null 2>&1 ; test $$? = 1 $(SUMMARIZE_CMD)
check_test-assertB: test-assert
	@# one argument, not "ok" should err
	@rm -f test-assert.out
	($(VGRIND) ./test-assert notok) > test-assert.out 2>&1 ; test $$? = 1 && fgrep failed test-assert.out > /dev/null $(SUMMARIZE_CMD)
check_test-assertC: tst-assert



check_test-assert: test-assert
	@# one argument, "ok" should not error
	$(DVGRIND) ./test-assert ok $(SUMMARIZE_CMD)

check_%: %
	$(VGRIND)  ./$< $(VERBVERBOSE) $(SUMMARIZE_CMD)

benchmark-test.o: ../brt.h ../brt-search.h ../../include/db.h
$(BINS): ../newbrt.o $(CYG_ADD_LIBZ)

test-inc-split test-del-inorder: ../brt-test-helpers.o

clean:
	rm -rf $(BINS) *.o *.bb *.bbg *.da *.gcov *.gcno *.gcda
	rm -rf test_oexcl.c.tmp *.brt *.dir *.tdb *.dat *.out
