.DEFAULT_GOAL=build
TOKUROOT=../../
INCLUDEDIRS=-I. -I..
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


# Put these one-per-line so that if we insert a new one the svn diff can understand it better.
# Also keep them sorted.
REGRESSION_TESTS_RAW = \
	block_allocator_test \
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
	cachetable-rwlock-test \
	cachetable-test \
	cachetable-test2 \
	cachetable-put-test \
	cachetable-getandpin-test \
	cachetable-unpin-test \
	cachetable-rename-test \
	cachetable-fd-test \
	cachetable-flush-test \
	cachetable-count-pinned-test \
	cachetable-debug-test \
	cachetable-debug-test \
	cachetable-checkpoint-test \
	cachetable-prefetch-maybegetandpin-test \
	cachetable-prefetch2-test \
	cachetable-prefetch-close-test \
	cachetable-prefetch-close-fail-test \
	cachetable-prefetch-getandpin-test \
	cachetable-prefetch-getandpin-fail-test \
	cachetable-prefetch-checkpoint-test \
	cachetable-prefetch-flowcontrol-test \
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
	log-test7 \
	memtest \
	omt-cursor-test \
	omt-test \
	shortcut \
	test1305 \
	test1308a \
	test-assert \
	test-brt-delete-both \
	test-brt-overflow \
	test-del-inorder \
	test-inc-split \
	test-leafentry \
	test_oexcl \
	test_toku_malloc_plain_free \
	threadpool-test \
	workqueue-test \
        x1764-test \
	ybt-test \
# This line intentially kept commented so I can have a \ on the end of the previous line

# Add in the binaries that must be run in various ways.
BINS_RAW =  $(REGRESSION_TESTS_RAW) \
        benchmark-test \
	cachetable-scan \
# This line intentially kept commented so I can have a \ on the end of the previous line
# BINS will be defined by adding .exe if appropriate.

$(BINS): BIN_FROM_C_FLAGS+=$(LIBPORTABILITY)

CHECKS = \
	benchmarktest_256 \
	test-assertA \
	test-assertB \
	$(REGRESSION_TESTS) \
# This line intentially kept commented so I can have a \ on the previous line
#CHECKS will be defined automatically. 

build: $(BINS)
check: $(patsubst %,check_%,$(CHECKS))

check_fail:
	test 0 = 1 $(SUMMARIZE_CMD)
check_ok:
	test 0 = 0 $(SUMMARIZE_CMD)

# Don't run 1305 under valgrind.  It takes way too long.
check_test1305: VGRIND=
check_test1305:

check_benchmarktest_256: benchmark-test$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) --valsize 256 --verify 1 $(SUMMARIZE_CMD)

check_test-assertA: test-assert$(BINSUF)
	@# no arguments, should err
	$(VGRIND) ./$< > /dev/null 2>&1 ; test $$? = 1 $(SUMMARIZE_CMD)
check_test-assertB: test-assert$(BINSUF)
	@# one argument, not "ok" should err
	@rm -f test-assert.out
	($(VGRIND) ./$< notok) > test-assert.out 2>&1 ; test $$? = 1 && fgrep failed test-assert.out > /dev/null $(SUMMARIZE_CMD)
check_test-assertC: test-assert$(BINSUF)

check_test-assert$(BINSUF): test-assert$(BINSUF)
	@# one argument, "ok" should not error
	$(VGRIND) ./$< ok $(SUMMARIZE_CMD)

check_%: %
	$(VGRIND)  ./$< $(VERBVERBOSE) $(SUMMARIZE_CMD)

benchmark-test.$(OEXT): ../brt.h ../brt-search.h ../../include/db.h

LINK_FILES += ../newbrt.$(AEXT)

clean:
	rm -rf log-test7.c.dir_*
	rm -rf *.dir 
	rm -f cachetable-fd-test.ctest2.data test_oexcl.c.tmp
	rm -f *.brt *brt.clean *brt.dirty *.tdb *.dat *.data *.out

$(BINS):
