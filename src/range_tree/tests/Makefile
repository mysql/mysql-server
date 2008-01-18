# On OSX do:
#   make OSX=OSX

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

ifeq ($(OSX),OSX)
#Note: OSX 10.4 needs DYLD_LIBRARY_PATH.  OSX 10.5 claims to support -rpath.
 LIBEXT=dylib
 VGRIND=
 TDB_LOADLIBES = -L../ -lrangetree
 SETTOKUENV=export DYLD_LIBRARY_PATH=.. ;
 UNSETTOKUENV=unset DYLD_LIBRARY_PATH ;
else
 SETTOKUENV=
 UNSETTOKUENV=
 LIBEXT=so
 TDB_LOADLIBES = -L../ -lrangetree -Wl,-rpath,.. -lpthread
 VGRIND=valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

LIBNAME=librt.$(LIBEXT)
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = -Wall -Werror $(OPTFLAGS) -g $(GCOV_FLAGS)
TDB_CPPFLAGS = -I../ -I../../../newbrt


SRCS = $(wildcard *.c)

TDB_TESTS = $(patsubst %.c,%.tdb,$(SRCS))
BDB_TESTS = $(patsubst %.c,%.bdb,$(SRCS))
LINEAR_TESTS = $(patsubst %.c,%.bdb,$(SRCS))

ALL_TESTS = $(TDB_TESTS) #$(BDB_TESTS)

RUN_TDB_TESTS = $(patsubst %.tdb,%.tdbrun,$(TDB_TESTS))
RUN_BDB_TESTS = $(patsubst %.bdb,%.bdbrun,$(BDB_TESTS))
RUN_ALL_TESTS = $(RUN_TDB_TESTS) $(RUN_BDB_TESTS)


all: make_libs $(ALL_TESTS)

foo:
	echo RUN_TDB_TESTS: $(RUN_TDB_TESTS)
	echo ALL_TESTS: $(ALL_TESTS)

.PHONY: check check.bdb check.tdb
check: check.bdb check.tdb all.recover test_db_assoc3.tdbrun_wasbad
	@ echo ok
tests.bdb: $(BDB_TESTS)
check.bdb: $(RUN_BDB_TESTS)
tests.tdb: $(TDB_TESTS)
check.tdb: $(RUN_TDB_TESTS)

# Need these rule so that Make knows about all the file names
.PHONY: %.bdbrun %.tdbrun %.run
$(RUN_ALL_TESTS):
$(ALL_TESTS):

%.run: %.bdbrun %.tdbrun
	@ echo ok

ifeq ($(VERBOSE),2)
VERBVERBOSE=-v
MAYBEATSIGN= 
VERBQUIET=
else
 ifeq ($(VERBOSE),1)
  VERBVERBOSE=
  MAYBEATSIGN=
  VERBQUIET=
 else
  VERBVERBOSE=
  MAYBEATSIGN=@
  VERBQUIET=--quiet
 endif
endif

# The @ sign makes the make quiet.  If there is an error there is enough info to tell what test failed.
%.bdbrun: %.bdb
	$(MAYBEATSIGN) $(UNSETTOKUENV) $(VGRIND) $(BDB_SUPPRESSIONS) ./$< $(VERBVERBOSE)
%.tdbrun: %.tdb
	$(MAYBEATSIGN) $(SETTOKUENV) $(VGRIND) ./$< $(VERBVERBOSE)

# For a few of the tests bdb is making valgrind unhappy.
FOO_NO_VGRIND = \
  db_already_exists \
  db_curs2 \
  db_dbt_appmalloc \
  db_dbt_mem_behavior \
  db_dup \
  db_env_open_nocreate \
  db_env_open_open_close \
  db_open_notexist_reopen \
  db_remove_subdb \
  db_subdb \
  dup_delete \
  dup_flags \
  dup_insert \
  dup_search \
  kv_limits \
  log4 \
  rand_insert \
  reverse_compare_fun \
  # Comment to terminate list so the previous line can end with a slash
 
