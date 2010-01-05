TARGETS = db-insert db-insert-bdb db-scan db-scan-bdb
CFLAGS = -g
CPPFLAGS = -I../include
LIBTOKUDB = tokudb
LDFLAGS = -L../lib -l$(LIBTOKUDB) -Wl,-rpath,../lib

default: $(TARGETS)

db-insert-bdb: db-insert.c
	cc -ldb db-insert.c  -o db-insert-bdb -DBDB $(CFLAGS)

db-scan-bdb: db-scan.c
	cc -ldb db-scan.c    -o db-scan-bdb   -DBDB $(CFLAGS)

default: db-insert

setup:
	cp ../../include/db.h ../include/tokudb.h
	cp ../../include/tdb-internal.h ../include/tdb-internal.h
	cp ../../toku_include/toku_list.h ../include/toku_list.h
	if [ ! -f ../lib/lib$(LIBTOKUDB).so ] ; then cp ../../lib/lib$(LIBTOKUDB).so ../lib; fi

check: $(TARGETS)
	./db-insert && ./db-scan --nox --lwc --prelock --prelockflag

clean:
	rm -rf $(TARGETS) bench.*
