.DEFAULT_GOAL=build
TOKUROOT=../../
INCLUDEDIRS=-I. -I.. -I../../include

ifeq ($(CYGWIN),)
LINK_FILES += $(NEWBRT)
else ifneq ($(CC),icc)
LINK_FILES += $(NEWBRT)
else ifeq  ($(DEBUG),0)
LINK_FILES += $(IPO_NEWBRT)
else ifeq  ($(DEBUG),)
LINK_FILES += $(IPO_NEWBRT)
else
LINK_FILES += $(NEWBRT)
endif

include $(TOKUROOT)toku_include/Makefile.include
CPPFLAGS+=-D_GNU_SOURCE

ifeq ($(CC),icc)
SKIP_WARNING += $(ICC_NOWARN)1418 #Non static functions do not need prototypes.
endif

# On cygwin do:
#   make CYGWIN=cygwin check

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

SRCS=$(sort $(filter-out dir.%.c,$(wildcard *.c)))

REGRESSION_TESTS_RAW = $(patsubst %.c,%,$(SRCS))

EXTRA_ARGS=
# Add in the binaries that must be run in various ways.
BINS_RAW =  $(REGRESSION_TESTS_RAW) \
# This line intentially kept commented so I can have a \ on the end of the previous line
# BINS will be defined by adding .exe if appropriate.

CHECKS = \
	benchmarktest_256 \
	test-assertA \
	test-assertB \
	$(REGRESSION_TESTS) \
# This line intentially kept commented so I can have a \ on the previous line
#CHECKS will be defined automatically. 

build: $(BINS)
check: $(PTHREAD_LOCAL) $(patsubst %,check_%,$(CHECKS))

check_fail:
	test 0 = 1 $(SUMMARIZE_CMD)
check_ok:
	test 0 = 0 $(SUMMARIZE_CMD)


ifeq ($(TOKU_SKIP_MINICRON),1)
check_minicron-test$(BINSUF):
	@echo "SKIPPED TEST $@ (Fails in win64 vm due to timing issues)"
endif

check_benchmark-test$(BINSUF): benchmark-test$(BINSUF);
check_logcursor-fw$(BINSUF): logcursor-fw$(BINSUF);
	@echo "$@ must be run manually (needs logs to iterate over)."
check_logcursor-bw$(BINSUF): logcursor-bw$(BINSUF);
	@echo "$@ must be run manually (needs logs to iterate over)."

check_benchmarktest_256: benchmark-test$(BINSUF) $(PTHREAD_LOCAL)
	$(VGRIND) ./$< $(VERBVERBOSE) --valsize 256 --verify 1 $(SUMMARIZE_CMD)


check_test-assertA: test-assert$(BINSUF) $(PTHREAD_LOCAL)
	@# no arguments, should err
	$(VGRIND) ./$< > /dev/null 2>&1 ; test $$? = 1 $(SUMMARIZE_CMD)

ifeq ($(OS_CHOICE),windows)
    TEST_ASSERT_FAILCODE=134 #Does not use raise(SIGABRT) to fail assert, so will not overwrite 134 with 1.
else
    TEST_ASSERT_FAILCODE=1
endif
check_test-assertB: test-assert$(BINSUF) $(PTHREAD_LOCAL)
	@# one argument, not "ok" should err
	@rm -f test-assert.out
	($(VGRIND) ./$< notok) > test-assert.out 2>&1 ; test $$? = $(TEST_ASSERT_FAILCODE) && fgrep failed test-assert.out > /dev/null $(SUMMARIZE_CMD)
check_test-assertC: test-assert$(BINSUF) $(PTHREAD_LOCAL)

check_test-assert$(BINSUF): test-assert$(BINSUF) $(PTHREAD_LOCAL)
	@# one argument, "ok" should not error
	$(VGRIND) ./$< ok $(SUMMARIZE_CMD)

check_brtloader-test-merge-files-dbufio: EXTRA_ARGS=-r 8000 -s dir.$@

check_brtloader-test$(BINSUF): EXTRA_ARGS=dir.$@

check_brtloader-test-bad-generate$(BINSUF): EXTRA_ARGS=dir.$@

check_brtloader-test-extractor$(BINSUF): $(patsubst %,check_brtloader-test-extractor-%, 1 2 3 1a 2a 3a 4a 5a)
	true $(SUMMARIZE_CMD)
# the 1,2,3 tests don't use valgrind, the 1a,2a,3a,4a,5a tests use valgrind.
check_brtloader-test-extractor-1: brtloader-test-extractor$(BINSUF)
	./$< $(VERBVERBOSE) -s -r 1000 --rowsets 1000 --asc dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-extractor-2: brtloader-test-extractor$(BINSUF)
	./$< $(VERBVERBOSE) -s -r 1000 --rowsets 1000 --dsc dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-extractor-3: brtloader-test-extractor$(BINSUF)
	./$< $(VERBVERBOSE) -s -r 1000 --rowsets 1000 --random dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-extractor-1a: brtloader-test-extractor$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -s -r 1000 --rowsets 1000 --asc dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-extractor-2a: brtloader-test-extractor$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -s -r 1000 --rowsets 1000 --dsc dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-extractor-3a: brtloader-test-extractor$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -s -r 1000 --rowsets 1000 --random dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-extractor-4a: brtloader-test-extractor$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -s -r 1000 --rowsets 3 --asc dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-extractor-5a: brtloader-test-extractor$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -s -r 1000 --rowsets 3 --asc --asc-poison dir.$@ $(SUMMARIZE_CMD)

check_brtloader-test-extractor-errors$(BINSUF): $(patsubst %,check_brtloader-test-extractor-errors-%, 1 2)
	true $(SUMMARIZE_CMD)
