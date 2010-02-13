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
include $(top_srcdir)/storage/ndb/config/java_support.mk.am

## ----------------------------------------------------------------------

## create the unit test scripts during build already
all-local:	scripts

## list of jar files to install
##   cannot use _LIBRARIES primary, which does not recognize .jar files;
##   with _DATA, prefix dist_ is needed to add files to the distribution;
##   with _DATA, prefix noinst_ seems not applicable.
myjapijar_DATA = $(MYJAPI_JAR)

## where to install the jar file(s)
myjapijardir = $(ndbjardir)

## create a versioned link
install-data-hook:
	cd $(DESTDIR)$(myjapijardir) && \
	$(LN_CP_F) -f $(MYJAPI_JAR) jtie-test-myjapi.jar

## dependencies
JTIE_JAR = $(top_srcdir)/storage/ndb/src/ndbjtie/jtie/jtie-@JAVA_NDB_VERSION@.jar
CLASSPATH_ENV = CLASSPATH=.:$(JTIE_JAR)

JAVAROOT=$(srcdir)/target/classes

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
		$(MYJAPI_DIR)/MyJapi.java \
		$(MYJAPI_DIR)/MyJapiCtypes.java

TEST_DIR = $(srcdir)/test
TEST_java = \
		$(TEST_DIR)/MyJapiTest.java

## files to be distributed and not covered in the automatic rules
EXTRA_DIST = $(MYJAPI_DIR)/*.java  \
  $(TEST_DIR)/*.java \
  myjapi_A.hpp \
  myjapi_D.hpp \
  myjapi_classes.hpp \
  myjapi_B.hpp \
  myjapi_MyJapi.hpp \
  myjapi_CI.hpp \
  myjapi_MyJapiCtypes.hpp


classmyjapi.stamp: $(MYJAPI_java) $(TEST_java)
	mkdir -p $(JAVAROOT)
	if test -n "$?"; then \
          echo '$(CLASSPATH_ENV) $(JAVAC) -d $(JAVAROOT) $(AM_JAVACFLAGS) $(JAVACFLAGS) $?' ; \
          $(CLASSPATH_ENV) $(JAVAC) -d $(JAVAROOT) \
          $(AM_JAVACFLAGS) $(JAVACFLAGS) $?; \
        fi
	echo timestamp > classmyjapi.stamp

$(MYJAPI_JAR): classmyjapi.stamp
	rm -f $@; @JAR@ cvf $@  \
          -C $(top_srcdir)/storage/ndb/src/ndbjtie/jtie/target/classes com/mysql/jtie \
          -C $(JAVAROOT) myjapi \
          -C $(JAVAROOT) test
	$(LN_CP_F) -f $(MYJAPI_JAR) jtie-test-myjapi.jar

## generate JNI header files (for development only)
javah: $(MYJAPI_CLASSES)
	$(COMPILE.JAVAH) \
	myjapi.A \
	myjapi.B0 \
	myjapi.B1 \
	myjapi.CI \
	myjapi.D0 \
	myjapi.D1 \
	myjapi.D2 \
	myjapi.MyJapi \
	myjapi.MyJapiCtypes

## ----------------------------------------------------------------------

## automake flags (overridden by per-target flags, added before user flags)
AM_CPPFLAGS = \
	-I../../../utils \
	-I../.. \
	-I../myapi \
	@JNI_INCLUDE_DIRS@ \
	-DJTIE_VERBOSE \
	-DJTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE

## build a shared library for testing
##   ideally, this library is not installed suggesting the use of prefixes
##   check_ or noinst_; however, these prefixes result in no -rpath option
##   is passed, which prevents libtool from creating a shared library; the
##   argument to a manual -rpath option must be an absolute path, so, e.g.
##   $(top_srcdir)/..., which resolves to a relative path, cannot be used;
##   hence, we fall back on using the pkglib_ prefix and install the lib.
#pkglib_LTLIBRARIES = libmyjapi.la
noinst_LTLIBRARIES = libmyjapi.la
#check_LTLIBRARIES = libmyjapi.la

## source files to build the library (added to distribuition by default)
libmyjapi_la_SOURCES = myjapi_lib.cpp

## per-target compilation flags (overrides the AM_ but not the user flags)
#libmyjapi_la_CPPFLAGS = $(AM_CPPFLAGS)

## extra libtool objects (.lo) or libtool libraries (.la)
#libmyjapi_la_LIBADD = ../myapi/libmyapi.la

## mode-specific libtool options (overrides AM_LDFLAGS)
#libmyjapi_la_LDFLAGS = -no-undefined ## build self-contained library
#libmyjapi_la_LDFLAGS += -shared ## only create a shared library
## libmyjapi_la_LDFLAGS = -static ## only create a static library
## libmyjapi_la_LDFLAGS = -rpath . ## only absolute run-paths are allowed

## generic libtool options (overrides AM_LIBTOOLFLAGS)
#libmyjapi_la_LIBTOOLFLAGS =

## ----------------------------------------------------------------------

## programs or scripts to run for testing
TESTS = $(UNIT_TESTS)
TESTS_ENVIRONMENT = $(SHELL)
#TESTS_ENVIRONMENT = $(SHELL) -x

UNIT_TESTS = test_myjapi.sh

scripts:	$(UNIT_TESTS)

# better enable asserts for these tests
#SUN_JAVAFLAGS = -ea -Xcheck:jni -XX:+UseParallelGC -XX:+UseParallelOldGC
#SUN_JAVAFLAGS = -ea -Xcheck:jni -XX:+UseSerialGC
SUN_JAVAFLAGS = -ea -Xcheck:jni
JAVAFLAGS_DEFAULT = $(SUN_JAVAFLAGS)
JAVA_DEFAULT64 = java -d64
JAVA_DEFAULT32 = java -d32

## dependencies
# test uninstalled jars and libraries as a pre-install sanity test
RUN_CLASSPATH = "$(MYJAPI_JAR)"
JAVA_CLASSPATH = -classpath "\"$(RUN_CLASSPATH)\""
JAVA_LIBPATH = -Djava.library.path="\"./.libs\""
#JAVA_LIBPATH = -Djava.library.path="\"$(ndblibdir)\""

test_myjapi.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_myjapi.log\"" >> $@
	@ echo "test -f test_myjapi.log && rm test_myjapi.log" >> $@
	if file .libs/myjapi_lib.o | grep 64 > /dev/null 2>&1; then echo "$(JAVA_DEFAULT64) \\" >> $@; else echo "$(JAVA_DEFAULT32) \\" >> $@; fi
	@ echo "  $(JAVAFLAGS_DEFAULT) \\" >> $@
	@ echo "  $(JAVA_LIBPATH) $(JAVA_CLASSPATH) \\" >> $@
	@ echo "  test.MyJapiTest > test_myjapi.log 2>&1" >> $@
	@CHMOD@ +x $@

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = $(MYJAPI_DIR)/*.class $(TEST_DIR)/*.class *.log
CLEANFILES = classmyjapi.stamp jtie-test-myjapi.jar $(MYJAPI_JAR) $(TESTS)

mostlyclean-local: 
	rm -rf $(top_srcdir)/storage/ndb/src/ndbjtie/jtie/test/myjapi/target


## ----------------------------------------------------------------------
