# standard build: make
# build with Berkeley DB 4.1: make BDB=/usr/local/BerkeleyDB.4.1
# build with TokuDB: make BDB=~/svn/tokudb

BENCHDB = bench.db

CFLAGS = -Wall -Werror -g
# CFLAGS += -pg
CFLAGS += -O2 

ifdef BDB
CPPFLAGS = -I$(BDB)/include
LDFLAGS = -L$(BDB)/lib -ldb -lpthread
else
CPPFLAGS =
LDFLAGS = -ldb
endif

TARGET = db-benchmark-test

$(TARGET):

clean:
	rm -rf $(TARGET) $(BENCHDB)