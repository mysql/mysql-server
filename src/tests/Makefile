CFLAGS = -Wall -Werror -O2 -g
CPPFLAGS = -I../../include
LOADLIBES = -L../ -ldb -Wl,-rpath,..
test_log1.bdb: test_log1
test_log1 test_log0: ../libdb.so
check_log0: ./test_log0
	valgrind --quiet ./test_log0
	test -f dir.test_log0/log000000000000.tokulog
check_log1: ./test_log1
	valgrind --quiet ./test_log1
check_db_close_no_open: ./test_db_close_no_open
	valgrind --quiet ./test_db_close_no_open
check_db_version: ./test_db_version
	valgrind --quiet ./test_db_version

.PHONY: check_log0 make_libs
make_libs:
	cd ..;make
check: make_libs check_log0 check_log1 check_db_close_no_open check_db_version

test_log1.bdb: test_log1.c
	cc -Wall -Werror -O2 -g test_log1.c -o $@ -ldb -DDBVERSION=\"bdb\"

test_log1.bdb_link: test_log1.c
	cc -Wall -Werror -O2 -g test_log1.c -o $@ -ldb $(CPPFLAGS)

test_log1.tokudb_link: test_log1.c
	cc -Wall -Werror -O2 -g test_log1.c -o $@ $(LOADLIBES)

