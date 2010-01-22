# Copyright (C) 2009 Sun Microsystems, Inc.
# All rights reserved. Use is subject to license terms.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

include $(top_srcdir)/storage/ndb/config/common.mk.am

## ----------------------------------------------------------------------

## automake flags (overridden by per-target flags, added before user flags)
AM_CPPFLAGS = -I../../../utils

## ----------------------------------------------------------------------

## create the unit test scripts during build already
all-local:	scripts

## build a libtool convenience (i.e., non-installed) library
##   prefixes check_, noinst_ result in no -rpath option being passed,
##   which prevents libtool from creating a shared library.
noinst_LTLIBRARIES = libmyapi.la
#check_LTLIBRARIES = libmyapi.la

## source files to build the library (added to the distribution by default)
libmyapi_la_SOURCES = myapi.cpp

## per-target compilation flags (overrides the AM_ but not the user flags)
#libmyapi_la_CPPFLAGS = $(AM_CPPFLAGS)

## extra libtool objects (.lo) or libtool libraries (.la)
#libmyapi_la_LIBADD =

## mode-specific libtool options (overrides AM_LDFLAGS)
libmyapi_la_LDFLAGS = -no-undefined ## build self-contained library
#libmyapi_la_LDFLAGS = -shared ## only create a shared library
#libmyapi_la_LDFLAGS = -static ## only create a static library

## generic libtool options (overrides AM_LIBTOOLFLAGS)
#libmyapi_la_LIBTOOLFLAGS =

## ----------------------------------------------------------------------

## build a noninstalled test program
noinst_PROGRAMS = myapi_test
#check_PROGRAMS = myapi_test

## source files to build the program (dist_ by default)
myapi_test_SOURCES = myapi_test.cpp

## per-target compilation flags (overrides the AM_ but not the user flags)
#myapi_test_CPPFLAGS = $(AM_CPPFLAGS)

## extra libtool objects (.lo) or libtool libraries (.la)
myapi_test_LDADD = libmyapi.la

## mode-specific libtool options (overrides AM_LDFLAGS)
##   using -no-install requires shared libraries to be installed
#myapi_test_LDFLAGS = -no-install ## never needs wrapper script
myapi_test_LDFLAGS = -no-undefined ## build self-contained executable
myapi_test_LDFLAGS += @ndb_cxx_runtime_libs@ ## Need to link with C++ runtime
#myapi_test_LDFLAGS += -static ## only create a static executable

## ----------------------------------------------------------------------

## programs or scripts to run for testing
TESTS = $(UNIT_TESTS)
TESTS_ENVIRONMENT = $(SHELL)
#TESTS_ENVIRONMENT = $(SHELL) -x

UNIT_TESTS = test_myapi.sh

scripts:	$(UNIT_TESTS)

test_myapi.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_myapi.log\"" >> $@
	@ echo "test -f test_myapi.log && rm test_myapi.log" >> $@
	@ echo "./myapi_test > test_myapi.log 2>&1" >> $@
	@CHMOD@ +x $@

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = *.log
CLEANFILES = $(TESTS)

## ----------------------------------------------------------------------
