CFLAGS = -std=gnu89 -W -Wall -Wno-unused -g -fPIC -O
LFLAGS = -l CPPFLAGS = -I../include -I../newbrt

#cc  $(CPPFLAGS) $(DBBINS) -shared -o libdb.so $(CFLAGS)

BDB_DUMP=/usr/local/BerkeleyDB.4.1/bin/db_dump
BDB_LOAD=/usr/local/BerkeleyDB.4.1/bin/db_load

UTILS=      \
   ydb_gen  \
#   ydb_dump \
#   ydb_load \
#End

.PHONY: all clean test test_gen test_gen_hex

all: $(UTILS)

test: test_gen

test_gen: test_gen_hex

# SHELL=/bin/bash

BDB_LOAD=/usr/local/BerkeleyDB.4.1/bin/db_load
BDB_DUMP=/usr/local/BerkeleyDB.4.1/bin/db_dump

TEST_GEN_HEX_FLAGS=-n 1000 -m 0 -M 1024 -r 5

test_gen_hex:
	#Generating 10,000 keys.  0 to 1024 bytes (not including identifier overhead)
	echo "Generating text input > db > text"
	rm -f test_gen_1
	./ydb_gen $(TEST_GEN_HEX_FLAGS) | $(BDB_LOAD) test_gen_1
	$(BDB_DUMP) test_gen_1 > 1
	./ydb_gen -Hf > 2
	./ydb_gen -hf $(TEST_GEN_HEX_FLAGS) -d g -s h | tr "h" "\n" | sort -t g -k 1,1 | tr -d "\n" | tr "g" "\n" >> 2
	./ydb_gen -Fh  >> 2
	diff -q 1 2
	if ! diff -q 1 2; then echo Files different!; exit 1; fi

#if diff -q <(echo "foo") <(echo "foo") > /dev/null; then echo yes; else echo no; fi
clean:
	rm -rf *.so *.o $(UTILS)

