TARGETS = db-insert db-insert-bdb db-scan db-scan-bdb db-update
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

check: $(TARGETS)
	./db-insert -x && ./db-scan --lwc --prelock --prelockflag

checknox: $(TARGETS)
	./db-insert && ./db-scan --nox --lwc --prelock --prelockflag

clean:
	rm -rf $(TARGETS) bench.*
