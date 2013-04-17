# On cygwin do:
#   make CYGWIN=cygwin check

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
# PROF_FLAGS = -pg
OPTFLAGS = -O3

ifeq ($(VERBOSE),2)
VERBVERBOSE=-v
else
 ifeq ($(VERBOSE),1)
  VERBVERBOSE=-q
 else
  VERBVERBOSE=-q
 endif
endif

#CFLAG default options
WERROR    = -Werror
WALL      = -Wall -Wextra -Wcast-align -Wbad-function-cast -Wmissing-noreturn
FORMAT    = -Wmissing-format-attribute
VISIBILITY= -fvisibility=hidden
FPICFLAGS = -fPIC
SHADOW    = -Wshadow
SYMBOLS   = -g3 -ggdb3
PORTABILITY=
SKIP_WARNING=
COMBINE_C = -combine -c
LINK_FILES= -lz -lpthread
C99       = -std=c99

#Tools
VGRIND    = valgrind --quiet --error-exitcode=1 --leak-check=yes --suppressions=../valgrind.suppressions

ifeq ($(CC),icc)
    #icc only:
    OPTFLAGS = -O3 -ip -ipo2
    COMBINE_C = -ipo-c
    FORMAT= #No argument for extra format warnings.
    WALL  = -Wall -Wcheck # '-Wextra' becomes '-Wcheck' in icc
    SYMBOLS= -g -debug all -inline-debug-info
    PORTABILITY=-diag-enable port-win
    ifneq ($(CYGWIN),)
	#Cygwin
        ICC_NOWARN=-Qdiag-disable:
    else
	#Linux
        ICC_NOWARN=-diag-disable #Need the space
    endif
    SKIP_WARNING += $(ICC_NOWARN)177     # Don't complain about static variables that are not used.
    #SKIP_WARNING += $(ICC_NOWARN)188     # Don't complain about enumerated type mixed with another type.
    SKIP_WARNING += $(ICC_NOWARN)589     # Don't complain about goto into a block that skips initializing variables.  GCC catches the actual uninitialized variables.
    SKIP_WARNING += $(ICC_NOWARN)869    # Don't complain about unused variables (since we defined __attribute__ to be nothing.)
    SKIP_WARNING += $(ICC_NOWARN)981    # Don't complain about "operands are evaluated in unspecified order".  This seems to be generated whenever more than one argument to a function or operand is computed by function call.
    SKIP_WARNING += $(ICC_NOWARN)1324    # Don't complain about rdtsc clobbering its registers more than once.
endif

ifneq ($(CYGWIN),)
    #Cygwin (Windows) Must override some settings
    CYG_ADD_LIBZ=/usr/lib/libz.a
    VGRIND   =#No Valgrind in cygwin
    FPICFLAGS=#FPIC is default and not allowed as an option.
    VISIBILITY=#Not supported
    SHADOW=#Not supported
    ifeq ($(CC),icc)
    	#Cygwin icc only
        C99       = -Qstd=c99
        OPTFLAGS  = -Ox -Qip -Qipo2
        COMBINE_C = -Qipo-c
        WERROR = -WX # Windows icc version of -Werror
        SYMBOLS= -Zi -debug:all -Qinline-debug-info
        PORTABILITY=
        LINK_FILES=#Not supported
        SKIP_WARNING += $(ICC_NOWARN)1786    # Don't complain about 'read/write/other standards' being deprecated
    else
        #Cygwin gcc only
        FORMAT = -Wno-format
    endif
endif

CFLAGS = $(WALL) $(WERROR) $(FORMAT) $(VISIBILITY) $(FPICFLAGS) $(SHADOW)
CFLAGS += $(OPTFLAGS) $(GCOV_FLAGS) $(PROF_FLAGS)
CFLAGS += $(SYMBOLS) $(SKIP_WARNING) $(C99)

LDFLAGS = $(OPTFLAGS) $(SYMBOLS) $(GCOV_FLAGS) $(PROF_FLAGS) $(LINK_FILES)
# Need XOPEN_SOURCE=600 to get strtoll()
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_XOPEN_SOURCE=600



CPPFLAGS += -D_GNU_SOURCE -I..

# Add -Wconversion

HERE=newbrt/tests

ifeq ($(SUMMARIZE),1)
SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sPASS\n" $(HERE)/$@; else printf "%-60sFAIL\n" $(HERE)/$@ ; test 0 = 1; fi
else
SUMMARIZE_CMD =
endif

