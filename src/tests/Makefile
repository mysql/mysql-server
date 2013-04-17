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

ifeq ($(CC),icc)
SKIP_WARNING += $(ICC_NOWARN)1418 #Non static functions do not need prototypes.
endif
BDBVGRIND=

TDB_EXTRA_NEEDED=
ifeq ($(OS_CHOICE),windows)
LIBTDB=$(WIN_YDB)
%.tdb$(BINSUF):  $(WIN_YDB)
%.tdb$(BINSUF):  LINK_FILES+=$(WIN_YDB)
TDB_EXTRA_NEEDED=$(WIN_YDB)
else
LIBTDB=../../lib/libtokudb.$(SOEXT)
TLIBTDB=../libtokudbtrace.$(SOEXT)
%.tdb$(BINSUF):  DLINK_FILES+=$(LIBTDB)
%.tdb$(BINSUF):  RPATH_DIRS+=$(patsubst %/,%,$(dir $(LIBTDB)))
endif

RECOVER_SRCS = $(wildcard recover-*.c)
LOADER_SRCS = $(wildcard loader-*.c)
TRANSPARENT_UPGRADE_SRCS = $(wildcard upgrade*.c)
NONSTANDARD_SRCS= \
		  $(RECOVER_SRCS) \
		  $(LOADER_SRCS) \

FAIL3312C_SRCS = filesize.c test3529.c

#SRCS = $(sort $(wildcard *.c))
# To patch out upgrade tests, replace line above with line below,
# and patch out loader-stress-test3 (see below)
#SRCS = $(sort $(filter-out $(TRANSPARENT_UPGRADE_SRCS),$(wildcard *.c)))
# To patch out tests failing on 3312c branch (including upgrade tests), use this:
SRCS = $(sort $(filter-out $(TRANSPARENT_UPGRADE_SRCS),$(filter-out $(FAIL3312C_SRCS),$(wildcard *.c))))


#end

#Tests that don't compile in windows.  SHould
WINDOWS_NOT_PORTED_TESTS = \
        diskfull \
#\ ends prev line

#Tests that are irrelevant in windows.
WINDOWS_DONTRUN_TESTS = \
	$(WINDOWS_NOT_PORTED_TESTS) \
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

TDB_DONTRUN_SRCS = \
	$(wildcard bdb-simple-deadlock*.c) \

TDB_DONTRUN_TESTS = $(patsubst %.c,%,$(TDB_DONTRUN_SRCS))

ifeq ($(OS_CHOICE),windows)
    TDB_BINS = $(patsubst %.c,%.tdb$(BINSUF),$(filter-out $(patsubst %,%.c,$(WINDOWS_DONTRUN_TESTS)),$(SRCS)))
    TDB_TESTS = $(patsubst %.c,%.tdb$(BINSUF),$(filter-out $(patsubst %,%.c,$(WINDOWS_DONTRUN_TESTS)),$(filter-out $(NONSTANDARD_SRCS),$(SRCS))))
else
    TDB_BINS = $(patsubst %.c,%.tdb$(BINSUF),$(filter-out $(TDB_DONTRUN_SRCS),$(SRCS)))
    TDB_TESTS = $(patsubst %.c,%.tdb$(BINSUF),$(filter-out $(NONSTANDARD_SRCS),$(filter-out $(TDB_DONTRUN_SRCS),$(SRCS))))
endif

