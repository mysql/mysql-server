ifndef BDBDIR
BDBDIR=/usr/local/BerkeleyDB.4.4
endif
BDB_DUMP=$(BDBDIR)/bin/db_dump
BDB_LOAD=$(BDBDIR)/bin/db_load

DIFF=diff -I 'db_pagesize='

OPTFLAGS = -O2
CFLAGS = -std=gnu89 -W -Wall -Wno-unused -g $(OPTFLAGS) $(GCOV_FLAGS)

# vars to compile bins that handle tokudb using libdb.so
# when one uses relative address in an rpath, the library better be located relative
# to the current directory
CPPFLAGS = -I../include

ifneq ($(OSX),)
#Note: OSX 10.4 needs DYLD_LIBRARY_PATH.  OSX 10.5 claims to support -rpath.
 TDB_LOADLIBES = 
 BDB_LOADLIBES =
 SETTOKUENV=export DYLD_LIBRARY_PATH=.. ;
 UNSETTOKUENV=export DYLD_LIBRARY_PATH=$(BDBDIR)/lib ;
else
 TDB_LOADLIBES = -Wl,-rpath,$(PWD)/../lib
 BDB_LOADLIBES = -Wl,-rpath,$(BDBDIR)/lib
 SETTOKUENV=
 UNSETTOKUENV=
endif
LDFLAGS = -L../lib -ltokudb -lpthread $(TDB_LOADLIBES)

# vars to compile bins that handle tokudb using libtokudb.a
STATIC_CPPFLAGS = -I../include 
STATIC_LDFLAGS = ../lib/libtokudb.a -lz

# vars to compile bins that handle bdb
BDB_CPPFLAGS = -I$(BDBDIR)/include
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb -lpthread $(BDB_LOADLIBES)

UTILS=            \
   tokudb_gen     \
   tokudb_load    \
   tokudb_dump    \
#End
BDB_UTILS=$(patsubst %,%.bdb,$(UTILS))
STATIC_UTILS=$(patsubst %,%_static,$(UTILS))

HERE=utils

ifeq ($(SUMMARIZE),1)
SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sPASS\n" $(HERE)/$@; else printf "%-60sFAIL\n" $(HERE)/$@ ; test 0 = 1; fi
else
SUMMARIZE_CMD =
endif

.PHONY: all clean test test_gen test_gen_hex test_load test_dump

build all: $(UTILS) $(BDB_UTILS) $(STATIC_UTILS)

coverage: $(UTILS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

%.bdb: %.c
	$(CC) $(BDB_CPPFLAGS) $(CFLAGS) -o $@ $< $(BDB_LDFLAGS)

%_static: %.c ../lib/libtokudb.a
	$(CC) $(STATIC_CPPFLAGS) $(CFLAGS) -o $@ $< $(STATIC_LDFLAGS)

strip: $(STATIC_UTILS) 
	strip $(STATIC_UTILS)

check: $(UTILS) $(BDB_UTILS) $(STATIC_UTILS) test_gen test_load test_dump test_nodup test_dupsort

test-coverage: $(UTILS) test_gen test_load test_dump test_nodup test_dupsort

test_gen: test_gen_hex

TEST_GEN_HEX_NUMKEYS=10000
TEST_GEN_HEX_LENGTHMIN=0
TEST_GEN_HEX_LENGTHLIMIT=1024
TEST_GEN_HEX_FLAGS=-n $(TEST_GEN_HEX_NUMKEYS) -m $(TEST_GEN_HEX_LENGTHMIN) -M $(TEST_GEN_HEX_LENGTHLIMIT) -r 5

test_gen_hex:
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	@#echo "Generating text input > db > text"
	rm -f $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) > $@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_LOAD) $@.db.temp < $@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_DUMP) $@.db.temp > $@.load_dump.temp && \
	./tokudb_gen -Hf > $@.gen_sorted.temp && \
	export LC_ALL=C;./tokudb_gen -hf $(TEST_GEN_HEX_FLAGS) -d "\t" -s "\n" | sort -k 1,1 | tr -d "\n" | tr "\t" "\n" >> $@.gen_sorted.temp && \
	./tokudb_gen -Fh  >> $@.gen_sorted.temp && \
	$(DIFF) -q $@.load_dump.temp $@.gen_sorted.temp \
	$(SUMMARIZE_CMD)

