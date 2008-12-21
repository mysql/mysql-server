# -*- Mode: Makefile -*-
# standard build: make
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

.DEFAULT_GOAL= default
TOKUROOT=../
INCLUDEDIRS=-I.
DEPEND_COMPILE += \
	./*.h \
#end

HERE = cxx
include $(TOKUROOT)include/Makefile.include
#
#
# OPTFLAGS = -O2
# GCOV_FLAGS = -fprofile-arcs -ftest-coverage
CPPFLAGS = -I../include
CXXFLAGS = -Wall -g $(OPTFLAGS) $(GCOV_FLAGS)
CC = c++
LDFLAGS = -lz
SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp, %.o, $(SRCS))

ifeq ($(CC),icc)
CXXFLAGS += -diag-disable 981
endif

LIBNAME = libtokudb_cxx

default: install build
build: $(LIBNAME).a
	if ! diff $(LIBNAME).a ../lib/$(LIBNAME).a >/dev/null 2>&1; then cp $< ../lib/; fi
	cd tests; $(MAKE) build
check:
	cd tests; $(MAKE) check
install: $(LIBNAME).a
	cp $< ../lib/
$(OBJS): ../include/db_cxx.h

test1: test1.o dbt.o db.o dbenv.o ../lib/libdb.a

$(LIBNAME).a: $(OBJS)
	$(AR) cr $@ $(OBJS)
clean:
	rm -f $(OBJS) $(LIBNAME).a $(LIBNAME).so *.gcno *.gcda *.gcov
	cd tests && $(MAKE) clean


