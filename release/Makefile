TOKUFRACTALTREE_LIBNAME = tokudb

setup:
	cp ../include/db.h include/tokudb.h
	cp ../include/db.h include
	cp ../include/tdb-internal.h include
	cp ../toku_include/toku_list.h include
	cp ../lib/lib$(TOKUFRACTALTREE_LIBNAME).so lib
	cp ../toku_include/toku_os.h include
	cp ../linux/toku_os_types.h include
	cp ../lib/libtokuportability.a lib
	if [ -f /usr/local/cilk/lib64/libcilkrts.so.1 ] ; then \
	cp /usr/local/cilk/lib64/libcilkrts.so.1 lib; \
	ln -s libcilkrts.so.1 lib/libcilkrts.so; \
	fi

clean:
	rm -f include/* lib/*