# For diskfull.bdb: db-4.6 seems OK, but db-4.3 segfaults
BDB_DONTRUN_TESTS = \
        backwards_10_each_le_and_msg \
	blocking-prelock-range \
	blocking-set-range-reverse-0 \
	blocking-table-lock \
        bug1381 \
        bug627 \
	cachetable-race \
        checkpoint_1 \
        checkpoint_callback \
        checkpoint_stress \
        checkpoint_truncate_1 \
	cursor-isolation \
	cursor-set-del-rmw \
	cursor-set-range-rmw \
	del-simple \
	del-multiple \
	del-multiple-huge-primary-row \
	del-multiple-srcdb \
	directory_lock \
        diskfull \
        env-put-multiple \
        env_startup \
	execute-updates \
        filesize \
        helgrind1 \
        helgrind2 \
        helgrind3 \
	hotindexer-bw \
	hotindexer-db-busy \
	hotindexer-error-callback \
	hotindexer-insert-committed-optimized \
	hotindexer-insert-committed \
	hotindexer-insert-provisional \
	hotindexer-lock-test \
	hotindexer-multiclient \
	hotindexer-nested-insert-committed \
	hotindexer-put-abort \
	hotindexer-put-commit \
	hotindexer-put-multiple \
	hotindexer-simple-abort \
	hotindexer-undo-do-test \
	hotindexer-with-queries \
	insert-dup-prelock \
        isolation \
        isolation-read-committed \
        keyrange \
        keyrange-dupsort \
        keyrange-dupsort-unflat \
        keyrange-unflat \
	last-verify-time \
        loader-cleanup-test \
	loader-create-abort \
	loader-create-close \
	loader-db-busy \
        loader-dup-test \
        loader-no-puts \
        loader-reference-test \
	loader-stress-del \
        loader-stress-test \
        loader-tpch-load \
	lock-pressure \
        manyfiles \
	multiprocess \
	mvcc-create-table \
        mvcc-many-committed \
	prelock-read-read \
	prelock-read-write \
	prelock-write-read \
	prelock-write-write \
        powerfail \
	preload-db \
	preload-db-nested \
        progress \
	put-multiple \
        recover-2483 \
        recover-3113 \
        recover-compare-db \
        recover-compare-db-descriptor \
	recover-del-multiple \
	recover-del-multiple-abort \
        recover-del-multiple-srcdb-fdelete-all \
        recover-delboth-after-checkpoint \
        recover-delboth-checkpoint \
        recover-descriptor \
        recover-descriptor2 \
        recover-descriptor3 \
        recover-descriptor4 \
        recover-descriptor5 \
        recover-descriptor6 \
        recover-descriptor7 \
        recover-descriptor8 \
        recover-descriptor9 \
        recover-descriptor10 \
        recover-descriptor11 \
        recover-descriptor12 \
        recover-fclose-in-checkpoint \
        recover-fcreate-basementnodesize \
	recover-ft1 \
	recover-ft2 \
	recover-ft3 \
	recover-ft4 \
	recover-ft5 \
	recover-ft6 \
	recover-ft7 \
	recover-ft8 \
        recover-loader-test \
        recover-lsn-filter-multiple \
	recover-put-multiple \
	recover-put-multiple-abort \
        recover-put-multiple-fdelete-all \
        recover-put-multiple-fdelete-some \
        recover-put-multiple-srcdb-fdelete-all \
        recover-split-checkpoint \
        recover-tablelock \
	recover-test-logsuppress \
	recover-test-logsuppress-put \
	recover-test_stress1 \
	recover-test_stress2 \
	recover-upgrade-db-descriptor-multihandle \
	recover-upgrade-db-descriptor \
	recover-update-multiple \
	recover-update-multiple-abort \
	recover-update_aborts \
	recover-update_aborts_before_checkpoint \
	recover-update_aborts_before_close \
	recover-update_changes_values \
	recover-update_changes_values_before_checkpoint \
	recover-update_changes_values_before_close \
	recover-update_broadcast_aborts \
	recover-update_broadcast_aborts2 \
	recover-update_broadcast_aborts3 \
	recover-update_broadcast_aborts_before_checkpoint \
	recover-update_broadcast_aborts_before_close \
	recover-update_broadcast_changes_values \
	recover-update_broadcast_changes_values2 \
	recover-update_broadcast_changes_values3 \
	recover-update_broadcast_changes_values_before_checkpoint \
	recover-update_broadcast_changes_values_before_close \
	recover-update_changes_values_before_close \
        recovery_fileops_stress \
        recovery_fileops_unit \
        recovery_stress \
        redirect \
	replace-into-write-lock \
        root_fifo_2 \
        root_fifo_32 \
        root_fifo_41 \
	shutdown-3344 \
        stat64 stat64-create-modify-times stat64_flatten stat64-null-txn \
	stress-gc \
        test1324 \
        test1426 \
        test1572 \
	test3219 \
	test3522 \
	test3522b \
        test938c \
        test_3645 \
        test_3755 \
        test_abort1 \
        test_abort4 \
        test_abort5 \
	test_blobs_leaf_split \
	test_bulk_fetch \
        test_cursor_delete_2119 \
        test_db_descriptor \
        test_db_descriptor_named_db \
	test_db_txn_locks_read_uncommitted \
        test_dbremove_old \
        test_dupsort_get_both_range_reverse \
        test_dupsort_set_range_reverse \
	test_large_update_broadcast_small_cachetable \
        test_logflush \
	test_redirect_func \
	test_stress1 \
	test_stress2 \
	test_stress3 \
	test_stress4 \
	test_stress5 \
	test_stress6 \
        test_transactional_descriptor \
        test_trans_desc_during_chkpt \
        test_trans_desc_during_chkpt2 \
        test_trans_desc_during_chkpt3 \
        test_trans_desc_during_chkpt4 \
        test_txn_abort6 \
        test_txn_abort8 \
        test_txn_abort9 \
        test_txn_close_open_commit \
        test_txn_commit8 \
        test_txn_nested1 \
        test_txn_nested2 \
        test_txn_nested3 \
        test_txn_nested4 \
        test_txn_nested5 \
	test_update_abort_works \
	test_update_calls_back \
	test_update_can_delete_elements \
	test_update_changes_values \
	test_update_nonexistent_keys \
	test_update_previously_deleted \
	test_update_stress \
	test_update_txn_snapshot_works_concurrently \
	test_update_txn_snapshot_works_correctly_with_deletes \
	test_update_broadcast_abort_works \
	test_update_broadcast_calls_back \
	test_update_broadcast_can_delete_elements \
	test_update_broadcast_changes_values \
	test_update_broadcast_previously_deleted \
	test_update_broadcast_stress \
	test_update_broadcast_update_fun_has_choices \
	test_update_broadcast_with_empty_table \
        test_update_broadcast_indexer \
	test_update_broadcast_loader \
	test_update_broadcast_nested_updates \
	test_update_nested_updates \
	test_update_with_empty_table \
	txn-ignore \
        transactional_fileops \
	update-multiple-data-diagonal \
	update-multiple-key0 \
	update-multiple-nochange \
	update-multiple-with-indexer \
	update \
        upgrade_simple \
	upgrade-test-1 \
	upgrade-test-2 \
	upgrade-test-3 \
	upgrade-test-4 \
	upgrade-test-5 \
        upgrade-test-6 \
        upgrade-test-7 \
        zombie_db \
