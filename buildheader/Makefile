BDB = 4_6
BDBDIR = ../../../berkeleydb
MYSQL_H = -I$(BDBDIR)/db-4.1.25/build_unix/
OPTFLAGS = -O2
ifeq ($(GCCVERSION),4.4.2)
 GCC_VERSION_SPECIFIC_WARNINGS = -Wno-strict-aliasing -Wno-deprecated 
else ifeq ($(GCCVERSION),4.4.1)
 GCC_VERSION_SPECIFIC_WARNINGS = -Wno-strict-aliasing -Wno-deprecated 
endif
CFLAGS = $(GCC_VERSION_SPECIFIC_WARNINGS) -Wall -W -Werror -g $(OPTFLAGS)
CPPFLAGS = -I. -I../toku_include

ifeq ($(CC),icc)
 CFLAGS += -diag-disable 188
endif

default: ../include/db.h

../include/db.h: make_tdb
	./make_tdb > $@

hfiles: tdb.h

clean:
	rm -f *.o make_tdb make_db_h_?_? sample_offsets sample_offsets_mysql make_tdb_h
