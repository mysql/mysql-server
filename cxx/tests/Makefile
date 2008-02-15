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

all: $(TARGETS)
$(TARGETS): $(DBCXX)

$(DBCXX):
	cd ..;make

clean:
	rm -rf $(TARGETS) *.gcno *.gcda *.gcov *.db

check: $(TARGETS) \
	check_db_create_1 check_db_create_2 check_db_create_3 check_db_create_4 check_permissions
	$(VGRIND) ./test1
	$(VGRIND) ./test1e
	rm -f foo.db
	$(VGRIND) ./db_create -s main foo.db
	rm -f foo.db
	$(VGRIND) ./db_create -D -S -s main foo.db
	rm -f foo.db
	$(VGRIND) ./db_create foo.db a b c d
	$(VGRIND) ./db_dump foo.db > foo.out
	(echo " 61";echo " 62";echo " 63";echo " 64")>foo.expectout
	diff foo.out foo.expectout
	$(VGRIND) ./db_dump_e foo.db > foo.out
	diff foo.out foo.expectout
	$(VGRIND) ./exceptions
	$(VGRIND) ./test_no_env
	$(VGRIND) ./test_db_assoc3
	$(VGRIND) ./test_db_assoc3 --more
	$(VGRIND) ./test_cursor_count
	$(VGRIND) ./test_fd
	$(VGRIND) ./test_error_stream
	$(VGRIND) ./test_reverse_compare_fun
	$(VGRIND) ./test_db_delete
	$(VGRIND) ./test_get_not_found

check_db_create_1:
	$(VGRIND) ./db_create; let exitcode=$$?; if [ $$exitcode -ne 0 ] ; then exit 0; else exit 1; fi
check_db_create_2:
	$(VGRIND) ./db_create -h; let exitcode=$$?; if [ $$exitcode -ne 0 ] ; then exit 0; else exit 1; fi
check_db_create_3:
	$(VGRIND) ./db_create --help; let exitcode=$$?; if [ $$exitcode -ne 0 ] ; then exit 0; else exit 1; fi
check_db_create_4:
	$(VGRIND) ./db_create -s; let exitcode=$$?; if [ $$exitcode -ne 0 ] ; then exit 0; else exit 1; fi

check_permissions:
	rm -f test.db
	./db_create test.db 1 1
	chmod -w test.db
	./db_create test.db 2 2; let exitcode=$$?; if [ $$exitcode -ne 0 ] ; then exit 0; else exit 1; fi