#\ ends prev line

# checkpoint tests depend on this header file, 
checkpoint_1.tdb$(BINSUF) checkpoint_stress.tdb$(BINSUF): checkpoint_test.h
# dependency only, not a rule (do not insert a tab at this line)

DEPENDS_ON_STRESS_HELPERS = \
 recover-ft1 \
 recover-ft2 \
 recover-ft3 \
 recover-ft4 \
 recover-ft5 \
 recover-ft6 \
 recover-ft7 \
 recover-ft8 \
 recover-test_stress1 \
 recover-test_stress2 \
 test_stress1 \
 test_stress2 \
 test_stress3 \
 test_stress4 \
 test_stress5 \
 test_stress6 \
 #blank
$(patsubst %,%.tdb,$(DEPENDS_ON_STRESS_HELPERS)): threaded_stress_test_helpers.h


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

ifeq ($(OS_CHOICE),windows)
BDB_TESTS=
BDB_BINS=
else
BDB_TESTS = $(patsubst %.c,%.bdb$(BINSUF),$(filter-out $(patsubst %,%.c,$(BDB_DONTRUN_TESTS)),$(SRCS)))
BDB_BINS = $(patsubst %.c,%.bdb$(BINSUF),$(filter-out $(patsubst %,%.c,$(BDB_DONTRUN_TESTS)),$(SRCS)))
endif

TDB_TESTS_THAT_SHOULD_FAIL= \
	test_blobs_leaf_split \
	test_truncate_txn_abort \
	test_db_no_env \
#\ ends prev line
#ifneq ($(OS_CHOICE),windows)
#    TDB_TESTS_THAT_SHOULD_FAIL+= \
#\ ends prev line
#endif

TDB_TESTS_THAT_SHOULD_FAIL_LIT= \
	test_log8.recover \
	test_log9.recover \
	test_log10.recover \
	recover-missing-dbfile.abortrecover \
	recover-missing-dbfile-2.abortrecover \
	recover-missing-dbfile-2.abortrecover \
	recover-dbopen-eclose.abortrecover \
	loader-tpch-load.loader \
#\ ends prev line

ifeq ($(OS_CHOICE),windows)
    #Tests that fail in windows but shouldn't.  TODO: Fix each one of these and remove from the list!
    WIN_IGNORE_TDB_TESTS_THAT_SHOULD_FAIL += \
#\ ends prev line
    WIN_IGNORE_TDB_TESTS_THAT_SHOULD_FAIL_LIT += \
#\ ends prev line
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
	test_groupcommit_count_hgrind.tdbrun \
	test_groupcommit_count_vgrind.tdbrun \
#\ ends prev line

ifeq ($(OS_CHOICE),windows)
EXTRA_TDB_TESTS = \
	$(patsubst %,test_log%.recover,$(filter-out $(patsubst %,%.c,$(WINDOWS_DONTRUN_TESTS)),$(TLRECOVER))) \
#\ ends prev line
endif

RECOVER_TESTS = $(patsubst %.c,%.abortrecover,$(RECOVER_SRCS))
LOADER_TESTS = $(patsubst %.c,%.loader,$(LOADER_SRCS))
ifeq ($(OS_CHOICE),windows)
RECOVER_TESTS = $(patsubst %.c,%.abortrecover,$(filter-out $(patsubst %,%.c,$(WINDOWS_DONTRUN_TESTS)),$(RECOVER_SRCS)))
LOADER_TESTS = $(patsubst %.c,%.loader,$(filter-out $(patsubst %,%.c,$(WINDOWS_DONTRUN_TESTS)),$(LOADER_SRCS)))
endif


RUN_TDB_TESTS = $(patsubst %.tdb$(BINSUF),%.tdbrun,$(TDB_TESTS))  $(EXTRA_TDB_TESTS) $(RECOVER_TESTS) $(LOADER_TESTS)
RUN_BDB_TESTS  = $(patsubst %.bdb$(BINSUF),%.bdbrun,$(BDB_TESTS))
RUN_ALL_TESTS  = $(RUN_BDB_TESTS) $(RUN_TDB_TESTS)

MORE_BDB_BINS = test1426.bdb$(BINSUF)

ALL_BINS = $(TDB_BINS) $(BDB_BINS) $(MORE_BDB_BINS)

.PHONY: default all check tests check.lin check.tlog check.log tests.lin tests.log tests.tlog

default all build: $(ALL_BINS)

ifndef TOKUDBONLY
MAYBE_CHECK_BDB = check.bdb
else
MAYBE_CHECK_BDB =
endif
check: check.tdb $(MAYBE_CHECK_BDB) ;

tests: tests.bdb tests.tdb ;
tests.bdb: $(BDB_BINS) ;
check.bdb: $(RUN_BDB_TESTS) ;
tests.tdb: $(TDB_BINS) ;
check.tdb: $(LIBTDB) $(PTHREAD_LOCAL) $(RUN_TDB_TESTS) ;
check.tdb.extra: $(EXTRA_TDB_TESTS) ;
check.abortrecover: $(RECOVER_TESTS) ;

