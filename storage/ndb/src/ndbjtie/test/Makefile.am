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

## files to be distributed and not covered in the automatic rules
EXTRA_DIST = $(TEST_DIR)/*.java \
	CMakeLists.txt \
	ndbjtie_unit_tests-t

NDBJTIE = ../

## list of jar files to install
##   cannot use _LIBRARIES primary, which does not recognize .jar files;
##   with _DATA, prefix dist_ is needed to add files to the distribution;
##   with _DATA, prefix noinst_ seems not applicable.
noinst_DATA = $(NDBJTIE_TEST_JAR)

## create the unit test scripts during build already
all-local:	scripts

## dependencies
NDBJTIE_JAR = ../ndbjtie-@JAVA_NDB_VERSION@.jar
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
		$(TEST_DIR)/MySqlUtilsDecimalTest.class \
		$(TEST_DIR)/MySqlUtilsCharsetMapTest.class \
		$(TEST_DIR)/NdbJTieLibraryLoadingTest.class \
		$(TEST_DIR)/NdbJTieSmokeTest.class

## ----------------------------------------------------------------------

## programs or scripts to run for testing
# the ndbjtie smoke test requires a running cluster (for now)
#TESTS = test_ndbjtie_smoke.sh test_unload_ndbjtie_smoke.sh
# the mysql utils tests are just a library test
TESTS = \
	test_mutils_decimal.sh \
	test_mutils_charsetmap.sh \
	test_ndbjtie_library.sh \
	test_unload_mutils_decimal.sh \
	test_unload_mutils_charsetmap.sh \
	test_unload_ndbjtie_library.sh
TESTS_ENVIRONMENT = $(SHELL)
#TESTS_ENVIRONMENT = $(SHELL) -x

UNIT_TESTS = \
	test_mutils_decimal.sh \
	test_mutils_charsetmap.sh \
	test_ndbjtie_library.sh \
	test_ndbjtie_smoke.sh \
	test_unload_mutils_decimal.sh \
	test_unload_mutils_charsetmap.sh \
	test_unload_ndbjtie_library.sh \
	test_unload_ndbjtie_smoke.sh

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
NDBJTIE_O = "$(NDBJTIE)/.libs/ndbjtie_lib.o"
RUN_CLASSPATH0 = "$(NDBJTIE_TEST_JAR):$(NDBJTIE_JAR)"
JAVA_CLASSPATH0 = -classpath "\"$(RUN_CLASSPATH0)\""
JAVA_LIBPATH0 = -Djava.library.path="\"../../.libs\""
#JAVA_LIBPATH0 = -Djava.library.path="\"$(ndblibdir)\""

## dependencies
# test uninstalled jars and libraries as a pre-install sanity test
UNLOAD = ../jtie/test/unload
UNLOAD_JAR = $(UNLOAD)/jtie-test-unload-@JAVA_NDB_VERSION@.jar
RUN_CLASSPATH1 = "$(UNLOAD_JAR):$(RUN_CLASSPATH0)"
JAVA_CLASSPATH1 = -classpath "\"$(RUN_CLASSPATH1)\""
JAVA_LIBPATH1 = $(JAVA_LIBPATH0)

## dependencies
# test uninstalled jars and libraries as a pre-install sanity test
RUN_CLASSPATH2 = "$(NDBJTIE_TEST_JAR):$(MYJAPI_JAR):$(NDBJTIE_JAR)"
JAVA_CLASSPATH2 = -classpath "\"$(RUN_CLASSPATH2)\""
JAVA_LIBPATH2 = -Djava.library.path="\"$(MYJAPI)/.libs:../../.libs\""

## dependencies
# test uninstalled jars and libraries as a pre-install sanity test
RUN_CLASSPATH3 = "$(UNLOAD_JAR):$(RUN_CLASSPATH2)"
JAVA_CLASSPATH3 = -classpath "\"$(RUN_CLASSPATH3)\""
JAVA_LIBPATH3 = $(JAVA_LIBPATH2)

## mysql-utils decimal test
test_mutils_decimal.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_mutils_decimal.log\"" >> $@
	@ echo "test -f test_mutils_decimal.log && rm test_mutils_decimal.log" >> $@
	if file $(NDBJTIE_O) | grep 64 > /dev/null 2>&1; then echo "$(JAVA_DEFAULT64) \\" >> $@; else echo "$(JAVA_DEFAULT32) \\" >> $@; fi
	@ echo "  $(JAVAFLAGS_DEFAULT) \\" >> $@
	@ echo "  $(JAVA_LIBPATH0) $(JAVA_CLASSPATH0) \\" >> $@
	@ echo "  test.MySqlUtilsDecimalTest > test_mutils_decimal.log 2>&1" >> $@
	@CHMOD@ +x $@

## mysql-utils charsetmap test
test_mutils_charsetmap.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_mutils_charsetmap.log\"" >> $@
	@ echo "test -f test_mutils_charsetmap.log && rm test_mutils_charsetmap.log" >> $@
	if file $(NDBJTIE_O) | grep 64 > /dev/null 2>&1; then echo "$(JAVA_DEFAULT64) \\" >> $@; else echo "$(JAVA_DEFAULT32) \\" >> $@; fi
	@ echo "  $(JAVAFLAGS_DEFAULT) \\" >> $@
	@ echo "  $(JAVA_LIBPATH0) $(JAVA_CLASSPATH0) \\" >> $@
	@ echo "  test.MySqlUtilsCharsetMapTest > test_mutils_charsetmap.log 2>&1" >> $@
	@CHMOD@ +x $@

## ndbjtie library test
test_ndbjtie_library.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_ndbjtie_library.log\"" >> $@
	@ echo "test -f test_ndbjtie_library.log && rm test_ndbjtie_library.log" >> $@
	if file $(NDBJTIE_O) | grep 64 > /dev/null 2>&1; then echo "$(JAVA_DEFAULT64) \\" >> $@; else echo "$(JAVA_DEFAULT32) \\" >> $@; fi
	@ echo "  $(JAVAFLAGS_DEFAULT) \\" >> $@
	@ echo "  $(JAVA_LIBPATH2) $(JAVA_CLASSPATH2) \\" >> $@
	@ echo "  test.NdbJTieLibraryLoadingTest > test_ndbjtie_library.log 2>&1" >> $@
	@CHMOD@ +x $@

## ndbjtie smoke test
test_ndbjtie_smoke.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_ndbjtie_smoke.log\"" >> $@
	@ echo "test -f test_ndbjtie_smoke.log && rm test_ndbjtie_smoke.log" >> $@
	if file $(NDBJTIE_O) | grep 64 > /dev/null 2>&1; then echo "$(JAVA_DEFAULT64) \\" >> $@; else echo "$(JAVA_DEFAULT32) \\" >> $@; fi
	@ echo "  $(JAVAFLAGS_DEFAULT) \\" >> $@
	@ echo "  $(JAVA_LIBPATH0) $(JAVA_CLASSPATH0) \\" >> $@
	@ echo "  test.NdbJTieSmokeTest > test_ndbjtie_smoke.log 2>&1" >> $@
	@CHMOD@ +x $@

## mysql-utils decimal unload test
MUTILS_DECIMAL_PROPERTIES = \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_package_prefixes="test.,com.mysql.ndbjtie." \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_class_name="test.MySqlUtilsDecimalTest" \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_method_name="test"

test_unload_mutils_decimal.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_unload_mutils_decimal.log\"" >> $@
	@ echo "test -f test_unload_mutils_decimal.log && rm test_unload_mutils_decimal.log" >> $@
	if file $(NDBJTIE_O) | grep 64 > /dev/null 2>&1; then echo "$(JAVA_DEFAULT64) \\" >> $@; else echo "$(JAVA_DEFAULT32) \\" >> $@; fi
	@ echo "  $(JAVAFLAGS_DEFAULT) \\" >> $@
	@ echo "  $(JAVA_LIBPATH1) $(JAVA_CLASSPATH1) \\" >> $@
	@ echo "  $(MUTILS_DECIMAL_PROPERTIES) \\" >> $@
	@ echo "  test.MyLoadUnloadTest > test_unload_mutils_decimal.log 2>&1" >> $@
	@CHMOD@ +x $@

## mysql-utils charsetmap unload test
MUTILS_CHARSETMAP_PROPERTIES = \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_package_prefixes="test.,com.mysql.ndbjtie." \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_class_name="test.MySqlUtilsCharsetMapTest" \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_method_name="test"

test_unload_mutils_charsetmap.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_unload_mutils_charsetmap.log\"" >> $@
	@ echo "test -f test_unload_mutils_charsetmap.log && rm test_unload_mutils_charsetmap.log" >> $@
	if file $(NDBJTIE_O) | grep 64 > /dev/null 2>&1; then echo "$(JAVA_DEFAULT64) \\" >> $@; else echo "$(JAVA_DEFAULT32) \\" >> $@; fi
	@ echo "  $(JAVAFLAGS_DEFAULT) \\" >> $@
	@ echo "  $(JAVA_LIBPATH1) $(JAVA_CLASSPATH1) \\" >> $@
	@ echo "  $(MUTILS_CHARSETMAP_PROPERTIES) \\" >> $@
	@ echo "  test.MyLoadUnloadTest > test_unload_mutils_charsetmap.log 2>&1" >> $@
	@CHMOD@ +x $@

## ndbjtie library unload test
NDBJTIE_LIBRARY_TEST_PROPERTIES = \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_package_prefixes="test.,myjapi.,com.mysql.ndbjtie." \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_class_name="test.NdbJTieLibraryLoadingTest" \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_method_name="test"
test_unload_ndbjtie_library.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_unload_ndbjtie_library.log\"" >> $@
	@ echo "test -f test_unload_ndbjtie_library.log && rm test_unload_ndbjtie_library.log" >> $@
	if file $(NDBJTIE_O) | grep 64 > /dev/null 2>&1; then echo "$(JAVA_DEFAULT64) \\" >> $@; else echo "$(JAVA_DEFAULT32) \\" >> $@; fi
	@ echo "  $(JAVAFLAGS_DEFAULT) \\" >> $@
	@ echo "  $(JAVA_LIBPATH3) $(JAVA_CLASSPATH3) \\" >> $@
	@ echo "  $(NDBJTIE_LIBRARY_TEST_PROPERTIES) \\" >> $@
	@ echo "  test.MyLoadUnloadTest > test_unload_ndbjtie_library.log 2>&1" >> $@
	@CHMOD@ +x $@

## ndbjtie smoke unload test
NDBJTIE_PROPERTIES = \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_package_prefixes="test.,com.mysql.ndbjtie." \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_class_name="test.NdbJTieSmokeTest" \
	-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_method_name="test"

test_unload_ndbjtie_smoke.sh: Makefile
	@ echo "#!/bin/sh" > $@
	@ echo "echo \"for test output see file: test_unload_ndbjtie_smoke.log\"" >> $@
	@ echo "test -f test_unload_ndbjtie_smoke.log && rm test_unload_ndbjtie_smoke.log" >> $@
	if file $(NDBJTIE_O) | grep 64 > /dev/null 2>&1; then echo "$(JAVA_DEFAULT64) \\" >> $@; else echo "$(JAVA_DEFAULT32) \\" >> $@; fi
	@ echo "  $(JAVAFLAGS_DEFAULT) \\" >> $@
	@ echo "  $(JAVA_LIBPATH1) $(JAVA_CLASSPATH1) \\" >> $@
	@ echo "  $(NDBJTIE_PROPERTIES) \\" >> $@
	@ echo "  test.MyLoadUnloadTest > test_unload_ndbjtie_smoke.log 2>&1" >> $@
	@CHMOD@ +x $@

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = $(TEST_DIR)/*.class *.log
CLEANFILES = $(NDBJTIE_TEST_JAR) $(UNIT_TESTS)

## ----------------------------------------------------------------------
