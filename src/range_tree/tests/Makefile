# On OSX do:
#   make OSX=OSX

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

ifeq ($(OSX),)
 VGRIND=valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

OPTFLAGS=-O0

# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CFLAGS = -W -Wall -Wextra -Werror $(OPTFLAGS) -g3 -ggdb3 $(GCOV_FLAGS)
CFLAGS += -Wbad-function-cast -Wcast-align -Wconversion -Waggregate-return
CFLAGS += -Wmissing-noreturn -Wmissing-format-attribute
CPPFLAGS += -I../ -I../../../newbrt -I../../../include


SRCS = $(wildcard *.c)

LOG_TESTS = $(patsubst %.c,%.log,$(SRCS))
TLOG_TESTS = $(patsubst %.c,%.tlog,$(SRCS))
LIN_TESTS = $(patsubst %.c,%.lin,$(SRCS))

ALL_TESTS = $(LIN_TESTS) $(LOG_TESTS) $(TLOG_TESTS)

RUN_LOG_TESTS  = $(patsubst %.log,%.logrun,$(LOG_TESTS))
RUN_TLOG_TESTS = $(patsubst %.tlog,%.tlogrun,$(TLOG_TESTS))
RUN_LIN_TESTS  = $(patsubst %.lin,%.linrun,$(LIN_TESTS))
RUN_ALL_TESTS  = $(RUN_LIN_TESTS) $(RUN_TLOG_TESTS) $(RUN_LOG_TESTS)

.PHONY: default all check tests check.lin check.tlog check.log tests.lin tests.log tests.tlog

default: build

all build: $(ALL_TESTS)

check: check.lin check.tlog #check.log
	@ echo $@ ok
tests: tests.lin tests.tlog #tests.log
	@ echo $@ ok
tests.lin:  $(LIN_TESTS)
	@ echo $@ ok
check.lin: $(RUN_LIN_TESTS)
	@ echo $@ ok
tests.tlog: $(TLOG_TESTS)
	@ echo $@ ok
check.tlog: $(RUN_TLOG_TESTS)
	@ echo $@ ok
tests.log:  $(LOG_TESTS)
	@ echo $@ ok
check.log:  $(RUN_LOG_TESTS)
	@ echo $@ ok

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
	$(MAYBEATSIGN) $(VGRIND) ./$< $(VERBVERBOSE)
%.logrun: %.log
	$(MAYBEATSIGN) $(VGRIND) ./$< $(VERBVERBOSE)
%.tlogrun: %.tlog
	$(MAYBEATSIGN) $(VGRIND) ./$< $(VERBVERBOSE)

LINEAR_BINS = ../linear.o        ../../../newbrt/newbrt.o
TLOG_BINS   = ../log_nooverlap.o ../../../newbrt/newbrt.o
LOG_BINS    = ../log.o           ../../../newbrt/newbrt.o
HEADERS=../rangetree.h ../rangetree-internal.h test.h

../../../newbrt/newbrt.o:
	cd ../../../newbrt && make

%.lin: %.c  $(HEADERS) $(LINEAR_BINS)
	$(CC) -DDIR=\"dir.$<.lin\" $(CFLAGS) $(CPPFLAGS) $< -o $@ $(LINEAR_BINS)
%.tlog: %.c $(HEADERS) $(TLOG_BINS)
	$(CC) -DDIR=\"dir.$<.log\" $(CFLAGS) $(CPPFLAGS) $< -o $@ $(TLOG_BINS) -DTOKU_RT_NOOVERLAPS
%.log: %.c  $(HEADERS) $(LOG_BINS)
	$(CC) -DDIR=\"dir.$<.log\" $(CFLAGS) $(CPPFLAGS) $< -o $@ $(LOG_BINS)

clean:
	rm -f $(ALL_TESTS) *.o *.gcno *.gcda *.gcov
	rm -rf dir.*.log dir.*.LIN
