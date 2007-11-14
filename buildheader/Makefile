CFLAGS = -Wall -W -O2 -Werror -g
MYSQL_H = -I../../berkeleydb/db-4.1.25/build_unix/
runs: runs_mysql runs_installed
hs: db.h_4_3 db.h_4_1
# the mysql versions use the header in the mysql distribution.
runs_mysql: ./sample_offsets_mysql
	./sample_offsets_mysql
runs_installed: sample_offsets
	./sample_offsets
sample_offsets_mysql: CPPFLAGS=$(MYSQL_H)
sample_offsets_mysql: sample_offsets.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

make_db_h_4_1.o: make_db_h.c sample_offsets_32_4_1.h sample_offsets_64_4_1.h
	$(CC) $(CFLAGS) -I. $(MYSQL_H) $< -c -o $@ -DUSE_MAJOR=4 -DUSE_MINOR=1
db.h_4_1: make_db_h_4_1
	./make_db_h_4_1 > $@

make_db_h_4_3.o: make_db_h.c sample_offsets_32_4_3.h sample_offsets_64_4_3.h
	$(CC) $(CFLAGS) -I. $< -c -o $@ -DUSE_MAJOR=4 -DUSE_MINOR=3
db.h_4_3: make_db_h_4_3
	./make_db_h_4_3 > $@

sample_offsets:
sample_offsets_local: ./db.h
sample_offsets_local: sample_offsets.c
	$(CC) $(CFLAGS) -I. -DLOCAL $< -o $@
 
check32: sample_offsets_local
	./sample_offsets_local
	diff sample_offsets_local.h sample_offsets_32.h

clean:
	rm sample_offsets_mysql sample_offsets
install_4_1: db.h_4_1
	cp db.h_4_1 ../include/db.h

# Note: To make the compile work, you must cd to the ../../berkeleydb/db-4.1.25/build_unix
# and do ../dist/configure

# to build mysql-5.0.27 with berkeley db 4.1.25
# ./configure --with-berkeley-db=../../berkeleydb/db-4.1.25/build_unix
