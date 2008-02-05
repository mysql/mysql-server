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
 CPPFLAGS = 
 SETENV=export DYLD_LIBRARY_PATH=.. ;
else
 SETTOKUENV=
 UNSETTOKUENV=
 LIBEXT=so
 CPPFLAGS = -Wl,-rpath,.. 
 VGRIND=valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

LIBNAME=librt.$(LIBEXT)
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = -W -Wall -Wextra -Werror $(OPTFLAGS) -g3 -ggdb3 $(GCOV_FLAGS)
CFLAGS += -Wbad-function-cast -Wcast-align -Wconversion -Waggregate-return
CFLAGS += -Wmissing-noreturn -Wmissing-format-attribute -Wunreachable-code
CPPFLAGS += -L../ -I../ -I../../../newbrt -I../../../include -lpthread


SRCS = $(wildcard *.c)

LOG_TESTS = $(patsubst %.c,%.log,$(SRCS))
TLOG_TESTS = $(patsubst %.c,%.tlog,$(SRCS))
LIN_TESTS = $(patsubst %.c,%.lin,$(SRCS))

ALL_TESTS = $(LIN_TESTS) $(LOG_TESTS) $(TLOG_TESTS)

RUN_LOG_TESTS  = $(patsubst %.log,%.logrun,$(LOG_TESTS))
RUN_TLOG_TESTS = $(patsubst %.log,%.logrun,$(TLOG_TESTS))
RUN_LIN_TESTS  = $(patsubst %.lin,%.linrun,$(LIN_TESTS))
RUN_ALL_TESTS  = $(RUN_LIN_TESTS) $(RUN_TLOG_TESTS) $(RUN_LOG_TESTS)


all: make_libs $(ALL_TESTS)

.PHONY: check check.lin check.log tests.lin tests.log tests.tlog
check: check.lin check.tlog check.log
	@ echo ok
tests.lin:  make_libs $(LIN_TESTS)
check.lin:  make_libs $(RUN_LIN_TESTS)
tests.tlog: make_libs $(TLOG_TESTS)
check.tlog: make_libs $(RUN_TLOG_TESTS)
tests.log:  make_libs $(LOG_TESTS)
check.log:  make_libs $(RUN_LOG_TESTS)

# Need these rule so that Make knows about all the file names
.PHONY: %.linrun %.logrun %.run %.tlogrun
$(RUN_ALL_TESTS):
$(ALL_TESTS):

%.run: %.linrun %.tlogrun %.logrun
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
%.linrun: %.lin
	$(MAYBEATSIGN) $(SETENV) $(VGRIND) ./$< $(VERBVERBOSE)
%.logrun: %.log
	$(MAYBEATSIGN) $(SETENV) $(VGRIND) ./$< $(VERBVERBOSE)
%.tlogrun: %.tlog
	$(MAYBEATSIGN) $(SETENV) $(VGRIND) ./$< $(VERBVERBOSE)

libs:
	cd ..;make

%.lin: %.c ../rangetree.h test.h
	$(SETENV) cc  -DDIR=\"dir.$<.lin\" -DTOKU_RT_LINEAR $(CPPFLAGS) $(CFLAGS) $< -lrangetree_linear -o $@
%.log: %.c ../rangetree.h test.h
	$(SETENV) cc  -DDIR=\"dir.$<.log\" -DTOKU_RT_LOG   $(CPPFLAGS) $(CFLAGS) $<  -lrangetree_log    -o $@
%.tlog: %.c ../rangetree.h test.h
	$(SETENV) cc  -DDIR=\"dir.$<.log\" -DTOKU_RT_TLOG   $(CPPFLAGS) $(CFLAGS) $< -lrangetree_tlog   -o $@

.PHONY: make_libs

make_libs:
	cd ..;make

clean:
	rm -f $(ALL_TESTS) *.o *.gcno *.gcda *.gcov
	rm -rf dir.*.log dir.*.LIN
