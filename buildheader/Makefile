CFLAGS = -Wall -W -O2 -Werror -g
make_db_h.o: make_db_h.c sample_offsets_32.h sample_offsets_64.h
sample_offsets: CPPFLAGS=-I../../mysql-5.0.27/mysql-5.0.27/bdb/build_unix/
run: make_db_h
	./make_db_h
runs: sample_offsets
	./sample_offsets

# Note: To make the ompile work, you must cd to the ../../mysql-5.0.27/mysql-5.0.27/bdb/build_unix
# and do ../dist/configure