test_load: test_load_hex test_load_text test_load_text_noendline

test_load_hex:
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	@#echo "Generating text input > db > text"
	rm -f $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) > $@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_LOAD) $@.bdb.temp < $@.gen.temp && \
	$(SETTOKUENV)   ./tokudb_load $@.tokudb.temp < $@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_DUMP) $@.bdb.temp > $@.dump.bdb.temp && \
	$(SETTOKUENV)   ./tokudb_dump $@.tokudb.temp > $@.dump.tokudb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tokudb.temp \
	$(SUMMARIZE_CMD)

TEST_GEN_TEXT_FLAGS=-n $(TEST_GEN_HEX_NUMKEYS) -m $(TEST_GEN_HEX_LENGTHMIN) -M $(TEST_GEN_HEX_LENGTHLIMIT) -r 5 -TP

test_load_text:
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	@#echo "Generating text input > db > text"
	rm -f $@.*.temp
	./tokudb_gen $(TEST_GEN_TEXT_FLAGS) > $@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_LOAD)   -T -t btree $@.bdb.temp < $@.gen.temp && \
	$(SETTOKUENV)   ./tokudb_load -T -t btree $@.tokudb.temp < $@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_DUMP)   -p $@.bdb.temp > $@.dump.bdb.temp && \
	$(SETTOKUENV)   ./tokudb_dump -p $@.tokudb.temp > $@.dump.tokudb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tokudb.temp \
	$(SUMMARIZE_CMD)

test_load_text_noendline:
	@#echo "Testing no end of line at end of file."
	rm -f $@.*.temp
	echo -en "key\nvalue" > $@.gen.temp
	./tokudb_load -T -t btree $@.tokudb.temp < $@.gen.temp $(SUMMARIZE_CMD)


test_dump:
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	@#echo "Generating text input > db > text"
	rm -f $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) > $@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_LOAD) $@.bdb.temp < $@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_DUMP) $@.bdb.temp > $@.dump.bdb.temp && \
	$(UNSETTOKUENV) ./tokudb_dump.bdb $@.bdb.temp > $@.dump.tokudb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tokudb.temp \
	$(SUMMARIZE_CMD)

test_nodup:
	rm -rf $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) >$@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_LOAD) $@.bdb.temp <$@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_DUMP) $@.bdb.temp >$@.dump.bdb.temp && \
	$(SETTOKUENV)   ./tokudb_load $@.tdb.temp <$@.gen.temp && \
	$(SETTOKUENV)   ./tokudb_dump $@.tdb.temp >$@.dump.tdb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tdb.temp && \
	$(SETTOKUENV)   ./tokudb_load_static $@.tdb.temp <$@.gen.temp && \
	$(SETTOKUENV)   ./tokudb_dump_static $@.tdb.temp >$@.dump.tdb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tdb.temp \
	$(SUMMARIZE_CMD)

test_dupsort:
	rm -rf $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) -DS >$@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_LOAD) $@.bdb.temp <$@.gen.temp && \
	$(UNSETTOKUENV) $(BDB_DUMP) $@.bdb.temp >$@.dump.bdb.temp && \
	$(SETTOKUENV)   ./tokudb_load $@.tdb.temp <$@.gen.temp && \
	$(SETTOKUENV)   ./tokudb_dump $@.tdb.temp >$@.dump.tdb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tdb.temp && \
	$(SETTOKUENV)   ./tokudb_load_static $@.tdb.temp <$@.gen.temp && \
	$(SETTOKUENV)   ./tokudb_dump_static $@.tdb.temp >$@.dump.tdb.temp && \
	$(DIFF) -q $@.dump.bdb.temp $@.dump.tdb.temp \
        $(SUMMARIZE_CMD)

#if $(DIFF) -q <(echo "foo") <(echo "foo") > /dev/null; then echo yes; else echo no; fi
clean:
	rm -rf *.so *.o $(UTILS) $(BDB_UTILS) $(STATIC_UTILS) *.temp *.gcno *.gcda *.gcov