NO_VGRIND = \
  db_dbt_appmalloc \
  db_dbt_mem_behavior \
  db_assoc3 \
  db_curs2 \
  db_delete \
  db_env_open_nocreate \
  db_env_open_open_close \
  db_open_notexist_reopen \
  db_remove_subdb \
  log4 \
  log5 \
  # Comment to terminate list so the previous line can end with a slash

$(patsubst %,test_%.bdbrun,$(NO_VGRIND)): VGRIND=
$(patsubst %,test_%.bdbrun,$(NO_VGRIND)): BDB_SUPPRESSIONS=

libs:
	cd ..;make

%.bdb: %.c ../rangetree.h test.h
	$(UNSETTOKUENV) cc  -DDIR=\"dir.$<.bdb\" $(BDB_CPPFLAGS) -DUSE_BDB -DIS_TDB=0 $(CFLAGS) $< $(BDB_LDFLAGS) -lrangetree -o $@
%.tdb: %.c ../rangetree.h test.h
	$(SETTOKUENV) cc  -DDIR=\"dir.$<.tdb\" -DUSE_TDB -DIS_TDB=1 $(CFLAGS) $(TDB_CPPFLAGS) $(TDB_LOADLIBES) $< -o $@

.PHONY: %.recover
all.recover: test_log2.recover test_log3.recover test_log4.recover test_log5.recover
%.recover: %.tdb
	$(MAYBEATSIGN) cd ..;make $(VERBQUIET)
	$(MAYBEATSIGN) $(VGRIND) ./$<
	$(MAYBEATSIGN) rm -rf dir.$(patsubst %.tdb,%.c.tdb,$<).recover
	$(MAYBEATSIGN) mkdir dir.$(patsubst %.tdb,%.c.tdb,$<).recover
	$(MAYBEATSIGN) cd dir.$(patsubst %.tdb,%.c.tdb,$<).recover;$(VGRIND) ../../../newbrt/recover ../dir.$(patsubst %.tdb,%.c.tdb,$<)
	$(MAYBEATSIGN) diff dir.$(patsubst %.tdb,%.c.tdb,$<) dir.$(patsubst %.tdb,%.c.tdb,$<).recover/foo.db

make_libs:
	cd ..;make

clean:
	rm -f $(ALL_TESTS) *.o *.gcno *.gcda *.gcov
	rm -rf dir.*.tdb dir.*.bdb

test_db_curs4.tdb: trace.h
test_db_curs4.bdb: trace.h
test_db_assoc3.tdb test_db_assoc3.bdb: test.h


# This one failed in both BDB and TokuDB, in the same way.  It was a program error.  Now it works
test_db_assoc3.tdbrun_wasbad: test_db_assoc3.tdb
	$(MAYBEATSIGN) ./test_db_assoc3.tdb --seed=1 --count=200
	$(MAYBEATSIGN) ./test_db_assoc3.tdb --seed=1 --count=200 --more
	$(MAYBEATSIGN) ./test_db_assoc3.tdb --seed=1 --count=200 --more
	$(MAYBEATSIGN) ./test_db_assoc3.tdb --seed=1 --count=200 --more
	$(MAYBEATSIGN) ./test_db_assoc3.tdb --seed=1 --count=200 --more
	$(MAYBEATSIGN) ./test_db_assoc3.tdb --seed=1 --count=200 --more
	$(MAYBEATSIGN) ./test_db_assoc3.tdb --seed=1 --count=200 --more

test_db_assoc3.tdbrun: test_db_assoc3.tdb
	$(MAYBEATSIGN) $(VGRIND) ./test_db_assoc3.tdb --seed=2 --count=100000 $(VERBVERBOSE)
	$(MAYBEATSIGN) $(VGRIND) ./test_db_assoc3.tdb --seed=2 --count=100000 --more $(VERBVERBOSE)

test_db_assoc3.bdbrun: test_db_assoc3.bdb
	$(MAYBEATSIGN) $(VGRIND) ./test_db_assoc3.bdb --seed=2 --count=100000 $(VERBVERBOSE)
	$(MAYBEATSIGN) $(VGRIND) ./test_db_assoc3.bdb --seed=2 --count=100000 --more $(VERBVERBOSE)
