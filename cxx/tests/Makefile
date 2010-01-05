# -*- Mode: Makefile -*-
# standard build: make
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

.DEFAULT_GOAL= default
TOKUROOT=../../
INCLUDEDIRS=-I.
DEPEND_COMPILE += \
	../*.h \
	./*.h \
#end

HERE = cxx/tests
include $(TOKUROOT)toku_include/Makefile.include

SHELL=/bin/bash #Use of &> is a bash feature

SHOULD_FAIL = 
$(SHOULD_FAIL): MAYBEINVERTER=;test $$? -ne 0

HERE = cxx/tests
ifeq ($(SUMMARIZE),1)
SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sPASS\n" $(HERE)/$@; else printf "%-60sFAIL\n" $(HERE)/$@ ; test 0 = 1; fi
$(SHOULD_FAIL): SUMMARIZE_CMD = ;if test $$? = 0; then printf "%-60sXFAIL\n" $(HERE)/$@; else printf "%-60sXPASS\n" $(HERE)/$@ ; test 0 = 1; fi
else
SUMMARIZE_CMD =
endif


SRCS = $(wildcard *.cpp)
TARGETS = $(patsubst %.cpp,%,$(SRCS))


# OPTFLAGS = -O0
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CPPFLAGS = -I../ -I../../include -I../../linux -I$(TOKUROOT)toku_include -DUSE_ENV
CXXFLAGS = $(GCC_VERSION_SPECIFIC) -Werror -Wall $(OPTFLAGS) -g $(GCOV_FLAGS)
LDLIBS = ../../lib/libtokudb_cxx.a -ltokudb -lz -lpthread
RPATH_DIRS=$(TOKUROOT)lib

ifeq ($(CC),icc)
CXXFLAGS += -diag-disable 981
endif

ifneq ($(OSX),)
 VGRIND=
else
 VGRIND=valgrind --quiet --error-exitcode=1 --leak-check=yes --suppressions=../../newbrt/valgrind.suppressions
endif

default: build
build all: $(TARGETS)
$(TARGETS): $(DBCXX)

$(DBCXX):
	cd ..;make

clean:
	rm -rf $(TARGETS) *.gcno *.gcda *.gcov *.db dir.test.db.assoc3 test_reverse_compare_fun.cpp.dir *.tdb.clean *.tdb.dirty
	rm -rf \
		cdd_tdb_0_b.tokudb                                                 \
		check_db_create_DSM_tdb_main_0_b.tokudb                            \
		tokudb.environment                                                 \
		test_error_stream_cpp_tdb_0_b.tokudb                               \
		test_reverse_compare_fun_cpp_dir_reverse_compare_db_main_0_b.tokudb\
		permissions_tdb_0_b.tokudb                                         \
		test_cursor_count_cpp_tdb_0_b.tokudb                               \
		check_db_create_tdb_main_0_b.tokudb                                \
		tokudb.directory                                                   \
#this line left blank


check_test1: test1
	$(VGRIND) ./$< $(SUMMARIZE_CMD)
check_test_errpfx: test_errpfx
	$(VGRIND) ./$< > errpfxactual.out && \
	 (echo "Prefix: Hello Name!";echo -n ": Success") > errpfxexpect.out && \
	 diff errpfxactual.out errpfxexpect.out \
	 $(SUMMARIZE_CMD)

check_test_db_assoc3: test_db_assoc3
	$(VGRIND) ./test_db_assoc3 && \
	 $(VGRIND) ./test_db_assoc3 --more \
	 $(SUMMARIZE_CMD)

check_test_cursor_count: test_cursor_count
	$(VGRIND) ./test_cursor_count $(MAYBEINVERTER) $(SUMMARIZE_CMD)

check_test_error_stream: test_error_stream
	$(VGRIND) ./test_error_stream 2> $@.out $(SUMMARIZE_CMD)

check_test1e: test1e
	$(VGRIND) ./test1e > test1eactual.out && \
	 (echo "Prefix: Hello Name!";echo -n ": Success") > test1expect.out && \
	 diff test1eactual.out test1expect.out \
	  $(SUMMARIZE_CMD)

check_create_dump_diff: db_create db_dump db_dump_e
	rm -f cdd.tdb cdd.tdb.dirty cdd.tdb.clean
	$(VGRIND) ./db_create cdd.tdb a b c d && \
	 $(VGRIND) ./db_dump cdd.tdb > cdd.out && \
	 (echo " 61";echo " 62";echo " 63";echo " 64")>cddexpect.out && \
	 diff cdd.out cddexpect.out && \
	 $(VGRIND) ./db_dump_e cdd.tdb > cdd.out && \
	 diff cdd.out cddexpect.out \
	 $(SUMMARIZE_CMD)

check_test_reverse_compare_fun: test_reverse_compare_fun
	$(VGRIND) ./test_reverse_compare_fun $(SUMMARIZE_CMD)

check: $(TARGETS) \
 check_create_dump_diff \
 check_test1 \
 check_test_errpfx \
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
	$(VGRIND) ./exceptions  $(SUMMARIZE_CMD)
	$(VGRIND) ./test_no_env  $(SUMMARIZE_CMD)
	$(VGRIND) ./test_fd  $(SUMMARIZE_CMD)
	$(VGRIND) ./test_db_delete  $(SUMMARIZE_CMD)
	$(VGRIND) ./test_get_not_found  $(SUMMARIZE_CMD)

check_db_create: db_create
	rm -rf $@.tdb
	$(VGRIND) ./db_create -s main $@.tdb  $(SUMMARIZE_CMD)
check_db_create_DSM: db_create
	rm -rf $@.tdb
	$(VGRIND) ./db_create -D -S -s main $@.tdb  $(SUMMARIZE_CMD)

check_db_create_1: db_create
	$(VGRIND) ./db_create &> $@.out ; test $$? -ne 0  $(SUMMARIZE_CMD)
check_db_create_2: db_create
	$(VGRIND) ./db_create -h &> $@.out ; test $$? -ne 0 $(SUMMARIZE_CMD)
check_db_create_3: db_create
	$(VGRIND) ./db_create --help &> $@.out; test $$? -ne 0 $(SUMMARIZE_CMD)
check_db_create_4: db_create
	$(VGRIND) ./db_create -s &> $@.out ; test $$? -ne 0 $(SUMMARIZE_CMD)

check_permissions:
	rm -f permissions.tdb 
	./db_create permissions.tdb 1 1 && \
	 chmod -w permissions.tdb && \
	 (./db_create permissions.tdb 2 2 &> check_permissions.out; test $$? -ne 0) \
	 $(SUMMARIZE_CMD)
