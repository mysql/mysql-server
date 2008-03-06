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

LOG_TESTS  = $(patsubst %.c,%.log,$(SRCS))
TLOG_TESTS = $(patsubst %.c,%.tlog,$(SRCS))
LIN_TESTS  = $(patsubst %.c,%.lin,$(SRCS))
TLIN_TESTS = $(patsubst %.c,%.tlin,$(SRCS))

ALL_TESTS = $(LIN_TESTS) $(TLIN_TESTS) $(TLOG_TESTS) $(LOG_TESTS)

RUN_LOG_TESTS  = $(patsubst %.log,%.logrun,$(LOG_TESTS))
RUN_TLOG_TESTS = $(patsubst %.tlog,%.tlogrun,$(TLOG_TESTS))
RUN_LIN_TESTS  = $(patsubst %.lin,%.linrun,$(LIN_TESTS))
RUN_TLIN_TESTS = $(patsubst %.tlin,%.tlinrun,$(TLIN_TESTS))
RUN_ALL_TESTS  = $(RUN_LIN_TESTS) $(RUN_TLIN_TESTS) $(RUN_TLOG_TESTS) $(RUN_LOG_TESTS)

.PHONY: check default all check.lin check.tlin check.log check.tlog tests.lin tests.tlin tests.tlog tests.log

default: check

all: make_libs $(ALL_TESTS)

check: check.lin check.tlin #check.tlog check.log
	@ echo $@ ok
tests.lin: make_libs $(LIN_TESTS)
	@ echo $@ ok
check.lin: make_libs $(RUN_LIN_TESTS)
	@ echo $@ ok
tests.tlin: make_libs $(TLIN_TESTS)
	@ echo $@ ok
check.tlin: make_libs $(RUN_TLIN_TESTS)
	@ echo $@ ok
tests.tlog: make_libs $(TLOG_TESTS)
	@ echo $@ ok
check.tlog: make_libs $(RUN_TLOG_TESTS)
	@ echo $@ ok
tests.log: make_libs $(LOG_TESTS)
	@ echo $@ ok
check.log: make_libs $(RUN_LOG_TESTS)
	@ echo $@ ok

# Need these rule so that Make knows about all the file names
.PHONY: %.linrun %.tlinrun %.tlogrun %.logrun %.run

%.run: %.linrun %.tlinrun %.tlogrun %.logrun
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
	$(MAYBEATSIGN) $(VGRIND) ./$< $(VERBVERBOSE)
%.tlinrun: %.tlin
	$(MAYBEATSIGN) $(VGRIND) ./$< $(VERBVERBOSE)
%.tlinrun: %.tlin
	$(MAYBEATSIGN) $(VGRIND) ./$< $(VERBVERBOSE)
%.logrun: %.log
	$(MAYBEATSIGN) $(VGRIND) ./$< $(VERBVERBOSE)
%.tlogrun: %.tlog
	$(MAYBEATSIGN) $(VGRIND) ./$< $(VERBVERBOSE)

libs:
	cd .. && make

RT_LINEAR_BINS  = ../../range_tree/linear.o
RT_TLINEAR_BINS = ../../range_tree/linear.o
RT_TLOG_BINS    = ../../range_tree/tlog.o
RT_LOG_BINS     = ../../range_tree/log.o

LT_BINS        = ../lth.o ../rth.o
LT_OVERLAP     = ../locktree_global_readset.o
LT_NOOVERLAP   = ../locktree_no_global_readset.o
LT_LINEAR      = $(LT_OVERLAP)   $(LT_BINS) $(RT_LINEAR_BINS)
LT_TLINEAR     = $(LT_NOOVERLAP) $(LT_BINS) $(RT_TLINEAR_BINS)
LT_TLOG        = $(LT_NOOVERLAP) $(LT_BINS) $(RT_TLOG_BINS)
LT_LOG         = $(LT_OVERLAP)   $(LT_BINS) $(RT_LOG_BINS)

%.lin: %.c ../locktree.h test.h $(LT_LINEAR)
	cc  -DDIR=\"dir.$<.lin\"  $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LT_LINEAR)
%.tlin: %.c ../locktree.h test.h $(LT_TLINEAR)
	cc  -DDIR=\"dir.$<.tlin\" $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LT_TLINEAR) -DTOKU_RT_NOOVERLAPS
%.tlog: %.c ../locktree.h test.h $(LT_TLOG)
	cc  -DDIR=\"dir.$<.tlog\" $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LT_TLOG)    -DTOKU_RT_NOOVERLAPS
%.log: %.c ../locktree.h test.h  $(LT_LOG)
	cc  -DDIR=\"dir.$<.log\"  $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LT_LOG)

.PHONY: make_libs

make_libs:
	cd .. && make

clean:
	rm -f $(ALL_TESTS) *.o *.gcno *.gcda *.gcov
	rm -rf dir.*
