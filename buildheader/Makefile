CFLAGS = -Wall -W -O2 -Werror -g
runs: runs_mysql runs_installed
# the mysql versions use the header in the mysql distribution.
runs_mysql: ./sample_offsets_mysql
	./sample_offsets_mysql
runs_installed: sample_offsets
	./sample_offsets
sample_offsets_mysql: CPPFLAGS=-I../../mysql-5.0.27/mysql-5.0.27/bdb/build_unix/
sample_offsets_mysql: sample_offsets.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

make_db_h.o: make_db_h.c sample_offsets_32.h sample_offsets_64.h
sample_offsets:
sample_offsets_local: ./db.h
sample_offsets_local: sample_offsets.c
	$(CC) $(CFLAGS) -I. -DLOCAL $< -o $@
 
db.h: make_db_h
	./make_db_h  > db.h
check32: sample_offsets_local
	./sample_offsets_local
	diff sample_offsets_local.h sample_offsets_32.h

clean:
	rm sample_offsets_mysql sample_offsets
install: db.h
	cp db.h ../include/

# Note: To make the ompile work, you must cd to the ../../mysql-5.0.27/mysql-5.0.27/bdb/build_unix
# and do ../dist/configure
