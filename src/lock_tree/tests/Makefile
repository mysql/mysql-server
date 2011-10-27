# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../../../
INCLUDEDIRS=-I. -I../ -I$(TOKUROOT)newbrt -I$(TOKUROOT)src/range_tree -I$(TOKUROOT)include
DEPEND_COMPILE += \
	../*.h \
	test.h \
#end
DEPEND_COMPILE+= $(NEWBRT)

include $(TOKUROOT)toku_include/Makefile.include
LDLIBS+=-lnewbrt -ltokuportability
LDFLAGS+=-L$(TOKUROOT)lib -Wl,-rpath,$(shell pwd)/$(TOKUROOT)lib

ifeq ($(CC),icc)
SKIP_WARNING += $(ICC_NOWARN)1418 #Non static functions do not need prototypes.
endif

SRCS = $(wildcard *.c)

LOG_TESTS =  $(patsubst %.c,%.log$(BINSUF),$(SRCS))
TLOG_TESTS = $(patsubst %.c,%.tlog$(BINSUF),$(SRCS))
LIN_TESTS =  $(patsubst %.c,%.lin$(BINSUF),$(SRCS))
WFG_TESTS =  $(patsubst %.c,%.lin$(BINSUF), $(wildcard test_wfg*.c))

ALL_TESTS = $(LIN_TESTS) $(TLOG_TESTS) #$(LOG_TESTS)

RUN_LOG_TESTS  = $(patsubst %.log$(BINSUF),%.logrun,$(LOG_TESTS))
RUN_TLOG_TESTS = $(patsubst %.tlog$(BINSUF),%.tlogrun,$(TLOG_TESTS))
RUN_LIN_TESTS  = $(patsubst %.lin$(BINSUF),%.linrun,$(LIN_TESTS))
RUN_ALL_TESTS  = $(RUN_LIN_TESTS) $(RUN_TLOG_TESTS) #$(RUN_LOG_TESTS)
RUN_WFG_TESTS  = $(patsubst %.lin$(BINSUF),%.linrun,$(WFG_TESTS))

.PHONY: default all check tests check.lin check.tlog check.log tests.lin tests.log tests.tlog

default all build: $(ALL_TESTS)

check: $(RUN_ALL_TESTS)
tests: $(ALL_TESTS)
tests.lin: $(LIN_TESTS)
check.lin: $(RUN_LIN_TESTS)
tests.tlog: $(TLOG_TESTS)
check.tlog: $(RUN_TLOG_TESTS)
tests.log:  $(LOG_TESTS)
check.log:  $(RUN_LOG_TESTS)
check.wfg:  $(RUN_WFG_TESTS)

.PHONY: %.linrun %.logrun %.run %.tlogrun
# STUFF!!!!

%.run: %.linrun %.tlogrun #%.logrun

%.linrun: %.lin$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) $(SUMMARIZE_CMD)
%.tlogrun: %.tlog$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) $(SUMMARIZE_CMD)
%.logrun: %.log$(BINSUF)
	$(VGRIND) ./$< $(VERBVERBOSE) $(SUMMARIZE_CMD)

A_LINEAR=$(LOCKTREE_LINEAR) $(RANGETREE_LINEAR)
A_TLOG  =$(LOCKTREE_TLOG)   $(RANGETREE_TLOG)
A_LOG   =$(LOCKTREE_LOG)    $(RANGETREE_LOG)

%.lin$(BINSUF): %.c  $(DEPEND_COMPILE) $(DEPEND_LINK) $(A_LINEAR)
	$(CC) -DTESTDIR=\"dir.$<.lin\"  $< $(A_LINEAR) $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)

%.tlog$(BINSUF): %.c $(DEPEND_COMPILE) $(DEPEND_LINK) $(A_TLOG)
	$(CC) -DTESTDIR=\"dir.$<.tlog\" $< $(A_TLOG) $(BIN_FROM_C_FLAGS) -DTOKU_RT_NOOVERLAPS $(LINK_MUST_BE_LAST)

%.log$(BINSUF): %.c  $(DEPEND_COMPILE) $(DEPEND_LINK) $(A_LOG)
	$(CC) -DTESTDIR=\"dir.$<.log\"  $< $(A_LOG) $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST)

clean:
	rm -f $(ALL_TESTS)
	rm -rf dir.*.lin dir.*.tlog dir.*.log

check_footprint.linrun: check_footprint_point.linrun check_footprint_range.linrun
	true

check_footprint_point.linrun: test_footprint_point_write.lin
	$(VGRIND) ./$< -v --nrows 1 && \
	$(VGRIND) ./$< -v --nrows 2 && \
	$(VGRIND) ./$< -v --nrows 10 --max_locks 1000 && \
	$(VGRIND) ./$< -v --nrows 100 --max_locks 1000 && \
	$(VGRIND) ./$< -v --nrows 1000 --max_locks 1000 

check_footprint_range.linrun: test_footprint_range_write.lin
	$(VGRIND) ./$< -v --nrows 1 && \
	$(VGRIND) ./$< -v --nrows 2 && \
	$(VGRIND) ./$< -v --nrows 10 --max_locks 1000 && \
	$(VGRIND) ./$< -v --nrows 100 --max_locks 1000 && \
	$(VGRIND) ./$< -v --nrows 1000 --max_locks 1000 

check_footprint.tlogrun: check_footprint_point.tlogrun check_footprint_range.tlogrun
	true

check_footprint_point.tlogrun: test_footprint_point_write.tlog
	$(VGRIND) ./$< -v --nrows 1 && \
	$(VGRIND) ./$< -v --nrows 2 && \
	$(VGRIND) ./$< -v --nrows 10 --max_locks 1000 && \
	$(VGRIND) ./$< -v --nrows 100 --max_locks 1000 && \
	$(VGRIND) ./$< -v --nrows 1000 --max_locks 1000 

check_footprint_range.tlogrun: test_footprint_range_write.tlog
	$(VGRIND) ./$< -v --nrows 1 && \
	$(VGRIND) ./$< -v --nrows 2 && \
	$(VGRIND) ./$< -v --nrows 10 --max_locks 1000 && \
	$(VGRIND) ./$< -v --nrows 100 --max_locks 1000 && \
	$(VGRIND) ./$< -v --nrows 1000 --max_locks 1000 

