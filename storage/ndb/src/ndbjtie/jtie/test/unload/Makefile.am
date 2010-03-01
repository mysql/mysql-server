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

## files to be distributed and not covered in the automatic rules
EXTRA_DIST = $(TEST_DIR)/*.java CMakeLists.txt

## ----------------------------------------------------------------------

## programs or scripts to run for testing
TESTS = $(UNIT_TESTS)
TESTS_ENVIRONMENT = $(SHELL)
#TESTS_ENVIRONMENT = $(SHELL) -x

UNIT_TESTS = test_unload.sh

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
MYJAPI = ../myjapi
MYJAPI_JAR = "$(MYJAPI)/jtie-test-myjapi-@JAVA_NDB_VERSION@.jar"
MYJAPI_O = "$(MYJAPI)/.libs/myjapi_lib.o"
RUN_CLASSPATH = "$(UNLOAD_TEST_JAR):$(MYJAPI_JAR)"
JAVA_CLASSPATH = -classpath "\"$(RUN_CLASSPATH)\""
JAVA_LIBPATH = -Djava.library.path="\"$(MYJAPI)/.libs\""

## myjapi unload test
PROPERTIES = \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_package_prefixes="test.,myjapi." \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_class_name="test.MyJapiTest" \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_method_name="test"

test_unload.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_unload.log\"" >> $@
	@ echo "test -f test_unload.log && rm test_unload.log" >> $@
	if file $(MYJAPI_O) | grep 64 > /dev/null 2>&1; then echo "$(JAVA_DEFAULT64) \\" >> $@; else echo "$(JAVA_DEFAULT32) \\" >> $@; fi
	@ echo "  $(JAVAFLAGS_DEFAULT) \\" >> $@
	@ echo "  $(JAVA_LIBPATH) $(JAVA_CLASSPATH) \\" >> $@
	@ echo "  $(PROPERTIES) \\" >> $@
	@ echo "  test.MyLoadUnloadTest > test_unload.log 2>&1" >> $@
	@CHMOD@ +x $@

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = $(TEST_DIR)/*.class *.log
CLEANFILES = $(UNLOAD_TEST_JAR) $(TESTS)

## ----------------------------------------------------------------------
