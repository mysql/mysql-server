DIRSUF = db
CPPFLAGS = -I../include -DDIRSUF=$(DIRSUF)
LDFLAGS = -L../.. -lfractaltreeindex -Wl,-rpath,../..

default: db-insert

clean:
	rm -rf db-insert bench.$(DIRSUF)

