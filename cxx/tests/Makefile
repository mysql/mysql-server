SRCS = $(wildcard *.cpp)
TARGETS = $(patsubst %.cpp,%,$(SRCS))


# OPTFLAGS = -O0
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CPPFLAGS = -I../ -I../../include
CXXFLAGS = -Wall $(OPTFLAGS) -g $(GCOV_FLAGS)
LDLIBS = ../../lib/libtokudb_cxx.a ../../lib/libtokudb.a -lz

ifneq ($(OSX),)
 VGRIND=
else
 VGRIND=valgrind --quiet --error-exitcode=1 --leak-check=yes
endif

default: build
build all: $(TARGETS)
$(TARGETS): $(DBCXX)

$(DBCXX):
	cd ..;make

clean:
	rm -rf $(TARGETS) *.gcno *.gcda *.gcov *.db

check_test1: test1
	$(VGRIND) ./$<
check_test_errpfx: test_errpfx
	$(VGRIND) ./$< > errpfxactual.out
	(echo "Prefix: Hello Name!";echo -n ": Success") > errpfxexpect.out
	diff errpfxactual.out errpfxexpect.out

check_test_db_assoc3: test_db_assoc3
	$(VGRIND) ./test_db_assoc3
	$(VGRIND) ./test_db_assoc3 --more

check_test_cursor_count: test_cursor_count
	$(VGRIND) ./test_cursor_count

check_test_error_stream: test_error_stream
	$(VGRIND) ./test_error_stream

check_test1e: test1e
	$(VGRIND) ./test1e > test1eactual.out
	(echo "Prefix: Hello Name!";echo -n ": Success") > test1expect.out
	diff test1eactual.out test1expect.out

check_create_dump_diff: db_create db_dump db_dump_e
	rm -f cdd.tdb
	$(VGRIND) ./db_create cdd.tdb a b c d
	$(VGRIND) ./db_dump cdd.tdb > cdd.out
	(echo " 61";echo " 62";echo " 63";echo " 64")>cddexpect.out
	diff cdd.out cddexpect.out
	$(VGRIND) ./db_dump_e cdd.tdb > cdd.out
	diff cdd.out cddexpect.out

check_test_reverse_compare_fun: test_reverse_compare_fun
	$(VGRIND) ./test_reverse_compare_fun

check: $(TARGETS) \
 check_create_dump_diff \
 check_test1 \
 check_test_errpfx \
 check_test_db_assoc3 \
 check_test_cursor_count \
 check_test_error_stream \
 check_test_reverse_compare_fun \
 check_test1e \
 check_db_create \
 check_db_create_DSM \
 check_db_create_1 check_db_create_2 check_db_create_3 check_db_create_4 \
 check_permissions \
 check_a_bunch

check_a_bunch:
	$(VGRIND) ./exceptions
	$(VGRIND) ./test_no_env
	$(VGRIND) ./test_fd
	$(VGRIND) ./test_db_delete
	$(VGRIND) ./test_get_not_found

check_db_create: db_create
	rm -f $@.tdb; $(VGRIND) ./db_create -s main $@.tdb
check_db_create_DSM: db_create
	rm -f $@.tdb
	$(VGRIND) ./db_create -D -S -s main $@.tdb

check_db_create_1:
	$(VGRIND) ./db_create; let exitcode=$$?; if [ $$exitcode -ne 0 ] ; then exit 0; else exit 1; fi
check_db_create_2:
	$(VGRIND) ./db_create -h; let exitcode=$$?; if [ $$exitcode -ne 0 ] ; then exit 0; else exit 1; fi
check_db_create_3:
	$(VGRIND) ./db_create --help; let exitcode=$$?; if [ $$exitcode -ne 0 ] ; then exit 0; else exit 1; fi
check_db_create_4:
	$(VGRIND) ./db_create -s; let exitcode=$$?; if [ $$exitcode -ne 0 ] ; then exit 0; else exit 1; fi

check_permissions:
	rm -f permissions.tdb
	./db_create permissions.tdb 1 1
	chmod -w permissions.tdb
	./db_create permissions.tdb 2 2; let exitcode=$$?; if [ $$exitcode -ne 0 ] ; then exit 0; else exit 1; fi
