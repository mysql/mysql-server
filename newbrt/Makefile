# On cygwin do:
#   make CYGWIN=cygwin check

# For verbose output do
#   make VERBOSE=1
# For very verbose output do 
#   make VERBOSE=2

# For CIL do
#   make CIL=1

# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
# PROF_FLAGS = -pg
OPTFLAGS = -O3

ifeq ($(VERBOSE),2)
VERBVERBOSE=-v
MAYBEATSIGN= 
else
 ifeq ($(VERBOSE),1)
  VERBVERBOSE=-q
  MAYBEATSIGN=
 else
  VERBVERBOSE=-q
  MAYBEATSIGN=@
 endif
endif

#CFLAG default options
WERROR    = -Werror
WALL      = -Wall -Wextra -Wcast-align -Wbad-function-cast
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
VGRIND    = valgrind --quiet --error-exitcode=1 --leak-check=yes

ifeq ($(CC),icc)
    #icc only:
    OPTFLAGS = -O3 -ip
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

# Add -Wconversion

ifdef BRT_FANOUT
CPPFLAGS += -DBRT_FANOUT=$(BRT_FANOUT)
endif

ifeq ($(CIL),1)
	CC=../../cil/cil-1.3.6/bin/cilly --merge --keepmerged
endif

# When debugging, try: valgrind --show-reachable=yes --leak-check=full ./brt-test

BINS= brtdump \
      tdb_logprint \
# Intentionally left blank

build default: bins libs tdb-recover tdb_logprint $(TEST_OFILES)
	cd tests;$(MAKE) build

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

ifeq ($(CIL),1)
 OFILES = $(patsubst %,%.o,$(BRT_SOURCES))
else
 OFILES = newbrt.o
endif
TEST_OFILES = brt-test-helpers.o

HFILES = $(wildcard *.h)

BRT_C_FILES = $(patsubst %,%.c,$(BRT_SOURCES))

ifeq ($(CIL),1)
else
newbrt.o: $(BRT_C_FILES) $(HFILES)
	$(CC) $(COMBINE_C) $(CFLAGS) $(CPPFLAGS) $(BRT_C_FILES) -o $@
endif

tdb_logprint.o: log-internal.h brttypes.h log.h kv-pair.h log_header.h
tdb_logprint: $(OFILES) $(CYG_ADD_LIBZ)

recover.o: log_header.h log-internal.h log.h brttypes.h kv-pair.h memory.h key.h cachetable.h
tdb-recover: $(OFILES) $(CYG_ADD_LIBZ)

roll.o: log_header.h log-internal.h log.h brttypes.h kv-pair.h memory.h key.h cachetable.h omt.h bread.h

log_code.o: log_header.h wbuf.h log-internal.h rbuf.h
log_header.h: log_code.c
	@echo generated log_code.c so log_header.c was also generated
log_code.c: logformat
	./logformat

libs: $(OFILES) $(CYG_ADD_LIBZ)
bins: $(BINS)
# Put the benchmarktest_256 first since it takes the longest (and we want to use parallelism in the make)

# Put check_benchmarktest_256 first because it is long-running (and therefore on the critical path, so get it started)
check: bins
	cd tests;$(MAKE) check

check-fanout:
	let BRT_FANOUT=4; \
	while [ $$BRT_FANOUT -le 16 ] ;do \
		$(MAKE) clean; $(MAKE) check BRT_FANOUT=$$BRT_FANOUT; \
		let BRT_FANOUT=BRT_FANOUT+1; \
	done

BRT_INTERNAL_H_INCLUDES = brt-internal.h cachetable.h fifo.h omt.h brt.h brt-search.h brttypes.h ybt.h log.h ../include/db.h kv-pair.h memory.h mempool.h leafentry.h log_header.h
brt-test-helpers.o: $(BRT_INTERNAL_H_INCLUDES) toku_assert.h
logformat: logformat.c
brt-serialize-test.o: $(BRT_INTERNAL_H_INCLUDES)

$(OFILES): $(BRT_INTERNAL_H_INCLUDES)


test_toku_malloc_plain_free: newbrt.o


cachetable-test.o: cachetable.h memory.h 
cachetable-test: $(OFILES) $(CYG_ADD_LIBZ)

cachetable-test2.o: cachetable.h memory.h
cachetable-test2: $(OFILES) $(CYG_ADD_LIBZ)

test-assert: newbrt.o

brtdump: $(OFILES) $(CYG_ADD_LIBZ)

test_oexcl: test_oexcl.o newbrt.o


checko2:
ifeq ($(OPTFLAGS),-O3)
	@echo OPTFLAGS=$(OPTFLAGS) ok
else
	@echo OPTFLAGS=$(OPTFLAGS) bad; exit 1
endif

clean: clean-local clean-tests
clean-tests:
	cd tests;$(MAKE) clean
clean-local:
	rm -rf $(BINS) *.o *.bb *.bbg *.da *.gcov *.gcno *.gcda
	rm -rf test_oexcl.c.tmp *.brt
	rm -rf *.obj *.pdb *.ilk *.exe
	rm -rf log_code.c log_header.h logformat

# After doing (cd ../src/tests;make test_log5.recover), run these.  The files should have no differences.
testdump: brtdump
	./brtdump  ../src/tests/dir.test_log5.c.tdb.recover/foo.db > dump.r && ./brtdump  ../src/tests/dir.test_log5.c.tdb/foo.db > dump.o && diff dump.o dump.r

TAGS: ../*/*.c ../*/*.h
	etags ../*/*.c ../*/*.h
ofiles: $(OFILES)
