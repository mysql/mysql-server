# On OSX do:
#   make OSX=OSX

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

ifdef BDBDIR
BDB_CPPFLAGS = -I$(BDBDIR)/include
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb
endif

ifneq ($(OSX),)
#Note: OSX 10.4 needs DYLD_LIBRARY_PATH.  OSX 10.5 claims to support -rpath.
 CFLAGS = -DOSX
 LIBEXT=dylib
 VGRIND=
 HGRIND=
# SETTOKUENV=export DYLD_LIBRARY_PATH=.. ;
# UNSETTOKUENV=unset DYLD_LIBRARY_PATH ;
else
 CFLAGS =
# SETTOKUENV=
# UNSETTOKUENV=
 LIBEXT=so
 VGRIND=valgrind --quiet --error-exitcode=1 --leak-check=full --show-reachable=yes
 HGRIND=valgrind --quiet --tool=helgrind
endif

 ifdef BDBDIR
  BDB_LDFLAGS += -Wl,-rpath,$(BDBDIR)/lib
 endif
 BDB_LDFLAGS += -lpthread
 TDB_LOADLIBES = -L.. -ltokudb -Wl,-rpath,.. -lpthread
 TDB_TRACELOADLIBES = -L.. -ltokudbtrace -Wl,-rpath,.. -lpthread

ifeq ($(VGRIND),)
  BDB_SUPPRESSIONS =
else
  BDB_SUPPRESSIONS = --suppressions=bdb.supressions --gen-suppressions=all
endif

# VERBOSE=true

LIBNAME=libdb.$(LIBEXT)
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS += -Wall -Werror $(OPTFLAGS) -g3 -ggdb3 $(GCOV_FLAGS)
TDB_CPPFLAGS = -I../../include

SRCS = $(sort $(wildcard *.c))

TDB_TESTS = $(patsubst %.c,%.tdb,$(SRCS))
BDB_DONTRUN = bug627 test_abort1 keyrange keyrange-unflat keyrange-dupsort keyrange-dupsort-unflat manyfiles test_938c
BDB_TESTS = $(patsubst %.c,%.bdb,$(filter-out $(patsubst %,%.c,$(BDB_DONTRUN)),$(SRCS)))

TDB_TESTS_THAT_SHOULD_FAIL = test_groupcommit_count test-recover1 test-recover2 test-recover3 test_txn_recover3 test944
TDB_TESTS_THAT_SHOULD_FAIL_LIT = test_log2.recover test_log3.recover test_log4.recover test_log5.recover test_log6.recover test_log7.recover test_log8.recover test_log9.recover test_log10.recover

ALL_TESTS = $(TDB_TESTS) $(BDB_TESTS)

RUN_TDB_TESTS = $(patsubst %.tdb,%.tdbrun,$(TDB_TESTS))
RUN_BDB_TESTS = $(patsubst %.bdb,%.bdbrun,$(BDB_TESTS))
MORE_TESTS = test_v6_v7_assoc3.tdbrun
RUN_ALL_TESTS = $(RUN_TDB_TESTS) $(RUN_BDB_TESTS) $(MORE_TESTS)

all build: $(ALL_TESTS)

#traces: test_env_open_flags.tdbt
traces: $(patsubst %.tdb,%.tdbt,$(TDB_TESTS))

foo:
	echo RUN_TDB_TESTS: $(RUN_TDB_TESTS)
	echo ALL_TESTS: $(ALL_TESTS)

.PHONY: check check.bdb check.tdb
check: check.tdb test_db_assoc3.tdbrun_wasbad check.bdb
tests: tests.bdb tests.tdb
tests.bdb: $(BDB_TESTS)
check.bdb: $(RUN_BDB_TESTS)
tests.tdb: $(TDB_TESTS)
check.tdb: $(RUN_TDB_TESTS) all.recover

# Need these rule so that Make knows about all the file names
.PHONY: %.run
$(RUN_ALL_TESTS):
$(ALL_TESTS):

%.run: %.bdbrun %.tdbrun
	@ echo ok

ifeq ($(VERBOSE),2)
VERBVERBOSE=-v
VERBQUIET=
else
 ifeq ($(VERBOSE),1)
  VERBVERBOSE=
  VERBQUIET=
 else
  VERBVERBOSE=
  VERBQUIET=--quiet
 endif
endif


