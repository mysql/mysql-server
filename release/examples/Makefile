TARGETS = db-insert db-insert-bdb db-scan db-scan-bdb
default: $(TARGETS)
CFLAGS = -g
CPPFLAGS = -I./include
LDFLAGS = -L.. -lfractaltreeindex -Wl,-rpath,..
db-insert-bdb: db-insert.c
	cc -ldb db-insert.c  -o db-insert-bdb -DBDB $(CFLAGS)
db-scan-bdb: db-scan.c
	cc -ldb db-scan.c    -o db-scan-bdb   -DBDB $(CFLAGS)
default: db-insert

clean:
	rm -rf $(TARGETS) bench
