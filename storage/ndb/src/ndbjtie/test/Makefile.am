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

## ----------------------------------------------------------------------

## files to be distributed and not covered in the automatic rules
EXTRA_DIST = 	$(TEST_DIR)/*.java \
		test_mutils.sh.in \
		test_mutils.cmd.in \
		test_ndbjtie_constants.sh.in \
		test_ndbjtie_constants.cmd.in \
		test_ndbjtie_multilib.sh.in \
		test_ndbjtie_multilib.cmd.in \
		test_ndbjtie_smoke.sh.in \
		test_ndbjtie_smoke.cmd.in \
		test_unload_mutils.sh.in \
		test_unload_mutils.cmd.in \
		test_unload_ndbjtie_constants.sh.in \
		test_unload_ndbjtie_constants.cmd.in \
		test_unload_ndbjtie_multilib.sh.in \
		test_unload_ndbjtie_multilib.cmd.in \
		test_unload_ndbjtie_smoke.sh.in \
		test_unload_ndbjtie_smoke.cmd.in \
		CMakeLists.txt

## ----------------------------------------------------------------------

## build noninstalled test program
noinst_PROGRAMS = ndbjtie_unit_tests-t

## source files to build the program (dist_ by default)
ndbjtie_unit_tests_t_SOURCES = ndbjtie_unit_tests-t.cpp

## per-target compilation flags (overrides the AM_ but not the user flags)
ndbjtie_unit_tests_t_CPPFLAGS = -I$(top_srcdir)/storage/ndb/include

## mode-specific libtool options (overrides AM_LDFLAGS)
ndbjtie_unit_tests_t_LDFLAGS = -no-undefined ## build self-contained executable

## ----------------------------------------------------------------------

## build shared library for testing
##   ideally, this library is not installed suggesting the use of prefixes
##   check_ or noinst_; however, these prefixes result in no -rpath option
##   is passed, which prevents libtool from creating a shared library; the
##   argument to a manual -rpath option must be an absolute path, so, e.g.
##   $(top_srcdir)/..., which resolves to a relative path, cannot be used;
##   hence, we fall back on using the pkglib_ prefix and install the lib.
pkglib_LTLIBRARIES = libndbjtie_unit_tests.la
#noinst_LTLIBRARIES = libndbjtie_unit_tests.la
#check_LTLIBRARIES = libndbjtie_unit_tests.la

## source files to build the library (added to distribuition by default)
libndbjtie_unit_tests_la_SOURCES = \
		ndbjtie_unit_tests_consts.cpp \
		ndbjtie_unit_tests_lib.cpp

## per-target compilation flags (overrides the AM_ but not the user flags)
libndbjtie_unit_tests_la_CPPFLAGS = \
		-I../utils \
		-I../jtie \
		-I$(top_srcdir)/storage/ndb/include \
		-I$(top_srcdir)/storage/ndb/include/mgmapi \
		-I$(top_srcdir)/storage/ndb/include/ndbapi \
		@JNI_INCLUDE_DIRS@ \
		-DJTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE

## extra libtool objects (.lo) or libtool libraries (.la)
#libndbjtie_unit_tests_la_LIBADD =

## mode-specific libtool options (overrides AM_LDFLAGS)
libndbjtie_unit_tests_la_LDFLAGS = -no-undefined ## build self-contained library
libndbjtie_unit_tests_la_LDFLAGS += -shared ## only create a shared library
libndbjtie_unit_tests_la_LDFLAGS += @ndb_cxx_runtime_libs@ ## need to link with C++ runtime
## libndbjtie_unit_tests_la_LDFLAGS = -static ## only create a static library
## libndbjtie_unit_tests_la_LDFLAGS = -rpath . ## only absolute run-paths are allowed

## generic libtool options (overrides AM_LIBTOOLFLAGS)
#libndbjtie_unit_tests_la_LIBTOOLFLAGS =

## ----------------------------------------------------------------------

## list of jar files to build
noinst_DATA = $(NDBJTIE_TEST_JAR)

## dependencies
NDBJTIE = ".."
NDBJTIE_JAR = "$(NDBJTIE)/ndbjtie-@JAVA_NDB_VERSION@.jar"
JTIE = "../jtie"
JTIE_JAR = "$(JTIE)/jtie-@JAVA_NDB_VERSION@.jar"
MYJAPI = "$(JTIE)/test/myjapi"
MYJAPI_JAR = "$(MYJAPI)/jtie-test-myjapi-@JAVA_NDB_VERSION@.jar"
CLASSPATH_ENV = CLASSPATH=.:$(NDBJTIE_JAR):$(MYJAPI_JAR)

## build directories
JAVAROOT=$(srcdir)/target/classes
JAVAHROOT=$(srcdir)/target

## jar file to be created
NDBJTIE_TEST_JAR = ndbjtie-test-@JAVA_NDB_VERSION@.jar

## java top-level class files
TEST_DIR = $(srcdir)/test
TEST_Java = \
		$(TEST_DIR)/JTieTestBase.java \
		$(TEST_DIR)/MySqlUtilsTest.java \
		$(TEST_DIR)/NdbJTieConstantsTest.java \
		$(TEST_DIR)/NdbJTieMultiLibTest.java \
		$(TEST_DIR)/NdbJTieSmokeTest.java

classndbjtietest.stamp: $(TEST_Java)
	mkdir -p $(JAVAROOT)
	if test -n "$?"; then \
	  echo '$(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
	  $(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) \
	  $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
	fi
	echo timestamp > classndbjtietest.stamp

$(NDBJTIE_TEST_JAR): classndbjtietest.stamp 
	rm -f $@; @JAR@ cvf $@  \
	  -C $(JTIE)/target/classes com/mysql/jtie \
	  -C $(JAVAROOT) test

## generate JNI header files (for development only)
javah: $(NDBJTIE_TEST_JAR)
	$(JAVAH) -classpath $(NDBJTIE_TEST_JAR) \
		$(AM_JAVAHFLAGS) $(JAVAHFLAGS) \
		-d $(JAVAHROOT) \
		test.JTieTestBase \
		test.MySqlUtilsTest \
		test.NdbJTieConstantsTest \
		test.NdbJTieMultiLibTest \
		test.NdbJTieSmokeTest

## ----------------------------------------------------------------------

## programs or scripts to run for testing
# unit-tests invoked individually
#TESTS = $(noinst_PROGRAMS)
# these ndbjtie tests do not require a running cluster
TESTS = test_mutils.sh \
	test_ndbjtie_constants.sh \
	test_ndbjtie_multilib.sh \
	test_ndbjtie_smoke.sh \
	test_unload_mutils.sh \
	test_unload_ndbjtie_constants.sh \
	test_unload_ndbjtie_multilib.sh \
	test_unload_ndbjtie_smoke.sh
TESTS_ENVIRONMENT = $(SHELL)
#TESTS_ENVIRONMENT = $(SHELL) -x

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = $(TEST_DIR)/*.class *.log
CLEANFILES = classndbjtietest.stamp $(NDBJTIE_TEST_JAR)
DISTCLEANFILES = $(TESTS)

mostlyclean-local:
	rm -rf $(top_srcdir)/storage/ndb/src/ndbjtie/test/target

## ----------------------------------------------------------------------