check_brtloader-test-extractor-errors-1: brtloader-test-extractor-errors$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -w -m -u -r 1 dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-extractor-errors-2: brtloader-test-extractor-errors$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -m -r 10000 dir.$@ $(SUMMARIZE_CMD)

check_brtloader-test-open$(BINSUF): EXTRA_ARGS=dir_$@

check_brtloader-test-writer$(BINSUF): $(patsubst %,check_brtloader-test-writer-%, 1 2)
	true $(SUMMARIZE_CMD)
check_brtloader-test-writer-1: brtloader-test-writer$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -r 100000 -s dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-writer-2: brtloader-test-writer$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -r 1000000 -s dir.$@ $(SUMMARIZE_CMD)

check_brtloader-test-writer-errors$(BINSUF): $(patsubst %,check_brtloader-test-writer-errors-%, 1 2 3 4)
	true $(SUMMARIZE_CMD)
check_brtloader-test-writer-errors-1: brtloader-test-writer-errors$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -w -m -u -r 100000 dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-writer-errors-2: brtloader-test-writer-errors$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -s -w -m -u -r 10000 dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-writer-errors-3: brtloader-test-writer-errors$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -s -r 20000 --malloc_limit 0 --realloc_errors dir.$@ $(SUMMARIZE_CMD)
check_brtloader-test-writer-errors-4: brtloader-test-writer-errors$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) -s -m --malloc_limit 0 -r 10000 dir.$@ $(SUMMARIZE_CMD)

check_%: % $(PTHREAD_LOCAL)
	$(VGRIND) ./$< $(VERBVERBOSE) $(EXTRA_ARGS) $(SUMMARIZE_CMD)

benchmark-test.$(OEXT): ../brt.h ../brt-search.h ../../include/db.h

brtloader_test$(BINSUF): ../brtloader-internal.h ../brtloader.o

../brtloader.$(OEXT): ../brtloader.c ../brtloader-internal.h 
	cd $(@D) && $(MAKE) $(@F)

CILKSCREEN=../../scripts/tokucilkscreen

check_brtloader:  check_brtloader-test-bad-generate check_brtloader-test-extractor check_brtloader-test-extractor-errors check_brtloader-test-merge-files-dbufio check_brtloader-test-open check_brtloader-test-writer check_brtloader-test-writer-errors

cilkscreen_brtloader-tests: cilkscreen_brtloader-test-1
	true $(SUMMARIZE_CMD)
cilkscreen_brtloader-test-1: brtloader-test$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) dir.$@ $(SUMMARIZE_CMD)

cilkscreen_brtloader-extractor-tests: $(patsubst %,cilkscreen_brtloader-test-extractor-%, 1 2)
	true $(SUMMARIZE_CMD)
cilkscreen_brtloader-test-extractor-1: brtloader-test-extractor$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) dir.$@ $(SUMMARIZE_CMD)
cilkscreen_brtloader-test-extractor-2: brtloader-test-extractor$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) -s dir.$@ $(SUMMARIZE_CMD)
cilkscreen_brtloader-test-extractor-3: brtloader-test-extractor$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) -s -r 100 --rowsets 100 dir.$@ $(SUMMARIZE_CMD)

cilkscreen_brtloader-writer-tests: $(patsubst %,cilkscreen_brtloader-test-writer-%, 1 2)
	true $(SUMMARIZE_CMD)
cilkscreen_brtloader-test-writer-1: brtloader-test-writer$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) -s -r 1000 dir.$@ $(SUMMARIZE_CMD)
cilkscreen_brtloader-test-writer-2: brtloader-test-writer$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) -s -r 10000 dir.$@ $(SUMMARIZE_CMD)

# this test sometimes crashes cilkscreen 8503, omit for now
cilkscreen_brtloader-test-writer-3: brtloader-test-writer$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) -s -r 100000 dir.$@ $(SUMMARIZE_CMD)

cilkscreen_brtloader-writer-error-tests: $(patsubst %,cilkscreen_brtloader-test-writer-errors-%, 1 2 3 4)
	true $(SUMMARIZE_CMD)
cilkscreen_brtloader-test-writer-errors-1: brtloader-test-writer-errors$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) -s -r 10000 -u dir.$@ $(SUMMARIZE_CMD)
cilkscreen_brtloader-test-writer-errors-2: brtloader-test-writer-errors$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) -s -r 10000 -w dir.$@ $(SUMMARIZE_CMD)
cilkscreen_brtloader-test-writer-errors-3: brtloader-test-writer-errors$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) -s -r 10000 -m dir.$@ $(SUMMARIZE_CMD)
cilkscreen_brtloader-test-writer-errors-4: brtloader-test-writer-errors$(BINSUF)
	$(CILKSCREEN) ./$< $(VERBVERBOSE) -s -r 10000 --realloc_errors dir.$@ $(SUMMARIZE_CMD)

cilkscreen_brtloader: cilkscreen_brtloader-tests cilkscreen_brtloader-extractor-tests cilkscreen_brtloader-writer-tests cilkscreen_brtloader-writer-error-tests

clean:
	rm -rf log-test7.c.dir_*
	rm -rf *.dir dir.*
	rm -f test_oexcl.c.tmp
	rm -f *.brt *.clean *.dirty *.tdb *.dat *.data *.out *.check.valgrind

$(BINS): LDFLAGS=-L../../lib -ltokuportability -lnewbrt $(ALWAYS_LINK) -Wl,-rpath,$(shell pwd)/$(TOKUROOT)lib
$(BINS): test.h

foo:
	echo $(LDFLAGS)