SHOULD_FAIL = $(TDB_TESTS_THAT_SHOULD_FAIL_LIT) $(patsubst %,%.tdbrun,$(TDB_TESTS_THAT_SHOULD_FAIL))

# Any test that should fail, we invert the result by using MAYBEINVERTER
$(SHOULD_FAIL): MAYBEINVERTER=;test $$? -ne 0

HERE = src/tests
ifeq ($(SUMMARIZE),1)
SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sPASS\n" $(HERE)/$@; else printf "%-60sFAIL\n" $(HERE)/$@ ; test 0 = 1; fi
$(SHOULD_FAIL): SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sXFAIL\n" $(HERE)/$@; else printf "%-60sXPASS\n" $(HERE)/$@ ; test 0 = 1; fi
else
SUMMARIZE_CMD =
endif

# Use -s on the command line to make things quiet.
%.bdbrun: %.bdb ../libtokudb.$(LIBEXT)
	$(UNSETTOKUENV) ./$< $(VERBVERBOSE) $(SUMMARIZE_CMD)
%.tdbrun: %.tdb ../libtokudb.$(LIBEXT)
	$(SETTOKUENV) $(VGRIND) ./$< $(VERBVERBOSE) $(MAYBEINVERTER) $(SUMMARIZE_CMD)

# Don't include log2 log3 log4 log5 etc since they are covered by all.recover

# Don't run valgrind on the groupcommit performance tests
test_groupcommit_perf.bdbrun test_groupcommit_perf.tdbrun: VGRIND=
# Use helgrind on the group commit count test
# helgrind is too flakey, so I'm removing it from the tests. -Bradley
#test_groupcommit_count_helgrind.tdbrun: test_groupcommit_count_helgrind.tdb
#	$(SETTOKUENV) $(HGRIND) ./$< $(VERBVERBOSE)



libs:
	cd ..;make

%.bdb: %.c
	$(UNSETTOKUENV) $(CC) -DENVDIR=\"dir.$<.bdb\" $(BDB_CPPFLAGS) -DUSE_BDB -DIS_TDB=0 $(CFLAGS) $< $(BDB_LDFLAGS) -ldb -o $@
%.tdb: %.c
	$(SETTOKUENV) $(CC) -DENVDIR=\"dir.$<.tdb\" -DUSE_TDB -DIS_TDB=1 $(CFLAGS) $(TDB_CPPFLAGS) $(TDB_LOADLIBES) $< -o $@
%.tdbt: %.c
	$(SETTOKUENV) $(CC) -DENVDIR=\"dir.$<.tdb\" -DUSE_TDB -DIS_TDB=1 $(CFLAGS) $(TDB_CPPFLAGS) $(TDB_TRACELOADLIBES) $< -o $@

.PHONY: %.recover
TLRECOVER = 2 3 4 5 6 7 8 9 10
all.recover: $(patsubst %,test_log%.recover,$(TLRECOVER))
$(patsubst %,test_log%.tdbrun,$(TLRECOVER)):
	@echo -n
%.recover: %.tdb
	$(SETTOKUENV) $(VGRIND) ./$< && \
	$(SETTOKUENV) rm -rf dir.$(patsubst %.tdb,%.c.tdb,$<).recover && \
	$(SETTOKUENV) mkdir dir.$(patsubst %.tdb,%.c.tdb,$<).recover && \
	($(SETTOKUENV) cd dir.$(patsubst %.tdb,%.c.tdb,$<).recover; $(VGRIND) ../../../newbrt/tdb-recover ../dir.$(patsubst %.tdb,%.c.tdb,$<) ) && \
	$(SETTOKUENV) diff dir.$(patsubst %.tdb,%.c.tdb,$<) dir.$(patsubst %.tdb,%.c.tdb,$<).recover/foo.db \
	$(MAYBEINVERTER) $(SUMMARIZE_CMD)

%.recoverwc: %.tdb
	$(SETTOKUENV) (cd dir.$(patsubst %.tdb,%.c.tdb,$<);pwd;cat log*| ../../../newbrt/tdb_logprint |wc -c)


.PHONY: clean cleanall

clean:
	rm -f $(ALL_TESTS) *.o *.gcno *.gcda *.gcov 
	rm -rf dir.*.tdb dir.*.bdb dir.*.tdb.recover

cleanall: clean
	rm -f *.tdbrun

test_db_curs4.tdb: trace.h
test_db_curs4.bdb: trace.h
test_db_assoc3.tdb test_db_assoc3.bdb: test.h


