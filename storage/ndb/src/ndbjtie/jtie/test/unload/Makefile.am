# Copyright (c) 2010, 2012 Oracle and/or its affiliates. All rights reserved.
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
include $(top_srcdir)/storage/ndb/config/java_support.mk.am

## files to be distributed and not covered in the automatic rules
EXTRA_DIST =	$(TEST_DIR)/*.java \
		test_unload.sh.in \
		test_unload.cmd.in \
		CMakeLists.txt

## ----------------------------------------------------------------------

## list of jar files to install
##   cannot use _LIBRARIES primary, which does not recognize .jar files;
##   with _DATA, prefix dist_ is needed to add files to the distribution;
##   with _DATA, prefix noinst_ seems not applicable.
noinst_DATA = $(UNLOAD_TEST_JAR)

## dependencies
CLASSPATH += "."

## jar file to be created
UNLOAD_TEST_JAR = jtie-test-unload-@JAVA_NDB_VERSION@.jar
$(UNLOAD_TEST_JAR):	$(TEST_CLASSES) $(TEST_DIR)
	$(JAR) cvf $@ $(TEST_DIR)

## java top-level class files
TEST_DIR = $(srcdir)/test
TEST_CLASSES = $(TEST_DIR)/MyLoadUnloadTest.class

## ----------------------------------------------------------------------

## programs or scripts to run for testing
TESTS = test_unload.sh
TESTS_ENVIRONMENT = $(SHELL)
#TESTS_ENVIRONMENT = $(SHELL) -x

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = $(TEST_DIR)/*.class *.log
CLEANFILES = $(UNLOAD_TEST_JAR)
DISTCLEANFILES = $(TESTS)

## ----------------------------------------------------------------------
