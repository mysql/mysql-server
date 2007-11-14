# On OSX do:
#   make OSX=OSX
#Note: On OSX, ld does not support -rpath, so /usr/lib/libdb.dylib must be the tokudb library.

ifeq ($(OSX),OSX)
LIBEXT=dylib
TEST=
LOADLIBES = -L../ -ldb
else
LIBEXT=so
LOADLIBES = -L../ -ldb -Wl,-rpath,..
TEST=valgrind --quiet --error-exitcode=1
endif

LIBNAME=libdb.$(LIBEXT)
CFLAGS = -Wall -Werror -O0 -g
CPPFLAGS = -I../../include

SRCS = $(wildcard *.c)
TESTS = $(patsubst %.c,%,$(SRCS))

all: make_libs $(TESTS)

test_log1.bdb: test_log1

check_log0: ./test_log0
	$(TEST) ./test_log0
	test -f dir.test_log0/log000000000000.tokulog
check_log1: ./test_log1
	$(TEST) ./test_log1
check_db_close_no_open: ./test_db_close_no_open
	$(TEST) ./test_db_close_no_open
check_db_version: ./test_db_version
	$(TEST) ./test_db_version
check_db_open_notexist_reopen : ./test_db_open_notexist_reopen
	$(TEST) ./test_db_open_notexist_reopen

.PHONY: check_log0 make_libs
make_libs:
	cd ..;make
check: $(TESTS) check_log0
	for testcase in $(TESTS) ; do echo $(TEST) ./$$testcase ; if ! $(TEST) ./$$testcase; then echo "Test Failed!" ; exit 1; fi; done

test_log1.bdb: test_log1.c
	cc -Wall -Werror -O2 -g test_log1.c -o $@ -ldb -DDBVERSION=\"bdb\"

test_log1.bdb_link: test_log1.c
	cc -Wall -Werror -O2 -g test_log1.c -o $@ -ldb $(CPPFLAGS)

test_log1.tokudb_link: test_log1.c
	cc -Wall -Werror -O2 -g test_log1.c -o $@ $(LOADLIBES)

clean:
	rm -f $(TESTS)
