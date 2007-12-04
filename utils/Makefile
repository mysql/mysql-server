ifndef BDBDIR
BDBDIR=/usr/local/BerkeleyDB.4.4
endif
BDB_DUMP=$(BDBDIR)/bin/db_dump
BDB_LOAD=$(BDBDIR)/bin/db_load

CPPFLAGS = -I$(BDBDIR)/include
CFLAGS = -std=gnu89 -W -Wall -Wno-unused -g
LDFLAGS = -L$(BDBDIR)/lib -ldb -lpthread -Wl,-rpath,$(BDBDIR)/lib
# LDFLAGS = -L$(BDBDIR)/lib -ldb -lpthread
STATIC_LDFLAGS = -static -L../lib -ldb -lz

UTILS=            \
   tokudb_gen     \
   tokudb_load    \
   tokudb_dump    \
#End
STATIC_UTILS=$(patsubst %,%_static,$(UTILS))

.PHONY: all clean test test_gen test_gen_hex test_load test_dump

all: $(UTILS) $(STATIC_UTILS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

%_static: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(STATIC_LDFLAGS)

strip: $(STATIC_UTILS) 
	strip $(STATIC_UTILS)

test: test_gen test_load test_dump test_bdb_tokudb

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
	$(BDB_DUMP) $@.tokudb.temp > $@.dump.tokudb.temp
	if ! diff -q $@.dump.bdb.temp $@.dump.tokudb.temp; then echo "Test Failed!"; exit 1; fi

test_dump:
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	echo "Generating text input > db > text"
	rm -f $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) > $@.gen.temp
	$(BDB_LOAD) $@.bdb.temp < $@.gen.temp
	$(BDB_DUMP) $@.bdb.temp > $@.dump.bdb.temp
	./tokudb_dump $@.bdb.temp > $@.dump.tokudb.temp
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
	rm -rf *.so *.o $(UTILS) $(STATIC_UTILS) *.temp

