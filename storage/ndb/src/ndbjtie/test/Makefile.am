# Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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
EXTRA_DIST = 	$(TEST_DIR)/*.java \
		test_mutils.sh.in \
		test_mutils.cmd.in \
		test_ndbjtie_multilib.sh.in \
		test_ndbjtie_multilib.cmd.in \
		test_ndbjtie_smoke.sh.in \
		test_ndbjtie_smoke.cmd.in \
		test_unload_mutils.sh.in \
		test_unload_mutils.cmd.in \
		test_unload_ndbjtie_multilib.sh.in \
		test_unload_ndbjtie_multilib.cmd.in \
		test_unload_ndbjtie_smoke.sh.in \
		test_unload_ndbjtie_smoke.cmd.in \
		CMakeLists.txt

## ----------------------------------------------------------------------

## build a noninstalled test program
noinst_PROGRAMS = ndbjtie_unit_tests-t
ndbjtie_unit_tests_t_SOURCES = ndbjtie_unit_tests-t.cpp
ndbjtie_unit_tests_t_LDFLAGS = -no-undefined ## build self-contained executable
ndbjtie_unit_tests_t_CXXFLAGS = -I$(top_srcdir)/storage/ndb/include

## ----------------------------------------------------------------------

## list of jar files to install
##   cannot use _LIBRARIES primary, which does not recognize .jar files;
##   with _DATA, prefix dist_ is needed to add files to the distribution;
##   with _DATA, prefix noinst_ seems not applicable.
noinst_DATA = $(NDBJTIE_TEST_JAR)

## dependencies
NDBJTIE = ..
NDBJTIE_JAR = "$(NDBJTIE)/ndbjtie-@JAVA_NDB_VERSION@.jar"
MYJAPI = ../jtie/test/myjapi
MYJAPI_JAR = "$(MYJAPI)/jtie-test-myjapi-@JAVA_NDB_VERSION@.jar"
CLASSPATH += .:$(NDBJTIE_JAR):$(MYJAPI_JAR)

## jar file to be created
NDBJTIE_TEST_JAR = ndbjtie-test-@JAVA_NDB_VERSION@.jar
$(NDBJTIE_TEST_JAR):	$(TEST_CLASSES) $(TEST_DIR)
	$(JAR) cvf $@ $(TEST_DIR)

## java top-level class files
TEST_DIR = $(srcdir)/test
TEST_CLASSES = \
		$(TEST_DIR)/JTieTestBase.class \
		$(TEST_DIR)/MySqlUtilsTest.class \
		$(TEST_DIR)/NdbJTieMultiLibTest.class \
		$(TEST_DIR)/NdbJTieSmokeTest.class

## ----------------------------------------------------------------------

## programs or scripts to run for testing
# unit-tests invoked individually
#TESTS = $(noinst_PROGRAMS)
# these ndbjtie tests do not require a running cluster
TESTS = test_mutils.sh \
	test_ndbjtie_multilib.sh \
	test_ndbjtie_smoke.sh \
	test_unload_mutils.sh \
	test_unload_ndbjtie_multilib.sh \
	test_unload_ndbjtie_smoke.sh
TESTS_ENVIRONMENT = $(SHELL)
#TESTS_ENVIRONMENT = $(SHELL) -x

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = $(TEST_DIR)/*.class *.log
CLEANFILES = $(NDBJTIE_TEST_JAR)
DISTCLEANFILES = $(TESTS)

## ----------------------------------------------------------------------