# This one failed in both BDB and TokuDB, in the same way.  It was a program error.  Now it works
test_db_assoc3.tdbrun_wasbad: test_db_assoc3.tdb
	$(MAYBEATSIGN) $(SETTOKUENV) ./test_db_assoc3.tdb --seed=1 --count=200
	$(MAYBEATSIGN) $(SETTOKUENV) ./test_db_assoc3.tdb --seed=1 --count=200 --more
	$(MAYBEATSIGN) $(SETTOKUENV) ./test_db_assoc3.tdb --seed=1 --count=200 --more
	$(MAYBEATSIGN) $(SETTOKUENV) ./test_db_assoc3.tdb --seed=1 --count=200 --more
	$(MAYBEATSIGN) $(SETTOKUENV) ./test_db_assoc3.tdb --seed=1 --count=200 --more
	$(MAYBEATSIGN) $(SETTOKUENV) ./test_db_assoc3.tdb --seed=1 --count=200 --more
	$(MAYBEATSIGN) $(SETTOKUENV) ./test_db_assoc3.tdb --seed=1 --count=200 --more
# serialize these two tests since they use the same directory
test_db_assoc3.tdbrun_wasbad: test_db_assoc3.tdbrun


.phony: build_primary_db build_name_db  build_expire_db
test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb:
	mkdir $@
build_primary_db: test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb
	gunzip < test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb.original/primary.db.gz > test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb/primary.db
build_name_db: test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb
	gunzip < test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb.original/name.db.gz > test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb/name.db
build_expire_db: test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb
	gunzip < test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb.original/expire.db.gz > test_v6_assoc3.dir/dir.test_db_assoc3.c.tdb/expire.db

test_v6v7_assoc3.tdbrun: test_db_assoc3.tdb build_primary_db  build_name_db build_expire_db
	$(SETTOKUENV) (cd test_v6_assoc3.dir; LD_LIBRARY_PATH=../.. $(VGRIND) ../test_db_assoc3.tdb --seed=3 --count=1000 --more)  $(SUMMARIZE_CMD)

test_db_assoc3.tdbrun: test_db_assoc3.tdb
	$(SETTOKUENV) $(VGRIND) ./test_db_assoc3.tdb --seed=2 --count=100000 $(VERBVERBOSE) && \
	$(SETTOKUENV) $(VGRIND) ./test_db_assoc3.tdb --seed=2 --count=100000 --more $(VERBVERBOSE) $(SUMMARIZE_CMD)

# Give up on VGRIND for bdbrun
test_db_assoc3.bdbrun: test_db_assoc3.bdb
	$(UNSETTOKUENV) ./test_db_assoc3.bdb --seed=2 --count=100000        $(VERBVERBOSE) && \
	$(UNSETTOKUENV) ./test_db_assoc3.bdb --seed=2 --count=100000 --more $(VERBVERBOSE) $(SUMMARIZE_CMD)

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
	# echo Did $@

tgbr_%_a.tdbrun: test_get_both_range.tdb
	$(SETTOKUENV) $(VGRIND) ./$< $(VERBVERBOSE) -i $(patsubst tgbr_%_a.tdbrun,%,$@) -a $(MAYBEINVERTER) $(SUMMARIZE_CMD)
tgbr_%_b.tdbrun: test_get_both_range.tdb
	$(SETTOKUENV) $(VGRIND) ./$< $(VERBVERBOSE) -i $(patsubst tgbr_%_b.tdbrun,%,$@) -b $(MAYBEINVERTER) $(SUMMARIZE_CMD)
tgbr_%_c.tdbrun: test_get_both_range.tdb
	$(SETTOKUENV) $(VGRIND) ./$< $(VERBVERBOSE) -i $(patsubst tgbr_%_c.tdbrun,%,$@) -c $(MAYBEINVERTER)  $(SUMMARIZE_CMD)

dumpit:
	../../newbrt/brtdump  dir.test_log5.c.tdb.recover/foo.db > dump.r && ../../newbrt/brtdump  dir.test_log5.c.tdb/foo.db > dump.o && diff dump.o dump.r

# test on small stacks
test_thread_stack.%run: test_thread_stack.%
	$(UNSETTOKUENV) ./test_thread_stack.% -a -thread_stack 16384         && \
	$(UNSETTOKUENV) ./test_thread_stack.% -a -thread_stack 16384 -resume $(SUMMARIZE_CMD)

