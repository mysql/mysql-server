ifndef BDBDIR
BDBDIR=/usr/local/BerkeleyDB.4.4
endif
BDB_DUMP=$(BDBDIR)/bin/db_dump
BDB_LOAD=$(BDBDIR)/bin/db_load

CFLAGS = -std=gnu89 -W -Wall -Wno-unused -g

# vars to compile bins that handle tokudb using libdb.so
# when one uses relative address in an rpath, the library better be located relative
# to the current directory
CPPFLAGS = -I../include
LDFLAGS = -L../lib -ldb -lpthread -Wl,-rpath,$(PWD)/../lib

# vars to compile bins that handle tokudb using libdb.a
STATIC_CPPFLAGS = -I../include 
STATIC_LDFLAGS = ../lib/libdb.a -lz

# vars to compile bins that handle bdb
BDB_CPPFLAGS = -I$(BDBDIR)/include
BDB_LDFLAGS = -L$(BDBDIR)/lib -ldb -lpthread -Wl,-rpath,$(BDBDIR)/lib

UTILS=            \
   tokudb_gen     \
   tokudb_load    \
   tokudb_dump    \
#End
BDB_UTILS=$(patsubst %,%.bdb,$(UTILS))
STATIC_UTILS=$(patsubst %,%_static,$(UTILS))

.PHONY: all clean test test_gen test_gen_hex test_load test_dump

all: $(UTILS) $(BDB_UTILS) $(STATIC_UTILS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

%.bdb: %.c
	$(CC) $(BDB_CPPFLAGS) $(CFLAGS) -o $@ $< $(BDB_LDFLAGS)

%_static: %.c
	$(CC) $(STATIC_CPPFLAGS) $(CFLAGS) -o $@ $< $(STATIC_LDFLAGS)

strip: $(STATIC_UTILS) 
	strip $(STATIC_UTILS)

test: $(UTILS) $(BDB_UTILS) $(STATIC_UTILS) test_gen test_load test_dump test_bdb_tokudb

test_gen: test_gen_hex

TEST_GEN_HEX_NUMKEYS=10000
TEST_GEN_HEX_LENGTHMIN=0
TEST_GEN_HEX_LENGTHLIMIT=1024
TEST_GEN_HEX_FLAGS=-n $(TEST_GEN_HEX_NUMKEYS) -m $(TEST_GEN_HEX_LENGTHMIN) -M $(TEST_GEN_HEX_LENGTHLIMIT) -r 5

test_gen_hex:
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	echo "Generating text input > db > text"
	rm -f $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) > $@.gen.temp
	$(BDB_LOAD) $@.db.temp < $@.gen.temp
	$(BDB_DUMP) $@.db.temp > $@.load_dump.temp
	./tokudb_gen -Hf > $@.gen_sorted.temp
	export LC_ALL=C;./tokudb_gen -hf $(TEST_GEN_HEX_FLAGS) -d "\t" -s "\n" | sort -k 1,1 | tr -d "\n" | tr "\t" "\n" >> $@.gen_sorted.temp
	./tokudb_gen -Fh  >> $@.gen_sorted.temp
	if ! diff -q $@.load_dump.temp $@.gen_sorted.temp; then echo "Test Failed!"; exit 1; fi
	rm $@.*.temp

test_load:
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	echo "Generating text input > db > text"
	rm -f $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) > $@.gen.temp
	$(BDB_LOAD) $@.bdb.temp < $@.gen.temp
	./tokudb_load $@.tokudb.temp < $@.gen.temp
	$(BDB_DUMP) $@.bdb.temp > $@.dump.bdb.temp
	./tokudb_dump $@.tokudb.temp > $@.dump.tokudb.temp
	if ! diff -q $@.dump.bdb.temp $@.dump.tokudb.temp; then echo "Test Failed!"; exit 1; fi

test_dump:
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	echo "Generating text input > db > text"
	rm -f $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) > $@.gen.temp
	$(BDB_LOAD) $@.bdb.temp < $@.gen.temp
	$(BDB_DUMP) $@.bdb.temp > $@.dump.bdb.temp
	./tokudb_dump.bdb $@.bdb.temp > $@.dump.tokudb.temp
	if ! diff -q $@.dump.bdb.temp $@.dump.tokudb.temp; then echo "Test Failed!"; exit 1; fi

test_bdb_tokudb:
	rm -rf $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) >$@.gen.temp
	$(BDB_LOAD) $@.bdb.temp <$@.gen.temp
	$(BDB_DUMP) $@.bdb.temp >$@.dump.bdb.temp
	./tokudb_load_static $@.tdb.temp <$@.gen.temp
	./tokudb_dump_static $@.tdb.temp >$@.dump.tdb.temp
	if ! diff -q $@.dump.bdb.temp $@.dump.tdb.temp; then echo "$@ failed"; exit 1; fi

#if diff -q <(echo "foo") <(echo "foo") > /dev/null; then echo yes; else echo no; fi
clean:
	rm -rf *.so *.o $(UTILS) $(BDB_UTILS) $(STATIC_UTILS) *.temp