.PHONY: fastcheck.tdb fastcheckonlyfail.tdb
fastcheck.tdb: $(patsubst %,%.fast,$(RUN_TDB_TESTS))
fastcheckonlyfail.tdb: $(patsubst %,%.fastonlyfail,$(RUN_TDB_TESTS))

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
# I want the failing tests to send their stderr to /dev/null
$(SHOULD_FAIL): VGRIND+= --log-file=/dev/null
# $(SHOULD_FAIL): INVERTER= 2> /dev/null ;test $$? -ne 0

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
%.tdbrun: %.tdb$(BINSUF) $(DEPEND_COMPILE) $(DEPEND_LINK) $(LIBTDB) $(PTHREAD_LOCAL)
	$(TDBVGRIND) ./$< $(VERBVERBOSE) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

ifeq ($(OS_CHOICE),windows)
    TDBDUMP=$(TOKUROOT)utils/tokudb_dump$(BINSUF)
    TDBLOAD=$(TOKUROOT)utils/tokudb_load$(BINSUF)
else
    TDBDUMP=$(TOKUROOT)utils/tokudb_dump_static$(BINSUF)
    TDBLOAD=$(TOKUROOT)utils/tokudb_load_static$(BINSUF)
endif
TDBRECOVER=$(TOKUROOT)newbrt/tdb-recover$(BINSUF)
TDBBRTDUMP=$(TOKUROOT)newbrt/brtdump$(BINSUF)

$(TDBBRTDUMP) $(TDBLOAD) $(TDBDUMP) $(TDBRECOVER):
	cd $(@D) && $(MAKE) $(@F)

