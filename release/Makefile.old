TOKUROOT=../
include $(TOKUROOT)toku_include/Makefile.include

local setup:
	cp ../include/db.h include/tokudb.h
	cp ../include/db.h include
	cp ../include/tdb-internal.h include
	cp ../toku_include/toku_list.h include
	cp ../toku_include/toku_os.h include
	cp ../linux/toku_os_types.h include
	cp ../linux/toku_time.h include
	cp ../lib/lib$(LIBTOKUDB).so lib
	cp ../lib/lib$(LIBTOKUPORTABILITY).* lib
	cp ../newbrt/brtdump_static bin

clean:
	rm -f include/* lib/* bin/*
