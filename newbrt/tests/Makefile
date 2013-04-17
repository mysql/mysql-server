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

$(BINS): BIN_FROM_C_FLAGS+=$(LIBPORTABILITY)

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


ifeq ($(TOKU_SKIP_1305),1)
check_test1305:
	@echo SKIPPED SLOW TEST $@
else
# Don't run 1305 under valgrind.  It takes way too long.
check_test1305$(BINSUF): test1305$(BINSUF)
	./$< $(VERBVERBOSE) $(SUMMARIZE_CMD)
endif

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

check_brtloader-test$(BINSUF): EXTRA_ARGS=dir.brtloader-test

check_%: % $(PTHREAD_LOCAL)
	$(VGRIND) ./$< $(VERBVERBOSE) $(EXTRA_ARGS) $(SUMMARIZE_CMD)

benchmark-test.$(OEXT): ../brt.h ../brt-search.h ../../include/db.h

brtloader_test$(BINSUF): ../brtloader-internal.h ../brtloader.o
../brtloader.$(OEXT): ../brtloader.c ../brtloader-internal.h 
	cd $(@D) && $(MAKE) $(@F)

clean:
	rm -rf log-test7.c.dir_*
	rm -rf *.dir dir.*
	rm -f test_oexcl.c.tmp
	rm -f *.brt *.clean *.dirty *.tdb *.dat *.data *.out *.check.valgrind

$(BINS): $(LINK_FILES)
$(BINS): test.h
