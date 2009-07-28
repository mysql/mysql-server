DIRSUF = db
CPPFLAGS = -I../include -DDIRSUF=$(DIRSUF)
LDFLAGS = -L../.. -lfractaltreeindex -Wl,-rpath,../..

default: db-scan

clean:
	rm -rf db-scan bench.$(DIRSUF)