%.recover: %.tdb$(BINSUF) $(PTHREAD_LOCAL) $(TDBRECOVER) $(TDBDUMP)
	echo doing ./$< &&\
	./$< --no-shutdown                                                         && \
	rm -rf dir.$*.c.tdb.recover                                                && \
	mkdir dir.$*.c.tdb.recover                                                 && \
	cp dir.$*.c.tdb/tokudb.directory   dir.$*.c.tdb.recover/                   && \
	cp dir.$*.c.tdb/tokudb.environment dir.$*.c.tdb.recover/                   && \
	cp dir.$*.c.tdb/tokudb.rollback    dir.$*.c.tdb.recover/                   && \
	cp dir.$*.c.tdb/*.tokulog*         dir.$*.c.tdb.recover/                   && \
	echo doing recovery &&\
	$(VGRIND) $(TDBRECOVER) dir.$*.c.tdb.recover dir.$*.c.tdb.recover         && \
	echo dump and compare &&\
	$(TDBDUMP) -h dir.$*.c.tdb         foo.db > dir.$*.c.tdb/foo.dump          && \
	$(TDBDUMP) -h dir.$*.c.tdb.recover foo.db > dir.$*.c.tdb.recover/foo.dump  && \
	diff -q dir.$*.c.tdb/foo.dump dir.$*.c.tdb.recover/foo.dump                                   \
	$(MAYBEINVERTER) $(SUMMARIZE_CMD)

%.recoverwc: %.tdb$(BINSUF) $(PTHREAD_LOCAL)
	 (cd dir.$*.c.tdb;pwd;cat log*| ../../../newbrt/tdb_logprint |wc -c)

ABORTCODE=137
%.abortrecover: %.tdb$(BINSUF) $(PTHREAD_LOCAL)
	{ ./$< --test ; } ; \
        if [ $$? -ne $(ABORTCODE) ] ; then \
		echo $* --test did not return $(ABORTCODE); \
		test 0 = 1; \
        else \
		$(VGRIND) ./$< --recover; \
	fi \
	$(MAYBEINVERTER) $(SUMMARIZE_CMD)

.PHONY: %.loader
# Generally these subtests cannot run in parallel unless they support an environment arg
%.loader: %.tdb$(BINSUF) $(PTHREAD_LOCAL)
	$(TDBVGRIND) ./$< $(EXTRA_ARGS) $(VERBVERBOSE) $(MAYBEINVERTER) $(SUMMARIZE_CMD)
	$(TDBVGRIND) ./$< $(EXTRA_ARGS) -p $(VERBVERBOSE) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

.PHONY: %.recover
all.recover: $(patsubst %,test_log%.recover,$(TLRECOVER)) $(PTHREAD_LOCAL) ;

#DISABLE standard tdbrun for recover tests.
$(patsubst %,test_log%.tdbrun,$(TLRECOVER)): ;

.PHONY: %.fast %.fastonlyfail
%.fast: $(DEPEND_COMPILE) $(DEPEND_LINK) $(LIBTDB) $(PTHREAD_LOCAL)
	@(perl -e 'alarm ($(FASTCHECKTIMEOUT) * 60) ; exec @ARGV' $(MAKE) VGRIND= SUMMARIZE=0 $*) &>$*.fastlog ; \
		if test $$? != 0 ; then \
			printf "%-60s FAIL\nexamine %s\n" $* $*.fastlog; \
			exit 2 ; \
		else \
			printf "%-60s PASS\n" $* ; \
			$(RM) $*.fastlog ; \
		fi
%.fastonlyfail: $(DEPEND_COMPILE) $(DEPEND_LINK) $(LIBTDB) $(PTHREAD_LOCAL)
	@(perl -e 'alarm ($(FASTCHECKTIMEOUT) * 60) ; exec @ARGV' $(MAKE) VGRIND= SUMMARIZE=0 $*) &>$*.fastlog ; \
		if test $$? != 0 ; then \
			printf "%-60s FAIL\nexamine %s\n" $* $*.fastlog; \
			exit 2 ; \
		else \
			$(RM) $*.fastlog ; \
		fi

BDB_CFLAGS=
TDB_CFLAGS=
%.bdb$(BINSUF): BDB_CFLAGS= -DENVDIR=\"dir.$<.bdb\" -DUSE_BDB -DIS_TDB=0 -DTOKU_ALLOW_DEPRECATED

%.bdb$(BINSUF): %.c $(DEPEND_COMPILE) $(DEPEND_LINK)
	$(CC) $< $(BDB_CFLAGS) $(BIN_FROM_C_FLAGS) -L$(TOKUROOT)lib -Wl,-rpath,$(shell pwd)/$(TOKUROOT)lib -ltokuportability $(LINK_MUST_BE_LAST)

%.tdbt$(BINSUF): DLINK_FILES+=$(TLIBTDB)
%.tdbt$(BINSUF): RPATH_DIRS+=$(dir $(TLIBTDB))

%.tdb$(BINSUF) %.tdbt$(BINSUF): TDB_CFLAGS= -DENVDIR=\"dir.$<.tdb\" -DUSE_TDB -DIS_TDB=1
%.tdb$(BINSUF) %.tdbt$(BINSUF): CPPFLAGS+=-I$(TOKUROOT)include
%.tdb$(BINSUF) %.tdbt$(BINSUF): LDFLAGS+=-ltokuportability
loader-stress-test: CPPFLAGS+=-I$(TOKUROOT)include -DENVDIR=\"dir.$<\"
loader-stress-test: LOADLIBES+=-L../lib -ltokudb -Wl,-rpath,..

%.tdb$(BINSUF) %.tdbt$(BINSUF): %.c $(DEPEND_COMPILE) $(DEPEND_LINK) $(TDB_EXTRA_NEEDED)
	$(CC) $< $(TDB_CFLAGS) $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)

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


# Don't run valgrind on the diskfull test (since it aborts)
diskfull.tdbrun: VGRIND=
stress-gc.tdbrun: VGRIND=
insert-dup-prelock: VGRIND=
test_3645: VGRIND=
test_large_update_broadcast_small_cachetable.tdbrun: VGRIND=
test_update_broadcast_stress.tdbrun: VGRIND=
test_update_stress.tdbrun: VGRIND=
stress-test.tdbrun: VGRIND=
stress-test.bdbrun: VGRIND=


libs:
	cd ..;$(MAKE)





loader-cleanup-test.tdb$(BINSUF) diskfull.tdb$(BINSUF): CPPFLAGS+=-DDONT_DEPRECATE_WRITES
test_db_curs4.tdb$(BINSUF): trace.h
test_db_curs4.bdb$(BINSUF): trace.h
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
# we use the -x flag (no transactions or recovery) in the dump program.  (-x is not in the BDB version of dump, but it's in ours)
test1426.tdbrun: test1426.tdb$(BINSUF) dump.bdb.1426 $(PTHREAD_LOCAL) $(TDBDUMP)
	(./test1426.tdb$(BINSUF) -q && \
	 $(TDBDUMP) -x -p -h dir.test1426.c.tdb main > dump.tdb.1426 && \
	 diff -q -I db_pagesize=4096 dump.bdb.1426 dump.tdb.1426 ) \
	 $(MAYBEINVERTER) $(SUMMARIZE_CMD)

# STRESS RUNS is number of iterations, STRESS_SIZE is operations per iteration (must not change between iterations)
# Note the magic with the brackets is there to suppress the printout of "/bin/bash: line 11: 1234 Killed ...."
# See http://stackoverflow.com/questions/714855/bash-suppress-notice-of-forked-command-being-killed
# also we only print the date if VERBOSE is set to non-null
STRESS_RUNS=5
STRESS_SIZE=5001
checkpoint_stress.tdbrun: SHELL=/bin/bash
checkpoint_stress.tdbrun: checkpoint_stress.tdb$(BINSUF) $(PTHREAD_LOCAL)
	$(VGRIND) ./$< -C -n $(STRESS_SIZE) $(VERBVERBOSE) && \
	($(VGRIND) ./$< -C -i 0 -n $(STRESS_SIZE) $(VERBVERBOSE) && \
	 for (( i = 1; i < $(STRESS_RUNS); i++ )); do \
		$(if $(VERBOSE),date;) \
		{ ./$< -c -i $$i -n $(STRESS_SIZE) $(VERBVERBOSE) 2> dir.checkpoint_stress.c.tdb/error.$$i ; } 2>/dev/null ; \
                if [ $$? -ne $(ABORTCODE) ] ; then \
                   exit 1 ; \
                fi ; \
		if ! grep -q 'HAPPY CRASH' dir.checkpoint_stress.c.tdb/error.$$i; then break; fi; \
	done && \
	test `grep -l 'HAPPY CRASH' dir.checkpoint_stress.c.tdb/error.* |wc -l` = $$(($(STRESS_RUNS)-1)) \
	) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

DISKFULL_ABORTCODE=134

diskfull.tdbrun: SHELL=/bin/bash
diskfull.tdbrun: diskfull.tdb$(BINSUF) $(PTHREAD_LOCAL)
	NUM_WRITES=$$($(VGRIND) ./$< -q) && \
	(for (( i = 0; i < $$NUM_WRITES; i++ )); do \
		{ ./$< -C $$i $(VERBVERBOSE) 2>/dev/null; } 2>/dev/null; \
		if [ $$? -ne $(DISKFULL_ABORTCODE) ] ; then \
		    echo "Diskfull (iteration $$i) did not abort" ; \
		    exit 1; \
		fi; \
	done; ) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

recover_stress.tdbrun: SHELL=/bin/bash
recover_stress.tdbrun: checkpoint_stress.tdb$(BINSUF) $(PTHREAD_LOCAL)
	$(VGRIND) ./$< -C -n $(STRESS_SIZE) -l $(VERBVERBOSE) && \
	($(VGRIND) ./$< -C -i 0 -n $(STRESS_SIZE) -l $(VERBVERBOSE) && \
	 for (( i = 1; i < $(STRESS_RUNS); i++ )); do \
		$(if $(VERBOSE),date;) \
		{ ./$< -c -i $$i -n $(STRESS_SIZE) -l $(VERBVERBOSE) 2> dir.checkpoint_stress.c.tdb/error.$$i ; } 2>/dev/null ; \
                if [ $$? -ne $(ABORTCODE) ] ; then \
                   exit 1 ; \
                fi ; \
		if ! grep -q 'HAPPY CRASH' dir.checkpoint_stress.c.tdb/error.$$i; then break; fi; \
	done && \
	test `grep -l 'HAPPY CRASH' dir.checkpoint_stress.c.tdb/error.* |wc -l` = $$(($(STRESS_RUNS)-1)) \
	) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

recovery_fileops_unit.tdbrun: SHELL=/bin/bash
recovery_fileops_unit.tdbrun: recovery_fileops_unit.tdb$(BINSUF) $(PTHREAD_LOCAL)
	rm -rf dir.$@.errors
	mkdir dir.$@.errors
	$(MAYBEATSIGN)( \
        Oset="c d r"; \
        aset="0 1"; \
        bset="0 1"; \
        cset="0 1 2"; \
        fset="0 1"; \
        count=0; \
        for O in $$Oset; do \
           if test $$O = c ; then \
              gset="0"; \
              hset="0"; \
           else \
              gset="0 1 2 3 4 5"; \
              hset="0 1"; \
           fi; \
           for a in $$aset; do \
              for b in $$bset; do \
                 if test $$b -eq 0 ; then \
                    dset="0"; \
                    eset="0"; \
                 else \
                    dset="0 1"; \
                    eset="0 1"; \
                 fi; \
                 for c in $$cset; do \
                    for d in $$dset; do \
                       for e in $$eset; do \
                          for f in $$fset; do \
                             for g in $$gset; do \
                                for h in $$hset; do \
                                   if test \( $$O != c \) -a \( $$c -eq 0 \) -a \( \( $$b -eq 0 \) -o \( $$e -eq 0 \) -o  \( $$d -eq 1 \)  \) ; then \
                                      iset="0 1"; \
                                   else \
                                      iset="0"; \
                                   fi; \
                                   for i in $$iset; do \
                                      errorfile=dir.$@.errors/crash.$$O.$$a.$$b.$$c.$$d.$$e.$$f.$$g.$$h.$$i; \
                                      combination="-O $$O -A $$a -B $$b -C $$c -D $$d -E $$e -F $$f -G $$g -H $$h -I $$i"; \
                                      { ./$< $(VERBVERBOSE) -c $$combination 2> $$errorfile ; } 2>/dev/null ;\
                                      if [ $$? -ne $(ABORTCODE) ] ; then \
                                         { cat dir.$@.errors/crash; echo Error: no crash in $$errorfile ; exit 1; }; \
                                      fi ; \
                                      grep 'HAPPY CRASH' $$errorfile >/dev/null || { cat $$errorfile; echo Error: incorrect crash in $$errorfile ; exit 1; }; \
                                      ./$< $(VERBVERBOSE) -r $$combination 2>> $$errorfile || { cat $$errorfile ; echo Error: during recovery in $$errorfile ; exit 1; }; \
                                      count=$$(($$count + 1)); \
                                   done; \
                                done; \
                             done; \
                          done; \
                       done; \
                    done; \
                 done; \
              done; \
           done; \
        done; \
	(test -n "$(VERBOSE)" && echo $$count tests passed) || true; \
	) $(SUMMARIZE_CMD)

# helgrind1 is supposed to fail.
helgrind1.tdbrun: TDBVGRIND=$(HGRIND) --log-file=helgrind1.tdb.deleteme
helgrind1.tdbrun: MAYBEINVERTER=$(INVERTER)
helgrind2.bdbrun: BDBVGRIND=$(HGRIND)
helgrind2.tdbrun helgrind3.tdbrun: TDBVGRIND=$(HGRIND)
helgrind2.tdbrun helgrind3.tdbrun: HGRIND+=--suppressions=helgrind.suppressions
helgrind3.bdbrun: BDBVGRIND=$(HGRIND)


test_groupcommit_count_hgrind.tdbrun: HGRIND+=--suppressions=helgrind.suppressions
test_groupcommit_count_hgrind.tdbrun: test_groupcommit_count.tdb$(BINSUF)
	$(HGRIND) ./test_groupcommit_count.tdb$(BINSUF) $(VERBVERBOSE) -n 1 -p hgrind $(SUMMARIZE_CMD)
test_groupcommit_count_vgrind.tdbrun: test_groupcommit_count.tdb$(BINSUF)
	$(VGRIND) ./test_groupcommit_count.tdb$(BINSUF) $(VERBVERBOSE) -n 1 -p vgrind $(SUMMARIZE_CMD)

# we deliberately don't close the env, so recovery runs
# lets avoid all of the valgrind errors
test-recover1.tdbrun: VGRIND=
test-recover2.tdbrun: VGRIND=
test-recover3.tdbrun: VGRIND=

# filesize is too slow with vgrind.
filesize.tdbrun: VGRIND=

powerfail.tdbrun:
	@echo must run powerfail by hand $(SUMMARIZE_CMD)

dumpit: $(TDBBRTDUMP)
	$(TDBBRTDUMP)  dir.test_log5.c.tdb.recover/foo.db > dump.r && $(TDBBRTDUMP) dir.test_log5.c.tdb/foo.db > dump.o && diff -q dump.o dump.r

# test on small stacks
test_thread_stack.%run: test_thread_stack.%$(BINSUF) $(PTHREAD_LOCAL)
	./$< -a -thread_stack 16384         && \
	./$< -a -thread_stack 16384 -resume $(SUMMARIZE_CMD)

preload-db-nested.tdbrun: VGRIND=
upgrade-test-4.tdbrun: VGRIND=

ROOT_FIFO_41_RUNS = $(foreach num, $(shell seq 1 100), root_fifo_41_$(num)_populate.tdbrun root_fifo_41_$(num)_nopopulate.tdbrun)
root_fifo_41.tdbrun: $(ROOT_FIFO_41_RUNS)
	@echo -n
root_fifo_41_%_populate.tdbrun: root_fifo_41.tdb
	$(VGRIND) ./$< -n $(patsubst root_fifo_41_%_populate.tdbrun, %, $@)   -h $(patsubst %.tdbrun, %.tdbdir, $@) -populate $(SUMMARIZE_CMD)
	@rm -rf $(patsubst %.tdbrun, %.dir, $@) 
root_fifo_41_%_nopopulate.tdbrun: root_fifo_41.tdb
	$(VGRIND) ./$< -n $(patsubst root_fifo_41_%_nopopulate.tdbrun, %, $@) -h $(patsubst %.tdbrun, %.tdbdir, $@)           $(SUMMARIZE_CMD)
	@rm -rf $(patsubst %.tdbrun, %.dir, $@) 

test3039.tdbrun: test3039.tdb
	( ($(VGRIND) ./$< -v -n 1000 ) && ./$< -v ) $(SUMMARIZE_CMD)

# loader tests
# re-enable loader-stress-test3 when upgrade is complete
#loader-stress-test.loader: $(patsubst %,loader-stress-test%.tdbrun, 0 1 2 3)
loader-stress-test.loader: $(patsubst %,loader-stress-test%.tdbrun, 0 1 2)
	true

loader-stress-test0.tdbrun: loader-stress-test.tdb$(BINSUF)
	$(TDBVGRIND) ./$< $(EXTRA_ARGS) -e 0 -c $(VERBVERBOSE) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

loader-stress-test1.tdbrun: loader-stress-test.tdb$(BINSUF)
	$(TDBVGRIND) ./$< $(EXTRA_ARGS) -e 1 -c -p $(VERBVERBOSE) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

loader-stress-test2.tdbrun: loader-stress-test.tdb$(BINSUF) 
	$(TDBVGRIND) ./$< $(EXTRA_ARGS) -e 2 -r 5000 -s $(VERBVERBOSE) $(MAYBEINVERTER) $(SUMMARIZE_CMD) # to manifest #2717.

loader-stress-test3.tdbrun: loader-stress-test.tdb$(BINSUF)
	$(TDBVGRIND) ./$< $(EXTRA_ARGS) -e 3 -u -c $(VERBVERBOSE) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

loader-dup-test.loader loader-dup-test.tdbrun: $(patsubst %,loader-dup-test%.tdbrun, 0 1 2 3 4 5)
	true $(SUMMARIZE_CMD) #State that this has passed.  Override the default rule.

loader-dup-test0.tdbrun: loader-dup-test.tdb$(BINSUF)
	$(VGRIND) ./loader-dup-test.tdb -e 0 $(SUMMARIZE_CMD)

loader-dup-test1.tdbrun: loader-dup-test.tdb$(BINSUF)
	$(VGRIND) ./loader-dup-test.tdb -d 1 -r 500000 -e 1 $(SUMMARIZE_CMD)

loader-dup-test2.tdbrun: loader-dup-test.tdb$(BINSUF)
	$(VGRIND) ./loader-dup-test.tdb -d 1 -r 1000000 -e 2 $(SUMMARIZE_CMD)

loader-dup-test3.tdbrun: loader-dup-test.tdb$(BINSUF)
	$(VGRIND) ./loader-dup-test.tdb -d 1 -s -r 100 -e 3 $(SUMMARIZE_CMD)

loader-dup-test4.tdbrun: loader-dup-test.tdb$(BINSUF)
	$(VGRIND) ./loader-dup-test.tdb -d 1 -s -r 1000 -e 4 $(SUMMARIZE_CMD)

loader-dup-test5.tdbrun: loader-dup-test.tdb$(BINSUF)
	$(VGRIND) ./loader-dup-test.tdb -d 1 -s -r 1000 -E -e 5 $(SUMMARIZE_CMD)

loader-cleanup-test.loader: $(patsubst %,loader-cleanup-test%.tdbrun, 0 1)
	true

loader-cleanup-test0.tdbrun: loader-cleanup-test.tdb$(BINSUF)
	$(VGRIND) ./$< -s -r 8000 -e 0 $(SUMMARIZE_CMD)

loader-cleanup-test1.tdbrun: loader-cleanup-test.tdb$(BINSUF)
	$(VGRIND) ./$< -s -r 8000 -p -e 1 $(SUMMARIZE_CMD)

LOADER_USE_DEFAULT_TESTS = create-abort create-close no-puts reference-test stress-test tpch-load

loader-tests: $(LOADER_TESTS)
	echo $(LOADER_TESTS)

# hot indexer tests
HOTINDEXER_UNDO_TESTS = $(wildcard hotindexer-undo-do-tests/*.test)
CHECK_HOTINDEXER_UNDO_TESTS = $(patsubst %.test,%.run,$(HOTINDEXER_UNDO_TESTS))

hotindexer-undo-do-test.tdbrun: $(CHECK_HOTINDEXER_UNDO_TESTS)
	true

$(CHECK_HOTINDEXER_UNDO_TESTS): %.run: %.test hotindexer-undo-do-test.tdb$(BINSUF) 
	./run-hotindexer-undo-do-tests.bash $< $(SUMMARIZE_CMD)

# blocking lock tree tests
BLOCKING_SRCS = $(wildcard blocking-*.c)
BLOCKING_TDB_TESTS = $(patsubst %.c,%.tdbrun,$(BLOCKING_SRCS))
BLOCKING_BDB_TESTS = $(patsubst %.c,%.bdbrun,$(BLOCKING_SRCS))

check_blocking_tdb: $(BLOCKING_TDB_TESTS)

check_blocking_bdb: $(BLOCKING_BDB_TESTS)

# stress tests with drd
STRESS_TEST_SRCS = $(wildcard test_stress*.c)
STRESS_TESTS = $(patsubst %.c,%,$(STRESS_TEST_SRCS))
TINY_STRESS_TESTS = $(patsubst %,tiny_%.drdrun,$(STRESS_TESTS))
MID_STRESS_TESTS = $(patsubst %,mid_%.drdrun,$(STRESS_TESTS))
LARGE_STRESS_TESTS = $(patsubst %,large_%.drdrun,$(STRESS_TESTS))

stress_tests.drdrun: $(TINY_STRESS_TESTS) \
	mid_test_stress1.drdrun mid_test_stress3.drdrun \
	large_test_stress1.drdrun large_test_stress3.drdrun
	true

tiny_stress_tests.drdrun: $(TINY_STRESS_TESTS)
	true

tiny_%.drdrun: %.tdb
	$(VALGRIND) --tool=drd --quiet --suppressions=drd.suppressions --error-exitcode=1 ./$< $(VERBVERBOSE) --num_seconds 5 --num_elements 150 --envdir dir.$@ \
	$(MAYBEINVERTER) $(SUMMARIZE_CMD)

mid_%.drdrun: %.tdb
	./$< --only_create --envdir dir.$@ --num_elements 10000 && \
	$(VALGRIND) --tool=drd --quiet --suppressions=drd.suppressions --error-exitcode=1 ./$< $(VERBVERBOSE) --only_stress --num_seconds 100 --envdir dir.$@ --num_elements 10000 \
	$(MAYBEINVERTER) $(SUMMARIZE_CMD)

large_%.drdrun: %.tdb
	./$< --only_create --envdir dir.$@ && \
	$(VALGRIND) --tool=drd --quiet --suppressions=drd.suppressions --error-exitcode=1 ./$< $(VERBVERBOSE) --only_stress --num_seconds 1000 --envdir dir.$@ \
	$(MAYBEINVERTER) $(SUMMARIZE_CMD)

# upgrade tests
UPGRADE_TEST_SRCS = $(wildcard upgrade-test-*.c)
UPGRADE_TESTS = $(patsubst %.c,%,$(UPGRADE_TEST_SRCS))
UPGRADE_TESTS_VALGRIND = $(patsubst upgrade-test-%,upgrade-test-%.valgrind,$(UPGRADE_TESTS))

upgrade-tests.tdbrun: $(UPGRADE_TESTS_VALGRIND)
	echo $(UPGRADE_TESTS_VALGRIND)
	true

upgrade-test-%.valgrind: upgrade-test-%.tdb
	$(VGRIND) ./$< $(VERBVERBOSE) $(SUMMARIZE_CMD)

clean:
	rm -f $(ALL_BINS)
	rm -rf dir.* *.check.output *.check.valgrind
	rm -f dump.bdb.1426 dump.tdb.1426 test1426.bdb
	rm -f *.bdb *.tdb
	rm -f *.fastlog
