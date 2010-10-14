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

## files to be distributed and not covered in the automatic rules
EXTRA_DIST = mysql_utils_unit_tests-t

## automake flags (overridden by per-target flags, added before user flags)
AM_CPPFLAGS = -I$(top_srcdir)/storage/ndb/include -I.. \
		-I$(top_srcdir)/storage/ndb/src/ndbjtie/utils


## create the unit test scripts during build already
all-local:	scripts

## build a noninstalled test program
noinst_PROGRAMS = mysql_utils_test
#check_PROGRAMS = mysql_utils_test

## source files to build the program (added to the distribution by default)
mysql_utils_test_SOURCES = mysql_utils_test.cpp

## per-target compilation flags (overrides the AM_ but not the user flags)
#mysqlutils_test_CPPFLAGS = $(AM_CPPFLAGS)

## extra libtool objects (.lo) or libtool libraries (.la)
mysql_utils_test_LDADD = ../libmysqlutils.la ../../../libndbclient.la

## mode-specific libtool options (overrides AM_LDFLAGS)
##   using -no-install requires shared libraries to be installed
mysql_utils_test_LDFLAGS = -no-undefined ## build self-contained executable
#mysql_utils_test_LDFLAGS += -no-install ## never needs wrapper script
#mysql_utils_test_LDFLAGS += -static ## only create a static executable

## ----------------------------------------------------------------------

## programs or scripts to run for testing
TESTS = $(UNIT_TESTS)
TESTS_ENVIRONMENT = $(SHELL)
#TESTS_ENVIRONMENT = $(SHELL) -x

UNIT_TESTS = test_mysql_utils.sh

scripts:	$(UNIT_TESTS)

test_mysql_utils.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_mysql_utils.log\"" >> $@
	@ echo "test -f test_mysql_utils_test.log && rm test_mysql_utils.log" >> $@
	@ echo "./mysql_utils_test > test_mysql_utils.log 2>&1" >> $@
	@CHMOD@ +x $@

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = *.log
CLEANFILES = $(TESTS)

## ----------------------------------------------------------------------
