# -*- Mode: Makefile -*-
# standard build: make
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

.DEFAULT_GOAL= build
TOKUROOT=../
INCLUDEDIRS=-I. -I$(TOKUROOT)toku_include -I$(TOKUROOT)include -I$(TOKUROOT)newbrt -I$(TOKUROOT)src
DEPEND_COMPILE += \
	./*.h \
#end

HERE = utils
include $(TOKUROOT)toku_include/Makefile.include
ifndef BDBDIR
BDBDIR=/usr/local/BerkeleyDB.4.6
endif
BDB_DUMP= LD_LIBRARY_PATH=$(BDBDIR)/lib $(BDBDIR)/bin/db_dump$(BINSUF) -h dir.$@.bdb
BDB_LOAD= LD_LIBRARY_PATH=$(BDBDIR)/lib $(BDBDIR)/bin/db_load$(BINSUF) -h dir.$@.bdb

DIFF=diff -q -I 'db_pagesize='

# vars to compile bins that handle tokudb using libdb.so
# when one uses relative address in an rpath, the library better be located relative
# to the current directory

# vars to compile bins that handle tokudb using libtokudb.a

CPPFLAGS+= -D_GNU_SOURCE
# vars to compile bins that handle bdb
BDB_CPPFLAGS = -I$(BDBDIR)/include
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb -lpthread $(BDB_LOADLIBES)

UTILS=            \
   tokudb_gen$(BINSUF)     \
   tokudb_load$(BINSUF)    \
   tokudb_dump$(BINSUF)    \
#End
BDB_UTILS=$(patsubst %$(BINSUF),%.bdb$(BINSUF),$(UTILS))
STATIC_UTILS=$(patsubst %$(BINSUF),%_static$(BINSUF),$(UTILS))

ifeq ($(OS_CHOICE),windows)
$(UTILS): LINK_FILES+=$(WIN_YDB)
$(UTILS): $(WIN_YDB)
else
$(UTILS): DLINK_FILES=$(TOKUROOT)lib/libtokudb.$(SOEXT) $(TOKUROOT)lib/libtokuportability.$(SOEXT)
NEWBRT_OBJS=$(patsubst %,../%,$(shell cat $(TOKUROOT)lib/newbrt.olist))
TOKUPORTABILITY_OBJS=$(patsubst %,../%,$(shell cat $(TOKUROOT)lib/tokuportability.olist))
YDB_OBJS=$(wildcard ../src/*.$(OEXT))
$(STATIC_UTILS): LINK_FILES+= $(NEWBRT_OBJS) $(TOKUPORTABILITY_OBJS) $(YDB_OBJS) ../src/lock_tree/locktree.a ../src/range_tree/rangetree.a

ifeq ($(BRTLOADER),cilk)
	DLINK_FILES += cilkrts stdc++
	RPATH_DIRS +=  $(CILKROOT)lib64
endif

$(UTILS) $(STATIC_UTILS): RPATH_DIRS+=$(TOKUROOT)lib
endif

#empty

ifeq ($(OS_CHOICE),windows)
ifdef BDBDIR
$(BDB_UTILS): INCLUDEDIRS=-I$(BDBDIR)/include
$(BDB_UTILS): RPATH_DIRS=$(BDBDIR)/lib
endif
ifeq ($(DEBUG),0)
WINDOWS_BDB_LIB_NAME=libdb.$(SOEXT)
$(WINDOWS_BDB_LIB_NAME):
	cp $(BDBDIR)/lib/libdb[0-9][0-9].$(SOEXT) ./
else
WINDOWS_BDB_LIB_NAME=libdbd.$(SOEXT)
$(WINDOWS_BDB_LIB_NAME):
	cp $(BDBDIR)/lib/libdb[0-9][0-9]d.$(SOEXT) ./
endif
$(BDB_UTILS): DLINK_FILES=$(BDBDIR)/lib/$(WINDOWS_BDB_LIB_NAME)
#empty
else
WINDOWS_BDB_LIB_NAME=
#linux
ifdef BDBDIR
$(BDB_UTILS): INCLUDEDIRS=-I$(BDBDIR)/include
$(BDB_UTILS): RPATH_DIRS=$(BDBDIR)/lib
endif
$(BDB_UTILS): DLINK_FILES=db.$(SOEXT)
endif
$(BDB_UTILS): CPPFLAGS+=-DTOKU_ALLOW_DEPRECATED
$(BDB_UTILS): LDLIBS+=-ltokuportability
$(BDB_UTILS): LDFLAGS+=-L$(TOKUROOT)lib -Wl,-rpath,$(shell pwd)/$(TOKUROOT)lib
#empty on purpose
$(BDB_UTILS): CPPFLAGS+=-DIS_TDB=0
$(UTILS) $(STATIC_UTILS): CPPFLAGS+=-DIS_TDB=1
$(STATIC_UTILS): CPPFLAGS+=-DTDB_IS_STATIC=1
$(UTILS): CPPFLAGS+=-DTDB_IS_STATIC=0

HERE=utils

ifeq ($(SUMMARIZE),1)
SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sPASS\n" $(HERE)/$@; else printf "%-60sFAIL\n" $(HERE)/$@ ; test 0 = 1; fi
else
SUMMARIZE_CMD =
endif

.PHONY: all clean test test_gen test_gen_hex test_load test_dump

build all: build.tdb build.bdb;
ifeq ($(OS_CHOICE),windows)
build.tdb: $(UTILS) copy.tdb;
build.bdb: $(BDB_UTILS) copy.bdb;
else
build.tdb: $(UTILS) $(STATIC_UTILS);
build.bdb: $(BDB_UTILS);
endif

copy.tdb: $(PTHREAD_LOCAL)
	cp ../lib/*.dll ./
copy.bdb:
	cp $(BDBDIR)/lib/*.dll ./

coverage: $(UTILS)

%$(BINSUF): %.c
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST) -DUSE_TDB=1

%.bdb$(BINSUF): %.c
	$(CC) $< $(BIN_FROM_C_FLAGS) $(LINK_MUST_BE_LAST) -DUSE_BDB=1

%_static$(BINSUF): %.c $(TOKUROOT)lib/libtokuportability.$(AEXT)
	$(CC) $< $(BIN_FROM_C_FLAGS)  $(TOKUROOT)lib/libtokuportability.$(AEXT) $(LINK_MUST_BE_LAST)

strip: $(STATIC_UTILS) 
	strip $(STATIC_UTILS)

check: build test_gen test_load test_dump test_nodup

test-coverage: $(UTILS) test_gen test_load test_dump test_nodup

test_gen: test_gen_hex

TEST_GEN_HEX_NUMKEYS=10000
TEST_GEN_HEX_LENGTHMIN=0
TEST_GEN_HEX_LENGTHLIMIT=1024

TEST_GEN_HEX_FLAGS=-n $(TEST_GEN_HEX_NUMKEYS) -m $(TEST_GEN_HEX_LENGTHMIN) -M $(TEST_GEN_HEX_LENGTHLIMIT) -r 5

TDB_GEN=./tokudb_gen$(BINSUF)
TDB_LOAD=./tokudb_load$(BINSUF) -h dir.$@.tdb
TDB_DUMP=./tokudb_dump$(BINSUF) -h dir.$@.tdb
TDB_DUMPBDB=./tokudb_dump.bdb$(BINSUF) -h dir.$@.bdb


ifeq ($(OS_CHOICE),windows)
    LINEEND   = "\r\n"
    TO_NATIVE = unix2dos --u2d
else
    LINEEND   = "\n"
    TO_NATIVE = cat
endif

test_gen_hex: build
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	@#echo "Generating text input > db > text"
	rm -rf dir.$@.*
	mkdir dir.$@.tdb
	mkdir dir.$@.bdb
	$(TDB_GEN) $(TEST_GEN_HEX_FLAGS) > $@.gen.temp && \
	$(BDB_LOAD) $@.db.temp < $@.gen.temp && \
	$(BDB_DUMP) $@.db.temp > $@.load_dump.temp && \
	$(TDB_GEN) -H -f > $@.gen_sorted.temp && \
	export LC_ALL=C;$(TDB_GEN) -h -f $(TEST_GEN_HEX_FLAGS) -d "\t" -s "\n" | sort -k 1,1 | tr -d $(LINEEND) | tr "\t" "\n" | $(TO_NATIVE) >> $@.gen_sorted.temp && \
	$(TDB_GEN) -F -h  >> $@.gen_sorted.temp && \
	$(DIFF) $@.load_dump.temp $@.gen_sorted.temp \
	$(SUMMARIZE_CMD)

test_load: test_load_hex test_load_text test_load_text_noendline

test_load_hex: build
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	@#echo "Generating text input > db > text"
	rm -rf dir.$@.*
	mkdir dir.$@.tdb
	mkdir dir.$@.bdb
	$(TDB_GEN) $(TEST_GEN_HEX_FLAGS) > $@.gen.temp && \
	$(BDB_LOAD) $@.bdb.temp < $@.gen.temp && \
	$(TDB_LOAD) $@.tokudb.temp < $@.gen.temp && \
	$(BDB_DUMP) $@.bdb.temp > $@.dump.bdb.temp && \
	$(TDB_DUMP) $@.tokudb.temp > $@.dump.tokudb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tokudb.temp \
	$(SUMMARIZE_CMD)

TEST_GEN_TEXT_FLAGS=-n $(TEST_GEN_HEX_NUMKEYS) -m $(TEST_GEN_HEX_LENGTHMIN) -M $(TEST_GEN_HEX_LENGTHLIMIT) -r 5 -T -P

test_load_text: build
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	@#echo "Generating text input > db > text"
	rm -rf dir.$@.*
	mkdir dir.$@.tdb
	mkdir dir.$@.bdb
	$(TDB_GEN) $(TEST_GEN_TEXT_FLAGS) > $@.gen.temp && \
	$(BDB_LOAD)   -T -t btree $@.bdb.temp < $@.gen.temp && \
	$(TDB_LOAD) -T -t btree $@.tokudb.temp < $@.gen.temp && \
	$(BDB_DUMP)   -p $@.bdb.temp > $@.dump.bdb.temp && \
	$(TDB_DUMP) -p $@.tokudb.temp > $@.dump.tokudb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tokudb.temp \
	$(SUMMARIZE_CMD)

test_load_text_noendline: build
	@#echo "Testing no end of line at end of file."
	rm -rf dir.$@.*
	mkdir dir.$@.tdb
	mkdir dir.$@.bdb
	echo -en "key\nvalue" > $@.gen.temp
	$(TDB_LOAD) -T -t btree $@.tokudb.temp < $@.gen.temp $(SUMMARIZE_CMD)


test_dump: build
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	@#echo "Generating text input > db > text"
	rm -rf dir.$@.*
	mkdir dir.$@.tdb
	mkdir dir.$@.bdb
	$(TDB_GEN) $(TEST_GEN_HEX_FLAGS) > $@.gen.temp && \
	$(BDB_LOAD) $@.bdb.temp < $@.gen.temp && \
	$(BDB_DUMP) $@.bdb.temp > $@.dump.bdb.temp && \
	$(TDB_DUMPBDB) $@.bdb.temp > $@.dump.tokudb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tokudb.temp \
	$(SUMMARIZE_CMD)

test_nodup: build
	rm -rf dir.$@.*
	mkdir dir.$@.tdb
	mkdir dir.$@.bdb
	$(TDB_GEN) $(TEST_GEN_HEX_FLAGS) >$@.gen.temp && \
	$(BDB_LOAD) $@.bdb.temp <$@.gen.temp && \
	$(BDB_DUMP) $@.bdb.temp >$@.dump.bdb.temp && \
	$(TDB_LOAD) $@.tdb.temp <$@.gen.temp && \
	$(TDB_DUMP) $@.tdb.temp >$@.dump.tdb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tdb.temp && \
	$(TDB_LOAD) $@.tdb.temp <$@.gen.temp && \
	$(TDB_DUMP) $@.tdb.temp >$@.dump.tdb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tdb.temp \
	$(SUMMARIZE_CMD)

#if $(DIFF) -q <(echo "foo") <(echo "foo") > /dev/null; then echo yes; else echo no; fi
clean:
	rm -rf *.so *.o $(UTILS) $(BDB_UTILS) $(STATIC_UTILS) *.temp *.gcno *.gcda *.gcov *.temp.clean *.temp.dirty
	rm -rf tokudb.directory tokudb.environment *.log *.tokudb *.tokulog
	rm -rf dir.*


