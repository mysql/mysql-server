CFLAGS = -std=gnu89 -W -Wall -Wno-unused -g -fPIC -I /usr/local/Berkeleydb.4.1/include/ -ldb
LFLAGS = -l CPPFLAGS = -I../include -I../newbrt

#cc  $(CPPFLAGS) $(DBBINS) -shared -o libdb.so $(CFLAGS)

BDB_DUMP=/usr/local/Berkeleydb.4.1/bin/db_dump
BDB_LOAD=/usr/local/Berkeleydb.4.1/bin/db_load

UTILS=            \
   tokudb_gen     \
   tokudb_load    \
#   tokudb_dump    \
#End

.PHONY: all clean test test_gen test_gen_hex

all: $(UTILS)

test: test_gen test_load

test_gen: test_gen_hex

# SHELL=/bin/bash

BDB_LOAD=/usr/local/Berkeleydb.4.1/bin/db_load
BDB_DUMP=/usr/local/Berkeleydb.4.1/bin/db_dump

TEST_GEN_HEX_NUMKEYS=10000
TEST_GEN_HEX_LENGTHMIN=0
TEST_GEN_HEX_LENGTHLIMIT=1024
TEST_GEN_HEX_FLAGS=-n $(TEST_GEN_HEX_NUMKEYS) -m $(TEST_GEN_HEX_LENGTHMIN) -M $(TEST_GEN_HEX_LENGTHLIMIT) -r 5

test_gen_hex:
	#Generating $(TEST_GEN_HEX_NUMKEYS) keys.  [$(TEST_GEN_HEX_LENGTHMIN),$(TEST_GEN_HEX_LENGTHLIMIT)) bytes + identifier overhead
	echo "Generating text input > db > text"
	rm -f $@.*.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) | $(BDB_LOAD) $@.db.temp
	$(BDB_DUMP) $@.db.temp > $@.load_dump.temp
	./tokudb_gen -Hf > $@.gen_sorted.temp
	./tokudb_gen -hf $(TEST_GEN_HEX_FLAGS) -d "\t" -s "\n" | sort -k 1,1 | tr -d "\n" | tr "\t" "\n" >> $@.gen_sorted.temp
	./tokudb_gen -Fh  >> $@.gen_sorted.temp
	if ! diff -q $@.load_dump.temp $@.gen_sorted.temp; then echo "Test Failed!"; exit 1; fi
	rm $@.*.temp

test_load: test_load_hex
	#Generating

#if diff -q <(echo "foo") <(echo "foo") > /dev/null; then echo yes; else echo no; fi
clean:
	rm -rf *.so *.o $(UTILS) *.temp

