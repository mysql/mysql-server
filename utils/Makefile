CFLAGS = -std=gnu89 -W -Wall -Wno-unused -g -fPIC -I /usr/local/Berkeletokudb.4.1/include/ -ldb
LFLAGS = -l CPPFLAGS = -I../include -I../newbrt

#cc  $(CPPFLAGS) $(DBBINS) -shared -o libdb.so $(CFLAGS)

BDB_DUMP=/usr/local/Berkeletokudb.4.1/bin/db_dump
BDB_LOAD=/usr/local/Berkeletokudb.4.1/bin/db_load

UTILS=            \
   tokudb_gen     \
   tokudb_load    \
#   tokudb_dump    \
#End

.PHONY: all clean test test_gen test_gen_hex

all: $(UTILS)

test: test_gen

test_gen: test_gen_hex

# SHELL=/bin/bash

BDB_LOAD=/usr/local/Berkeletokudb.4.1/bin/db_load
BDB_DUMP=/usr/local/Berkeletokudb.4.1/bin/db_dump

TEST_GEN_HEX_FLAGS=-n 10000 -m 0 -M 1024 -r 5

test_gen_hex:
	#Generating 10,000 keys.  0 to 1024 bytes (not including identifier overhead)
	echo "Generating text input > db > text"
	rm -f $@.db.temp
	./tokudb_gen $(TEST_GEN_HEX_FLAGS) | $(BDB_LOAD) $@.db.temp
	$(BDB_DUMP) $@.db.temp > $@.load_dump.temp
	./tokudb_gen -Hf > $@.gen_sorted.temp
	./tokudb_gen -hf $(TEST_GEN_HEX_FLAGS) -d g -s h | tr "h" "\n" | sort -t g -k 1,1 | tr -d "\n" | tr "g" "\n" >> $@.gen_sorted.temp
	./tokudb_gen -Fh  >> $@.gen_sorted.temp
	if ! diff -q $@.load_dump.temp $@.gen_sorted.temp; then echo Files different!; exit 1; fi
	rm $@.*.temp
	

#if diff -q <(echo "foo") <(echo "foo") > /dev/null; then echo yes; else echo no; fi
clean:
	rm -rf *.so *.o $(UTILS) *.temp

