default: db-insert db-insert-bdb
CPPFLAGS = -I../include
LDFLAGS = -L../.. -lfractaltreeindex -Wl,-rpath,../..
db-insert-bdb: db-insert.c
	cc  -ldb  db-insert.c  -o db-insert-bdb -DBDB
default: db-insert

clean:
	rm -rf db-insert bench.$(DIRSUF)
