# How to build db.h_4_4
# 1. cd ../../berkeleydb/db-4.4.20; ../dist/configure; make; make install
# 2. on both 32 and 64 bit architectures
#	make sample_offsets_4_4
# 3. gather the sample_offsets_32/64_4_4.h files into a single directory
# 4. make db.h_4_4
#
# How to build all of the db.h files
# 1. install all of the bdb's
# 2. make samples
# 3. gather the generated sample_offsets header files
# 4. make hs

BDB = 4_4
BDBDIR = ../../berkeleydb
MYSQL_H = -I$(BDBDIR)/db-4.1.25/build_unix/
OPTFLAGS = -O2
CFLAGS = -Wall -W -Werror -g $(OPTFLAGS)

runs: runs_mysql runs_installed
samples: sample_offsets_4_6 sample_offsets_4_5 sample_offsets_4_4 sample_offsets_4_3 sample_offsets_4_1
hs: db.h_4_6 db.h_4_5 db.h_4_4 db.h_4_3 db.h_4_1
# the mysql versions use the header in the mysql distribution.
runs_mysql: ./sample_offsets_mysql
	./sample_offsets_mysql
runs_installed: sample_offsets
	./sample_offsets
sample_offsets_mysql: CPPFLAGS=$(MYSQL_H)
sample_offsets_mysql: sample_offsets.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

make_db_h_4_1.o: make_db_h.c sample_offsets_32_4_1.h sample_offsets_64_4_1.h
	$(CC) $(CFLAGS) -I. -I$(BDBDIR)/db-4.1.25/build_unix $< -c -o $@ -DUSE_MAJOR=4 -DUSE_MINOR=1
db.h_4_1: make_db_h_4_1
	./make_db_h_4_1 > $@
sample_offsets_4_1: sample_offsets.c
	$(CC) $(CFLAGS) -I. -I$(BDBDIR)/db-4.1.25/build_unix $< -o $@
	./sample_offsets_4_1

make_db_h_4_3.o: make_db_h.c sample_offsets_32_4_3.h sample_offsets_64_4_3.h
	$(CC) $(CFLAGS) -I. -I$(BDBDIR)/db-4.3.29/build_unix $< -c -o $@ -DUSE_MAJOR=4 -DUSE_MINOR=3
db.h_4_3: make_db_h_4_3
	./make_db_h_4_3 > $@
sample_offsets_4_3: sample_offsets.c
	$(CC) $(CFLAGS) -I. -I$(BDBDIR)/db-4.3.29/build_unix $< -o $@
	./sample_offsets_4_3

make_db_h_4_4.o: make_db_h.c sample_offsets_32_4_4.h sample_offsets_64_4_4.h
	$(CC) $(CFLAGS) -I. -I$(BDBDIR)/db-4.4.20/build_unix $< -c -o $@ -DUSE_MAJOR=4 -DUSE_MINOR=4
db.h_4_4: make_db_h_4_4
	./make_db_h_4_4 > $@
sample_offsets_4_4: sample_offsets.c
	$(CC) $(CFLAGS) -I. -I$(BDBDIR)/db-4.4.20/build_unix $< -o $@
	./sample_offsets_4_4

make_db_h_4_5.o: make_db_h.c sample_offsets_32_4_5.h sample_offsets_64_4_5.h
	$(CC) $(CFLAGS) -I. -I$(BDBDIR)/db-4.5.20/build_unix $< -c -o $@ -DUSE_MAJOR=4 -DUSE_MINOR=5
db.h_4_5: make_db_h_4_5
	./make_db_h_4_5 > $@
sample_offsets_4_5: sample_offsets.c
	$(CC) $(CFLAGS) -I. -I$(BDBDIR)/db-4.5.20/build_unix $< -o $@
	./sample_offsets_4_5

make_db_h_4_6.o: make_db_h.c sample_offsets_32_4_6.h sample_offsets_64_4_6.h
	$(CC) $(CFLAGS) -I. -I$(BDBDIR)/db-4.6.19/build_unix $< -c -o $@ -DUSE_MAJOR=4 -DUSE_MINOR=6
db.h_4_6: make_db_h_4_6
	./make_db_h_4_6 > $@
sample_offsets_4_6: sample_offsets.c
	$(CC) $(CFLAGS) -I. -I$(BDBDIR)/db-4.6.19/build_unix $< -o $@
	./sample_offsets_4_6

sample_offsets:
sample_offsets_local: ./db.h
sample_offsets_local: sample_offsets.c
	$(CC) $(CFLAGS) -I. -DLOCAL $< -o $@
 
hfiles: db.h_4_1 db.h_4_3 db.h_4_4 db.h_4_5 db.h_4_6

check32: sample_offsets_local
	./sample_offsets_local
	diff sample_offsets_local.h sample_offsets_32.h

clean:
	rm -f sample_offsets_mysql sample_offsets sample_offsets_?_? *.o make_db_h_?_?
install: db.h_$(BDB)
	cp db.h_$(BDB) ../include/db.h

# Note: To make the compile work, you must cd to the ../../berkeleydb/db-4.1.25/build_unix
# and do ../dist/configure

# to build mysql-5.0.27 with berkeley db 4.1.25
# ./configure --with-berkeley-db=../../berkeleydb/db-4.1.25/build_unix
