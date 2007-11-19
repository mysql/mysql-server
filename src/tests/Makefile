# On OSX do:
#   make OSX=OSX

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
TDB_LOADLIBES = -L../ -ldb -Wl,-rpath,..
VGRIND=valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

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
check: check.bdb check.tdb
check.bdb: $(RUN_BDB_TESTS)
check.tdb: $(RUN_TDB_TESTS)

# Need these rule so that Make knows about all the file names
.PHONY: %.bdbrun %.tdbrun
$(RUN_ALL_TESTS):
$(ALL_TESTS):

%.foo:
	echo $@

%.bdbrun: %.bdb
	$(UNSETTOKUENV); $(VGRIND) ./$<
%.tdbrun: %.tdb
	$(SETTOKUENV); $(VGRIND) ./$<

# For a few of the tests bdb is making valgrind unhappy.
NO_VGRIND = \
  db_already_exists \
  db_dup \
  db_env_open_open_close \
  db_remove_subdb \
  db_subdb \
  reverse_compare_fun \
  # Comment to terminate list so the previous line can end with a slash

$(patsubst %,test_%.bdbrun,$(NO_VGRIND)): VGRIND=

%.bdb: %.c
	$(UNSETTOKUENV); cc  -DDIR=\"dir.$<.bdb\" -DUSE_BDB $(CFLAGS) $< -ldb -o $@
%.tdb: %.c
	$(SETTOKUENV); cc  -DDIR=\"dir.$<.tdb\" -DUSE_TDB $(CFLAGS) $(TDB_CPPFLAGS) $(TDB_LOADLIBES) $< -o $@

make_libs:
	cd ..;make

clean:
	rm -f $(ALL_TESTS) *.o
	rm -rf dir.*.tdb dir.*.bdb