# Put these one-per-line so that if we insert a new one the svn diff can understand it better.
# Also keep them sorted.
REGRESSION_TESTS = \
	block_allocator_test \
	bread-test \
	brt-serialize-test \
	brt-test \
	brt-test-cursor \
	brt-test-cursor-2 \
	brt-test-named-db \
	brt-test0 \
	brt-test1 \
	brt-test2 \
	brt-test3 \
	brt-test4 \
	brt-test5 \
	cachetable-rwlock-test \
	cachetable-writequeue-test \
	threadpool-test \
	cachetable-test \
	cachetable-test2 \
	cachetable-put-test \
	cachetable-getandpin-test \
	cachetable-unpin-test \
	cachetable-rename-test \
	cachetable-fd-test \
	cachetable-flush-test \
	cachetable-count-pinned-test \
	cachetable-debug-test \
	cachetable-debug-test \
	fifo-test \
	list-test \
	keyrange \
	keyrange-unflat \
	keyrange-dupsort \
	keyrange-dupsort-unflat \
	log-test \
	log-test2 \
	log-test3 \
	log-test4 \
	log-test5 \
	log-test6 \
	memtest \
	omt-cursor-test \
	omt-test \
	shortcut \
	test-assert \
	test-brt-delete-both \
	test-brt-overflow \
	test-del-inorder \
	test-inc-split \
	test-leafentry \
	test_oexcl \
	test_toku_malloc_plain_free \
        x1764-test \
	ybt-test \
# This line intentially kept commented so I can have a \ on the end of the previous line

# Add in the binaries that must be run in various ways.
BINS =  $(REGRESSION_TESTS) \
        benchmark-test \
	cachetable-scan \
# This line intentially kept commented so I can have a \ on the end of the previous line

CHECKS = \
 benchmarktest_256 \
 test-assertA test-assertB \
 $(REGRESSION_TESTS) \
# This line intentially kept commented so I can have a \ on the previous line

ifeq ($(CIL),1)
	CC=../../../cil/cil-1.3.6/bin/cilly --merge --keepmerged
endif

build: $(BINS)
check: $(patsubst %,check_%,$(CHECKS))

check_fail:
	test 0 = 1 $(SUMMARIZE_CMD)
check_ok:
	test 0 = 0 $(SUMMARIZE_CMD)

check_benchmarktest_256: benchmark-test
	$(VGRIND) ./benchmark-test $(VERBVERBOSE) --valsize 256 --verify 1 $(SUMMARIZE_CMD)

check_test-assertA: test-assert
	@# no arguments, should err
	$(VGRIND) ./test-assert > /dev/null 2>&1 ; test $$? = 1 $(SUMMARIZE_CMD)
check_test-assertB: test-assert
	@# one argument, not "ok" should err
	@rm -f test-assert.out
	($(VGRIND) ./test-assert notok) > test-assert.out 2>&1 ; test $$? = 1 && fgrep failed test-assert.out > /dev/null $(SUMMARIZE_CMD)
check_test-assertC: tst-assert



check_test-assert: test-assert
	@# one argument, "ok" should not error
	$(DVGRIND) ./test-assert ok $(SUMMARIZE_CMD)

check_%: %
	$(VGRIND)  ./$< $(VERBVERBOSE) $(SUMMARIZE_CMD)

benchmark-test.o: ../brt.h ../brt-search.h ../../include/db.h
ifeq ($(CIL),1)

BRT_SOURCES = \
  block_allocator \
  bread \
  brt-serialize \
  brt-verify \
  brt \
  cachetable \
  fifo \
  fingerprint \
  key \
  leafentry \
  log \
  log_code \
  memory \
  memarena \
  mempool \
  omt \
  recover \
  roll \
  toku_assert \
  ybt \
  x1764 \
  trace_mem \
  threadpool \
# keep this line so I can ha vea \ on the previous line
$(BINS): $(patsubst %,../%.o,$(BRT_SOURCES)) $(CYG_ADD_LIBZ)
else
$(BINS): ../newbrt.o $(CYG_ADD_LIBZ)
endif

test-inc-split test-del-inorder: ../brt-test-helpers.o

clean:
	rm -rf $(BINS) *.o *.bb *.bbg *.da *.gcov *.gcno *.gcda
	rm -rf test_oexcl.c.tmp *.brt *.dir *.tdb *.dat *.out
	rm -rf *.exe *.pdb *.ilk *.obj
	rm -f cachetable-fd-test.ctest2.data

foo: ../cachetable.o ../fifo.o ../fingerprint.o ../key.o ../memory.o ../memarena.o ../mempool.o ../omt.o ../toku_assert.o ../ybt.o ../x1764.o ../trace_mem.o ../threadpool.o
