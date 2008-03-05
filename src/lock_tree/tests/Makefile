# On OSX do:
#   make OSX=OSX

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

OPTFLAGS = -O0
ifeq ($(OSX),)
 VGRIND=valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

ifeq ($(GCOV),GCOV)
 GCOV_FLAGS = -fprofile-arcs -ftest-coverage
else
 GCOV_FLAGS =
endif
CFLAGS = -W -Wall -Wextra -Werror $(OPTFLAGS) -g3 -ggdb3 $(GCOV_FLAGS)
CFLAGS += -Wbad-function-cast -Wcast-align -Waggregate-return
CFLAGS += -Wmissing-noreturn -Wmissing-format-attribute 
CPPFLAGS += -L../ -L../../range_tree 
CPPFLAGS += -I. -I../ -I../../range_tree -I../../../newbrt -I../../../include


SRCS = $(wildcard *.c)

LOG_TESTS = $(patsubst %.c,%.log,$(SRCS))
TLOG_TESTS = $(patsubst %.c,%.tlog,$(SRCS))
LIN_TESTS = $(patsubst %.c,%.lin,$(SRCS))

ALL_TESTS = $(LIN_TESTS) $(TLOG_TESTS) $(LOG_TESTS)

RUN_LOG_TESTS  = $(patsubst %.log,%.logrun,$(LOG_TESTS))
RUN_TLOG_TESTS = $(patsubst %.tlog,%.tlogrun,$(TLOG_TESTS))
RUN_LIN_TESTS  = $(patsubst %.lin,%.linrun,$(LIN_TESTS))
RUN_ALL_TESTS  = $(RUN_LIN_TESTS) $(RUN_TLOG_TESTS) $(RUN_LOG_TESTS)

.PHONY: check default all check.lin check.log tests.lin tests.log check.tlog tests.tlog

default: check

all: make_libs $(ALL_TESTS)

#check: check.lin check.tlog check.log
check: check.lin #check.tlog check.log
	@ echo ok
tests.lin: make_libs $(LIN_TESTS)
check.lin: make_libs $(RUN_LIN_TESTS)
tests.tlog: make_libs $(TLOG_TESTS)
check.tlog: make_libs $(RUN_TLOG_TESTS)
tests.log: make_libs $(LOG_TESTS)
check.log: make_libs $(RUN_LOG_TESTS)

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
	cd .. && make

RT_LINEAR_BINS = ../../range_tree/linear.o
RT_TLOG_BINS   = ../../range_tree/tlog.o
RT_LOG_BINS    = ../../range_tree/log.o
LT_BINS        = ../locktree.o ../lth.o ../rth.o


%.lin: %.c ../locktree.h test.h
	cc  -DDIR=\"dir.$<.lin\" -DTOKU_LT_LINEAR $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LT_BINS) $(RT_LINEAR_BINS)
%.tlog: %.c ../locktree.h test.h
	cc  -DDIR=\"dir.$<.log\" -DTOKU_LT_TLOG   $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LT_BINS) $(RT_TLOG_BINS)
%.log: %.c ../locktree.h test.h
	cc  -DDIR=\"dir.$<.log\" -DTOKU_LT_LOG    $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LT_BINS) $(RT_LOG_BINS)

.PHONY: make_libs

make_libs:
	cd .. && make

clean:
	rm -f $(ALL_TESTS) *.o *.gcno *.gcda *.gcov
	rm -rf dir.*.log dir.*.lin
