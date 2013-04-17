TARGETS = db-insert db-insert-bdb db-scan db-scan-bdb db-update db-update-bdb db-insert-multiple
CPPFLAGS = -I../include -D_GNU_SOURCE
CFLAGS = -g -std=c99 -Wall -Werror
LIBTOKUDB = tokudb
LDFLAGS = -L../lib -l$(LIBTOKUDB) -Wl,-rpath,../lib

default: $(TARGETS)

db-insert-bdb: db-insert.c
	cc -D_GNU_SOURCE -DBDB $(CFLAGS) db-insert.c  -o db-insert-bdb -ldb

db-scan-bdb: db-scan.c
	cc -D_GNU_SOURCE -DBDB $(CFLAGS) db-scan.c    -o db-scan-bdb -ldb

db-update-bdb: db-update.c
	cc -D_GNU_SOURCE -DBDB $(CFLAGS) db-update.c  -o db-update-bdb -ldb


default: db-insert

check: $(TARGETS)
	./db-insert -x && ./db-scan --lwc --prelock --prelockflag

checknox: $(TARGETS)
	./db-insert && ./db-scan --nox --lwc --prelock --prelockflag

clean:
	rm -rf $(TARGETS) bench.*
