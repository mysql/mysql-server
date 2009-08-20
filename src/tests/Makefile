# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../../
INCLUDEDIRS=-I. -I../ -I$(TOKUROOT)newbrt -I../range_tree -I../lock_tree
DEPEND_COMPILE += \
	../*.h \
	../range_tree/*.h \
	../lock_tree/*.h \
	test.h \
#end
include $(TOKUROOT)toku_include/Makefile.include
CPPFLAGS+=-D_GNU_SOURCE
LIBTDB=../libtokudb.$(SOEXT)
TLIBTDB=../libtokudbtrace.$(SOEXT)

ifeq ($(CC),icc)
SKIP_WARNING += $(ICC_NOWARN)1418 #Non static functions do not need prototypes.
endif
BDBVGRIND=

$(LIBTDB) $(TLIBTDB):
	cd $(@D) && $(MAKE) $(@F)

$(notdir $(LIBTDB)): $(LIBTDB)
	cp $< $@

$(notdir $(TLIBTDB)): $(TLIBTDB)
	cp $< $@

SRCS = $(sort $(wildcard *.c))


#Tests that are irrelevant in windows.
WINDOWS_DONTRUN_TESTS = \
	helgrind1 \
	helgrind2 \
	helgrind3 \
#\ ends prev line

#BDB Tests that are irrelevant in windows.
WINDOWS_BDB_DONTRUN_TESTS = \
	diskfull \
	test_groupcommit_count \
	test_set_func_malloc \
#\ ends prev line
#diskfull: windows bdb is missing db_env_set_func_pwrite and db_env_set_func_write
#test_groupcommit_count: windows bdb is missing db_env_set_func_fsync
#test_set_func_malloc: windows bdb (some) are missing db_env_set_func_(malloc|realloc|free)


#These tests fail, but sometimes they hang! (in windows).  Just disable them.
WINDOWS_BDB_DONTRUN_TESTS += \
	test_error \
#\ ends prev line

ifeq ($(OS_CHOICE),windows)
    TDB_TESTS = $(patsubst %.c,%.tdb$(BINSUF),$(filter-out $(patsubst %,%.c,$(WINDOWS_DONTRUN_TESTS)),$(SRCS)))
else
    TDB_TESTS = $(patsubst %.c,%.tdb$(BINSUF),$(SRCS))
endif

# For diskfull.bdb: db-4.6 seems OK, but db-4.3 segfaults
BDB_DONTRUN_TESTS = \
	bug1381 \
	bug627 \
	diskfull \
	test_abort1 \
	test_abort4 \
	test_abort5 \
	keyrange \
	keyrange-unflat \
	keyrange-dupsort \
	keyrange-dupsort-unflat \
	manyfiles \
	stat64 stat64_flatten \
	test938c \
	test1324 \
	helgrind1 \
	helgrind2 \
	helgrind3 \
	test1426 \
	test1572 \
	test_logflush \
	test_txn_abort6 \
	test_txn_abort8 \
	test_txn_abort9 \
	test_txn_close_open_commit \
	test_txn_commit8 \
	test_db_descriptor \
	test_db_descriptor_named_db \
	test_db_txn_locks_heaviside \
	test_heaviside_rh_1756 \
	test_heaviside_straddle_1622 \
	test_dbremove_old \
	root_fifo_2 \
	root_fifo_32 \
	root_fifo_41 \
	checkpoint_1 \
	checkpoint_stress \
	checkpoint_truncate_1 \
	test_txn_nested1 \
	test_txn_nested2 \
	test_txn_nested3 \
	test_txn_nested4 \
	backwards_10_each_le_and_msg \
	test_dupsort_set_range_reverse \
	test_dupsort_get_both_range_reverse \
#\ ends prev line

# checkpoint tests depend on this header file, 
checkpoint_1.tdb$(BINSUF) checkpoint_stress.tdb$(BINSUF): checkpoint_test.h
# dependency only, not a rule (do not insert a tab at this line)

BDB_TESTS_THAT_SHOULD_FAIL= \
#\ ends prev line
ifeq ($(OS_CHOICE),windows)
    #The following are nondeterministic test failures or hangs in windows.  Just get rid of them in BDB. (for now)
    WINDOWS_BDB_DONTRUN_TESTS += \
	test_archive2 \
	test_cursor_delete \
	test_cursor_stickyness \
	test_db_dbt_mem_behavior \
	test_dup_insert \
	test_dup_search \
	test_dupsort_get \
	test_dupsort_set \
	test_unused_memory_crash \
	test_dupsort_get_both_range \
	test_reverse_compare_fun \
	test-recover3 \
#\ ends prev line
endif


ifeq ($(OS_CHOICE),windows)
    BDB_DONTRUN_TESTS+=$(WINDOWS_BDB_DONTRUN_TESTS)
endif

BDB_TESTS = $(patsubst %.c,%.bdb$(BINSUF),$(filter-out $(patsubst %,%.c,$(BDB_DONTRUN_TESTS)),$(SRCS)))

TDB_TESTS_THAT_SHOULD_FAIL= \
	test_groupcommit_count \
	test944 \
	test_truncate_txn_abort \
#\ ends prev line
ifneq ($(OS_CHOICE),windows)
    TDB_TESTS_THAT_SHOULD_FAIL+= \
	test_db_remove \
#\ ends prev line
endif

TDB_TESTS_THAT_SHOULD_FAIL_LIT= \
#\ ends prev line

ifeq ($(OS_CHOICE),windows)
    #Tests that fail in windows but shouldn't.  TODO: Fix each one of these and remove from the list!
    TDB_TESTS_THAT_SHOULD_FAIL += \
	test-recover2 \
	test-recover3 \
#\ ends prev line
    TDB_TESTS_THAT_SHOULD_FAIL_LIT += \
	test_log4.recover \
	test_log5.recover \
	test_log7.recover \
	test_log8.recover \
	test_log9.recover \
	test_log10.recover \
#\ ends prev line
    #NOTE:  test_log[89].recover is non-deterministic.  Sometimes passes/fails
endif

ALL_TESTS  = $(TDB_TESTS)

#Skip all BDB tests for CYGWIN+ICC
ifeq ($(CYGWIN),)
ALL_TESTS += $(BDB_TESTS)
else ifneq ($(CC),icc)
ALL_TESTS += $(BDB_TESTS)
endif

TLRECOVER = 2 3 4 5 6 7 8 9 10

EXTRA_TDB_TESTS = \
	$(patsubst %,test_log%.recover,$(TLRECOVER)) \
#	test_db_assoc3.tdbrun_wasbad \# Obsolete right now.
#\ ends prev line

RUN_TDB_TESTS = $(patsubst %.tdb$(BINSUF),%.tdbrun,$(TDB_TESTS))  $(EXTRA_TDB_TESTS)
RUN_BDB_TESTS  = $(patsubst %.bdb$(BINSUF),%.bdbrun,$(BDB_TESTS))
RUN_ALL_TESTS  = $(RUN_BDB_TESTS) $(RUN_TDB_TESTS)

MORE_BDB_BINS = test1426.bdb$(BINSUF)

.PHONY: default all check tests check.lin check.tlog check.log tests.lin tests.log tests.tlog

default all build: $(ALL_TESTS) $(MORE_BDB_BINS)

ifndef TOKUDBONLY
MAYBE_CHECK_BDB = check.bdb
else
MAYBE_CHECK_BDB =
endif
check: check.tdb $(MAYBE_CHECK_BDB) ;

tests: tests.bdb tests.tdb ;
tests.bdb: $(BDB_TESTS) ;
check.bdb: $(RUN_BDB_TESTS) ;
tests.tdb: $(TDB_TESTS) ;
check.tdb: $(notdir $(LIBTDB)) $(PTHREAD_LOCAL) $(RUN_TDB_TESTS) ;

foo:
	echo RUN_TDB_TESTS: $(RUN_TDB_TESTS)
	echo ALL_TESTS: $(ALL_TESTS)

#TODO: What is this for? TODO: Port this if necessary.
#traces: test_env_open_flags.tdbt$(BINSUF)
traces: $(patsubst %.tdb$(BINSUF),%.tdbt$(BINSUF),$(TDB_TESTS))

.PHONY: %.bdbrun %.run %.tdbrun
# STUFF!!!!

%.run: %.bdbrun %.tdbrun

SHOULD_FAIL = $(TDB_TESTS_THAT_SHOULD_FAIL_LIT) $(patsubst %,%.tdbrun,$(TDB_TESTS_THAT_SHOULD_FAIL)) \
	      $(BDB_TESTS_THAT_SHOULD_FAIL_LIT) $(patsubst %,%.bdbrun,$(BDB_TESTS_THAT_SHOULD_FAIL))
# Any test that should fail, we invert the result by using MAYBEINVERTER
$(SHOULD_FAIL): MAYBEINVERTER=$(INVERTER)
$(SHOULD_FAIL): SUMMARIZE_CMD=$(SUMMARIZE_SHOULD_FAIL)

TDBVGRIND=$(VGRIND)

ifeq ($(OS_CHOICE),windows)
ifdef BDBDIR
%.bdb$(BINSUF): INCLUDEDIRS=-I$(BDBDIR)/include -I$(TOKUROOT)newbrt
%.bdb$(BINSUF): RPATH_DIRS=$(BDBDIR)/lib
endif
ifeq ($(DEBUG),0)
WINDOWS_BDB_LIB_NAME=libdb.$(SOEXT)
$(WINDOWS_BDB_LIB_NAME):
	cp $(BDBDIR)/lib/libdb[0-9][0-9].$(SOEXT) ./
else
WINDOWS_BDB_LIB_NAME=libdbd.$(SOEXT)
$(WINDOWS_BDB_LIB_NAME):
	cp $(BDBDIR)/lib/libdb[0-9][0-9]d.$(SOEXT) ./
endif
%.bdb$(BINSUF): DLINK_FILES=$(BDBDIR)/lib/$(WINDOWS_BDB_LIB_NAME)
#empty
else
WINDOWS_BDB_LIB_NAME=
#linux
ifdef BDBDIR
%.bdb$(BINSUF): INCLUDEDIRS=-I$(BDBDIR)/include
%.bdb$(BINSUF): RPATH_DIRS=$(BDBDIR)/lib
endif
%.bdb$(BINSUF): DLINK_FILES=db.$(SOEXT)
endif
#empty on purpose



# Use -s on the command line to make things quiet.
# Use -s on the command line to make things quiet.
%.bdbrun: %.bdb$(BINSUF) $(DEPEND_COMPILE) $(DEPEND_LINK) $(WINDOWS_BDB_LIB_NAME)
	$(BDBVGRIND) ./$< $(VERBVERBOSE) $(MAYBEINVERTER) $(SUMMARIZE_CMD)
%.tdbrun: %.tdb$(BINSUF) $(DEPEND_COMPILE) $(DEPEND_LINK) $(notdir $(LIBTDB)) $(PTHREAD_LOCAL)
	$(TDBVGRIND) ./$< $(VERBVERBOSE) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

ifeq ($(OS_CHOICE),windows)
    TDBDUMP=$(TOKUROOT)utils/tokudb_dump$(BINSUF)
    TDBLOAD=$(TOKUROOT)utils/tokudb_load$(BINSUF)
else
    TDBDUMP=$(TOKUROOT)utils/tokudb_dump_static$(BINSUF)
    TDBLOAD=$(TOKUROOT)utils/tokudb_load_static$(BINSUF)
endif

%.recover: %.tdb$(BINSUF) $(PTHREAD_LOCAL)
	echo doing ./$< &&\
	$(VGRIND) ./$< --no-shutdown                                                            && \
	rm -rf dir.$*.c.tdb.recover                                                             && \
	mkdir dir.$*.c.tdb.recover                                                              && \
	echo doing recovery &&\
	$(VGRIND) ../../newbrt/tdb-recover dir.$*.c.tdb.recover dir.$*.c.tdb                    && \
	echo dump and compare &&\
	$(TDBDUMP) -h dir.$*.c.tdb         foo.db > dir.$*.c.tdb/foo.dump          && \
	$(TDBDUMP) -h dir.$*.c.tdb.recover foo.db > dir.$*.c.tdb.recover/foo.dump  && \
	diff -q dir.$*.c.tdb/foo.dump dir.$*.c.tdb.recover/foo.dump                                   \
	$(MAYBEINVERTER) $(SUMMARIZE_CMD)

%.recoverwc: %.tdb$(BINSUF) $(PTHREAD_LOCAL)
	 (cd dir.$*.c.tdb;pwd;cat log*| ../../../newbrt/tdb_logprint |wc -c)

.PHONY: %.recover
all.recover: $(patsubst %,test_log%.recover,$(TLRECOVER)) $(PTHREAD_LOCAL) ;

#DISABLE standard tdbrun for recover tests.
$(patsubst %,test_log%.tdbrun,$(TLRECOVER)): ;

%.bdb$(BINSUF): CFLAGS+= -DENVDIR=\"dir.$<.bdb\" -DUSE_BDB -DIS_TDB=0 -DTOKU_ALLOW_DEPRECATED

%.bdb$(BINSUF): %.c $(DEPEND_COMPILE) $(DEPEND_LINK)
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)

TDB_EXTRA_NEEDED=
ifeq ($(CYGWIN),)
%.tdb$(BINSUF):  DLINK_FILES+=$(LIBTDB)
%.tdb$(BINSUF):  RPATH_DIRS+=$(dir $(LIBTDB))
else ifneq ($(CC),icc)
%.tdb$(BINSUF):  DLINK_FILES+=$(LIBTDB)
%.tdb$(BINSUF):  RPATH_DIRS+=$(dir $(LIBTDB))
else ifeq  ($(DEBUG),0)
%.tdb$(BINSUF):  LINK_FILES+=$(IPO_YDB)
TDB_EXTRA_NEEDED=$(IPO_YDB)
else
%.tdb$(BINSUF):  LINK_FILES+=$(NOIPO_YDB)
TDB_EXTRA_NEEDED=$(NOIPO_YDB)
endif

%.tdbt$(BINSUF): DLINK_FILES+=$(TLIBTDB)
%.tdbt$(BINSUF): RPATH_DIRS+=$(dir $(TLIBTDB))
%.tdb$(BINSUF) %.tdbt$(BINSUF): CFLAGS+= -DENVDIR=\"dir.$<.tdb\" -DUSE_TDB -DIS_TDB=1
%.tdb$(BINSUF) %.tdbt$(BINSUF): CPPFLAGS+=-I$(TOKUROOT)include

%.tdb$(BINSUF) %.tdbt$(BINSUF): %.c $(DEPEND_COMPILE) $(DEPEND_LINK) $(TDB_EXTRA_NEEDED)
	$(CC) $< $(filter-out ../../lib/libtokuportability.a,$(BIN_FROM_C_FLAGS)) $(LINK_MUST_BE_LAST)

clean:
	rm -f $(ALL_TESTS)
	rm -rf dir.*.tdb dir.*.bdb dir.*.tdb.recover
	rm -f dump.bdb.1426 dump.tdb.1426 test1426.bdb

ifeq ($(VGRIND),)
  BDB_SUPPRESSIONS =
else
  BDB_SUPPRESSIONS = --suppressions=bdb.supressions --gen-suppressions=all
endif

# VERBOSE=true

# Need these rule so that Make knows about all the file names
.PHONY: %.run
$(RUN_ALL_TESTS):
$(ALL_TESTS):

%.run: %.bdbrun %.tdbrun
	@ echo ok




INVERTER=;test $$? -ne 0

# Any test that should fail, we invert the result by using MAYBEINVERTER
$(SHOULD_FAIL): MAYBEINVERTER=$(INVERTER)

HERE = src/tests


# Don't include log2 log3 log4 log5 etc since they are covered by all.recover

# Don't run valgrind on the groupcommit performance tests
test_groupcommit_perf.bdbrun test_groupcommit_perf.tdbrun: VGRIND=
# Use helgrind on the group commit count test
# helgrind is too flakey, so I'm removing it from the tests. -Bradley
#test_groupcommit_count_helgrind.tdbrun: test_groupcommit_count_helgrind.tdb
#	$(HGRIND) ./$< $(VERBVERBOSE)



libs:
	cd ..;$(MAKE)




test_db_curs4.tdb$(BINSUF): trace.h
test_db_curs4.bdb$(BINSUF): trace.h
test_db_assoc3.tdb$(BINSUF) test_db_assoc3.bdb$(BINSUF): test.h


# This one failed in both BDB and TokuDB, in the same way.  It was a program error.  Now it works
test_db_assoc3.tdbrun_wasbad: test_db_assoc3.tdb$(BINSUF)
	./$< --seed=1 --count=200
	./$< --seed=1 --count=200 --more
	./$< --seed=1 --count=200 --more
	./$< --seed=1 --count=200 --more
	./$< --seed=1 --count=200 --more
	./$< --seed=1 --count=200 --more
	./$< --seed=1 --count=200 --more
# serialize these two tests since they use the same directory
test_db_assoc3.tdbrun_wasbad: test_db_assoc3.tdbrun


.phony: build_primary_db build_name_db  build_expire_db
test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb$(BINSUF):
	mkdir $@
build_primary_db: test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb$(BINSUF)
	gunzip < test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb.original/primary.db.gz > test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb/primary.db
build_name_db: test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb$(BINSUF)
	gunzip < test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb.original/name.db.gz > test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb/name.db
build_expire_db: test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb$(BINSUF)
	gunzip < test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb.original/expire.db.gz > test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb/expire.db

test_v6v7_assoc3.tdbrun: test_db_assoc3.tdb$(BINSUF) build_primary_db  build_name_db build_expire_db
	(cd test_v6_assoc3.dir; LD_LIBRARY_PATH=../.. $(VGRIND) ../test_db_assoc3.tdb$(BINSUF) --seed=3 --count=1000 --more)  $(SUMMARIZE_CMD)

test_db_assoc3.tdbrun: test_db_assoc3.tdb$(BINSUF)
	$(VGRIND) ./$< --seed=2 --count=100000 $(VERBVERBOSE) && \
	              $(VGRIND) ./$< --seed=2 --count=100000 --more $(VERBVERBOSE) $(SUMMARIZE_CMD)

# Give up on VGRIND for bdbrun
test_db_assoc3.bdbrun: test_db_assoc3.bdb$(BINSUF) $(WINDOWS_BDB_LIB_NAME)
	./$< --seed=2 --count=100000        $(VERBVERBOSE) && \
	              ./$< --seed=2 --count=100000 --more $(VERBVERBOSE) $(SUMMARIZE_CMD)

# a bunch of little tests designed to run in parallel
test_get_both_range.tdbrun: \
 tgbr_256_a.tdbrun tgbr_256_b.tdbrun tgbr_256_c.tdbrun \
 tgbr_128_a.tdbrun tgbr_128_b.tdbrun tgbr_128_c.tdbrun \
 tgbr_64_a.tdbrun  tgbr_64_b.tdbrun  tgbr_64_c.tdbrun \
 tgbr_32_a.tdbrun  tgbr_32_b.tdbrun  tgbr_32_c.tdbrun \
 tgbr_16_a.tdbrun  tgbr_16_b.tdbrun  tgbr_16_c.tdbrun \
 tgbr_8_a.tdbrun   tgbr_8_b.tdbrun   tgbr_8_c.tdbrun \
 tgbr_4_a.tdbrun   tgbr_4_b.tdbrun   tgbr_4_c.tdbrun \
 tgbr_2_a.tdbrun   tgbr_2_b.tdbrun   tgbr_2_c.tdbrun \
 tgbr_1_a.tdbrun   tgbr_1_b.tdbrun   tgbr_1_c.tdbrun \
 # intentionally blank line
	true $(SUMMARIZE_CMD) #State that this has passed.

tgbr_%_a.tdbrun: test_get_both_range.tdb$(BINSUF) $(PTHREAD_LOCAL)
	$(VGRIND) ./$< $(VERBVERBOSE) -i $* -a $(MAYBEINVERTER) $(SUMMARIZE_CMD)
tgbr_%_b.tdbrun: test_get_both_range.tdb$(BINSUF) $(PTHREAD_LOCAL)
	$(VGRIND) ./$< $(VERBVERBOSE) -i $* -b $(MAYBEINVERTER) $(SUMMARIZE_CMD)
tgbr_%_c.tdbrun: test_get_both_range.tdb$(BINSUF) $(PTHREAD_LOCAL)
	$(VGRIND) ./$< $(VERBVERBOSE) -i $* -c $(MAYBEINVERTER) $(SUMMARIZE_CMD)

ifeq ($(OS_CHOICE),windows)
    #Windows BDB cannot run 1426.  Save output from linux and compare.
dump.bdb.1426: test1426.bdbdump/dump.bdb.1426
	cat $< | unix2dos --u2d > $@

else
dump.bdb.1426: test1426.bdb$(BINSUF) test1426.bdbdump/dump.bdb.1426
	./$< -q  && \
	 $(BDBDIR)/bin/db_dump -p -h dir.test1426.c.bdb main > $@
	diff -q $@ test1426.bdbdump/$@ #Assert output is correct.
endif

# test1426 is run by comparing the BDB output to the TDB output
test1426.tdbrun: test1426.tdb$(BINSUF) dump.bdb.1426 $(PTHREAD_LOCAL)
	(./test1426.tdb$(BINSUF) -q && \
	 $(TDBDUMP) -p -h dir.test1426.c.tdb main > dump.tdb.1426 && \
	 diff -q -I db_pagesize=4096 dump.bdb.1426 dump.tdb.1426 ) \
	 $(MAYBEINVERTER) $(SUMMARIZE_CMD)

# STRESS RUNS is number of iterations, STRESS_SIZE is operations per iteration (must not change between iterations)
STRESS_RUNS=5
STRESS_SIZE=5001
checkpoint_stress.tdbrun: SHELL=/bin/bash
checkpoint_stress.tdbrun: checkpoint_stress.tdb$(BINSUF) $(PTHREAD_LOCAL)
	$(VGRIND) ./$< -C -n $(STRESS_SIZE) $(VERBVERBOSE) && \
	($(VGRIND) ./$< -C -i 0 -n $(STRESS_SIZE) $(VERBVERBOSE) && \
	 for (( i = 1; i < $(STRESS_RUNS); i++ )); do \
		date; \
		./$< -c -i $$i -n $(STRESS_SIZE) $(VERBVERBOSE) 2> dir.checkpoint_stress.c.tdb/error.$$i ; \
		if ! grep -q 'HAPPY CRASH' dir.checkpoint_stress.c.tdb/error.$$i; then break; fi; \
	done && \
	test `grep -l 'HAPPY CRASH' dir.checkpoint_stress.c.tdb/error.* |wc -l` = $$(($(STRESS_RUNS)-1)) \
	) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

# helgrind1 is supposed to fail.
helgrind1.tdbrun: TDBVGRIND=$(HGRIND) --log-file=helgrind1.tdb.deleteme
helgrind1.tdbrun: MAYBEINVERTER=$(INVERTER)
helgrind2.tdbrun: TDBVGRIND=$(HGRIND)
helgrind2.bdbrun: BDBVGRIND=$(HGRIND)
helgrind3.tdbrun: TDBVGRIND=$(HGRIND)
helgrind3.bdbrun: BDBVGRIND=$(HGRIND)

dumpit:
	../../newbrt/brtdump  dir.test_log5.c.tdb.recover/foo.db > dump.r && ../../newbrt/brtdump  dir.test_log5.c.tdb/foo.db > dump.o && diff -q dump.o dump.r

# test on small stacks
test_thread_stack.%run: test_thread_stack.%$(BINSUF) $(PTHREAD_LOCAL)
	./$< -a -thread_stack 16384         && \
	./$< -a -thread_stack 16384 -resume $(SUMMARIZE_CMD)
