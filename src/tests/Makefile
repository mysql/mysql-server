# On OSX do:
#   make OSX=OSX

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

ifndef BDBDIR
BDBDIR=/usr
endif
BDB_CPPFLAGS = -I$(BDBDIR)/include
BDB_LDFLAGS = -L$(BDBDIR)/lib

ifeq ($(OSX),OSX)
#Note: OSX 10.4 needs DYLD_LIBRARY_PATH.  OSX 10.5 claims to support -rpath.
LIBEXT=dylib
VGRIND=
TDB_LOADLIBES = -L../ -ldb
SETTOKUENV=export DYLD_LIBRARY_PATH=..
UNSETTOKUENV=unset DYLD_LIBRARY_PATH
else
SETTOKUENV=true
UNSETTOKUENV=true
LIBEXT=so
BDB_LDFLAGS += -L$(BDBDIR)/lib -ldb -Wl,-rpath,$(BDBDIR)/lib
TDB_LOADLIBES = -L../ -ldb -Wl,-rpath,..
VGRIND=valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

# VERBOSE=true

LIBNAME=libdb.$(LIBEXT)
CFLAGS = -Wall -Werror -O0 -g

TDB_CPPFLAGS = -I../../include

SRCS = $(wildcard *.c)

TDB_TESTS = $(patsubst %.c,%.tdb,$(SRCS))
BDB_TESTS = $(patsubst %.c,%.bdb,$(SRCS))

ALL_TESTS = $(TDB_TESTS) $(BDB_TESTS)

RUN_TDB_TESTS = $(patsubst %.tdb,%.tdbrun,$(TDB_TESTS))
RUN_BDB_TESTS = $(patsubst %.bdb,%.bdbrun,$(BDB_TESTS))
RUN_ALL_TESTS = $(RUN_TDB_TESTS) $(RUN_BDB_TESTS)


all: make_libs $(ALL_TESTS)

foo:
	echo RUN_TDB_TESTS: $(RUN_TDB_TESTS)
	echo ALL_TESTS: $(ALL_TESTS)

.PHONY: check check.bdb check.tdb
check: check.bdb check.tdb all.recover
	@ echo ok
check.bdb: $(RUN_BDB_TESTS)
check.tdb: $(RUN_TDB_TESTS)

# Need these rule so that Make knows about all the file names
.PHONY: %.bdbrun %.tdbrun %.run
$(RUN_ALL_TESTS):
$(ALL_TESTS):

%.run: %.bdbrun %.tdbrun
	@ echo ok

ifeq ($(VERBOSE),2)
VERBVERBOSE=-v
endif

# The @ sign makes the make quiet.  If there is an error there is enough info to tell what test failed.
%.bdbrun: %.bdb
ifdef VERBOSE
	$(UNSETTOKUENV); $(VGRIND) ./$< $(VERBVERBOSE)
else
	@ $(UNSETTOKUENV); $(VGRIND) ./$<
endif
%.tdbrun: %.tdb
ifdef VERBOSE
	$(SETTOKUENV); $(VGRIND) ./$< $(VERBVERBOSE)
else
	@ $(SETTOKUENV); $(VGRIND) ./$<
endif
# For a few of the tests bdb is making valgrind unhappy.
NO_VGRIND = \
  db_already_exists \
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
  rand_insert \
  reverse_compare_fun \
  # Comment to terminate list so the previous line can end with a slash

$(patsubst %,test_%.bdbrun,$(NO_VGRIND)): VGRIND=

%.bdb: %.c
	$(UNSETTOKUENV); cc  -DDIR=\"dir.$<.bdb\" $(BDB_CPPFLAGS) -DUSE_BDB $(CFLAGS) $< $(BDB_LDFLAGS) -ldb -o $@
%.tdb: %.c
	$(SETTOKUENV); cc  -DDIR=\"dir.$<.tdb\" -DUSE_TDB $(CFLAGS) $(TDB_CPPFLAGS) $(TDB_LOADLIBES) $< -o $@

.PHONY: %.recover
all.recover: test_log2.recover test_log3.recover
%.recover: %.tdb
	cd ../../newbrt;make recover
	./$<
	rm -rf dir.$(patsubst %.tdb,%.c.tdb,$<).recover
	mkdir dir.$(patsubst %.tdb,%.c.tdb,$<).recover
	cd dir.$(patsubst %.tdb,%.c.tdb,$<).recover;../../../newbrt/recover ../dir.$(patsubst %.tdb,%.c.tdb,$<)
	diff dir.$(patsubst %.tdb,%.c.tdb,$<) dir.$(patsubst %.tdb,%.c.tdb,$<).recover/foo.db

make_libs:
	cd ..;make

clean:
	rm -f $(ALL_TESTS) *.o
	rm -rf dir.*.tdb dir.*.bdb
