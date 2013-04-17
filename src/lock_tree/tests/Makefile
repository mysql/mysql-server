# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../../../
INCLUDEDIRS=-I. -I../ -I$(TOKUROOT)newbrt -I$(TOKUROOT)src/range_tree
DEPEND_COMPILE += \
	../*.h \
	test.h \
#end
DEPEND_COMPILE+= $(NEWBRT)

LINK_FILES=$(NEWBRT)
include $(TOKUROOT)toku_include/Makefile.include

ifeq ($(CC),icc)
SKIP_WARNING += $(ICC_NOWARN)1418 #Non static functions do not need prototypes.
endif

SRCS = $(wildcard *.c)

LOG_TESTS =  $(patsubst %.c,%.log$(BINSUF),$(SRCS))
TLOG_TESTS = $(patsubst %.c,%.tlog$(BINSUF),$(SRCS))
LIN_TESTS =  $(patsubst %.c,%.lin$(BINSUF),$(SRCS))

ALL_TESTS = $(LIN_TESTS) $(TLOG_TESTS) #$(LOG_TESTS)

RUN_LOG_TESTS  = $(patsubst %.log$(BINSUF),%.logrun,$(LOG_TESTS))
RUN_TLOG_TESTS = $(patsubst %.tlog$(BINSUF),%.tlogrun,$(TLOG_TESTS))
RUN_LIN_TESTS  = $(patsubst %.lin$(BINSUF),%.linrun,$(LIN_TESTS))
RUN_ALL_TESTS  = $(RUN_LIN_TESTS) $(RUN_TLOG_TESTS) #$(RUN_LOG_TESTS)

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
	$(CC) -DTESTDIR=\"dir.$<.lin\"  $< $(A_LINEAR) $(BIN_FROM_C_FLAGS)

%.tlog$(BINSUF): %.c $(DEPEND_COMPILE) $(DEPEND_LINK) $(A_TLOG)
	$(CC) -DTESTDIR=\"dir.$<.tlog\" $< $(A_TLOG) $(BIN_FROM_C_FLAGS) -DTOKU_RT_NOOVERLAPS

%.log$(BINSUF): %.c  $(DEPEND_COMPILE) $(DEPEND_LINK) $(A_LOG)
	$(CC) -DTESTDIR=\"dir.$<.log\"  $< $(A_LOG) $(BIN_FROM_C_FLAGS)

clean:
	rm -f $(ALL_TESTS)
	rm -rf dir.*.lin dir.*.tlog dir.*.log

