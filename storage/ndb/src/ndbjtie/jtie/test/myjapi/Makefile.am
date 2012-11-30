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
EXTRA_DIST = 	$(MYJAPI_DIR)/*.java  \
		$(TEST_DIR)/*.java \
		test_myjapi.sh.in \
		test_myjapi.cmd.in \
		CMakeLists.txt

## ----------------------------------------------------------------------

## list of jar files to install
##   cannot use _LIBRARIES primary, which does not recognize .jar files;
##   with _DATA, prefix dist_ is needed to add files to the distribution;
##   with _DATA, prefix noinst_ seems not applicable.
noinst_DATA = $(MYJAPI_JAR)

## dependencies
JTIE_JAR = $(top_srcdir)/storage/ndb/src/ndbjtie/jtie/jtie-@JAVA_NDB_VERSION@.jar
CLASSPATH_ENV = CLASSPATH=.:$(JTIE_JAR)

JAVAROOT=$(srcdir)/target/classes
JAVAHROOT=$(srcdir)/target

## jar file to be created
MYJAPI_JAR = jtie-test-myjapi-@JAVA_NDB_VERSION@.jar

## java top-level class files
MYJAPI_DIR = $(srcdir)/myjapi
MYJAPI_java = \
		$(MYJAPI_DIR)/A.java \
		$(MYJAPI_DIR)/B0.java \
		$(MYJAPI_DIR)/B1.java \
		$(MYJAPI_DIR)/CI.java \
		$(MYJAPI_DIR)/D0.java \
		$(MYJAPI_DIR)/D1.java \
		$(MYJAPI_DIR)/D2.java \
		$(MYJAPI_DIR)/E.java \
		$(MYJAPI_DIR)/MyJapi.java \
		$(MYJAPI_DIR)/MyJapiCtypes.java

TEST_DIR = $(srcdir)/test
TEST_java = \
		$(TEST_DIR)/MyJapiTest.java

classmyjapi.stamp: $(MYJAPI_java) $(TEST_java)
	mkdir -p $(JAVAROOT)
	if test -n "$?"; then \
          echo '$(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
          $(CLASSPATH_ENV) $(JAVAC) -target $(JAVAC_TARGET) -source $(JAVAC_TARGET) -d $(JAVAROOT) \
          $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
        fi
	echo timestamp > classmyjapi.stamp

$(MYJAPI_JAR): classmyjapi.stamp
	rm -f $@; @JAR@ cvf $@  \
          -C $(top_srcdir)/storage/ndb/src/ndbjtie/jtie/target/classes com/mysql/jtie \
          -C $(JAVAROOT) myjapi \
          -C $(JAVAROOT) test

## generate JNI header files (for development only)
javah: $(MYJAPI_JAR)
	$(JAVAH) -classpath $(MYJAPI_JAR) \
		$(AM_JAVAHFLAGS) $(JAVAHFLAGS) \
		-d $(JAVAHROOT) \
		myjapi.A \
		myjapi.B0 \
		myjapi.B1 \
		myjapi.CI \
		myjapi.D0 \
		myjapi.D1 \
		myjapi.E \
		myjapi.MyJapi \
		myjapi.MyJapiCtypes

## ----------------------------------------------------------------------

## automake flags (overridden by per-target flags, added before user flags)
AM_CPPFLAGS = \
		-I$(top_srcdir)/storage/ndb/include \
		-I../../../utils \
		-I../.. \
		-I../myapi \
		@JNI_INCLUDE_DIRS@ \
		-DJTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE

## build a shared library for testing
##   ideally, this library is not installed suggesting the use of prefixes
##   check_ or noinst_; however, these prefixes result in no -rpath option
##   is passed, which prevents libtool from creating a shared library; the
##   argument to a manual -rpath option must be an absolute path, so, e.g.
##   $(top_srcdir)/..., which resolves to a relative path, cannot be used;
##   hence, we fall back on using the pkglib_ prefix and install the lib.
pkglib_LTLIBRARIES = libmyjapi.la
#noinst_LTLIBRARIES = libmyjapi.la
#check_LTLIBRARIES = libmyjapi.la

## source files to build the library (added to distribuition by default)
libmyjapi_la_SOURCES = myjapi_lib.cpp

## per-target compilation flags (overrides the AM_ but not the user flags)
#libmyjapi_la_CPPFLAGS = $(AM_CPPFLAGS)

## extra libtool objects (.lo) or libtool libraries (.la)
libmyjapi_la_LIBADD = ../myapi/libmyapi.la

## mode-specific libtool options (overrides AM_LDFLAGS)
libmyjapi_la_LDFLAGS = -no-undefined ## build self-contained library
libmyjapi_la_LDFLAGS += -shared ## only create a shared library
libmyjapi_la_LDFLAGS += @ndb_cxx_runtime_libs@ ## need to link with C++ runtime
## libmyjapi_la_LDFLAGS = -static ## only create a static library
## libmyjapi_la_LDFLAGS = -rpath . ## only absolute run-paths are allowed

## generic libtool options (overrides AM_LIBTOOLFLAGS)
#libmyjapi_la_LIBTOOLFLAGS =

## ----------------------------------------------------------------------

## programs or scripts to run for testing
TESTS = test_myjapi.sh
TESTS_ENVIRONMENT = $(SHELL)
#TESTS_ENVIRONMENT = $(SHELL) -x

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = *.log
CLEANFILES = classmyjapi.stamp $(MYJAPI_JAR)
DISTCLEANFILES = $(TESTS)

mostlyclean-local:
	rm -rf $(top_srcdir)/storage/ndb/src/ndbjtie/jtie/test/myjapi/target

## ----------------------------------------------------------------------
