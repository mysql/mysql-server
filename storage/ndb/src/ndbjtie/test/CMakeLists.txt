# Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# the test classes
FILE(GLOB JAVA_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/test/*.java)

SET(CLASSPATH
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/test/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/test/myjapi/target/classes)

CREATE_JAR(ndbjtie-test ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  DEPENDENCIES ndbjtie.jar jtie.jar jtie-test-myjapi.jar)


INCLUDE_DIRECTORIES(
  	${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/utils
  	${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/jtie
  	${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmapi
  	${CMAKE_SOURCE_DIR}/storage/ndb/include/ndbapi
  	${JNI_INCLUDE_DIRS})

# enable safety behaviour
SET(CMAKE_CXX_FLAGS
	"${CMAKE_CXX_FLAGS} -DJTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE")

# shared library for testing
ADD_LIBRARY(libndbjtie_unit_tests SHARED
	ndbjtie_unit_tests_consts.cpp
	ndbjtie_unit_tests_lib.cpp)

SET_TARGET_PROPERTIES(libndbjtie_unit_tests PROPERTIES
  LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}"
  OUTPUT_NAME "ndbjtie_unit_tests")


#### Tests ###

SET(BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}")
SET(TEST_JAR "${BIN_DIR}/ndbjtie-test-${JAVA_NDB_VERSION}.jar")

GET_FILENAME_COMPONENT(NDBJTIE_DIR "${BIN_DIR}" PATH)
SET(NDBJTIE_JAR "${NDBJTIE_DIR}/ndbjtie-${JAVA_NDB_VERSION}.jar")

GET_FILENAME_COMPONENT(NDBCLIENT_DIR "${NDBJTIE_DIR}" PATH)

SET(MYJAPI_TEST_DIR "${NDBJTIE_DIR}/jtie/test/myjapi")
SET(MYJAPI_TEST_JAR "${MYJAPI_TEST_DIR}/jtie-test-myjapi-${JAVA_NDB_VERSION}.jar")

SET(UNLOAD_TEST_DIR "${NDBJTIE_DIR}/jtie/test/unload")
SET(UNLOAD_TEST_JAR "${UNLOAD_TEST_DIR}/jtie-test-unload-${JAVA_NDB_VERSION}.jar")


ADD_TEST(
  NAME test_ndbjtie_mutils
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} ${Java_JAVA_ARCH_OPT} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}:${NDBJTIE_JAR}"
  "-Djava.library.path=${NDBCLIENT_DIR}"
  "test.MySqlUtilsTest"
)

ADD_TEST(
  NAME test_ndbjtie_constants
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} ${Java_JAVA_ARCH_OPT} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}:${NDBJTIE_JAR}"
  "-Djava.library.path=${NDBCLIENT_DIR}:${BIN_DIR}"
  "-Dtest.NdbJTieConstantsTest.ndbjtie_jar_file=${NDBJTIE_JAR}"
  "test.NdbJTieConstantsTest"
)

ADD_TEST(
  NAME test_ndbjtie_multilib
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} ${Java_JAVA_ARCH_OPT} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}:${NDBJTIE_JAR}:${MYJAPI_TEST_JAR}"
  "-Djava.library.path=${NDBCLIENT_DIR}:${MYJAPI_TEST_DIR}:${BIN_DIR}"
  "test.NdbJTieMultiLibTest"
)

ADD_TEST(
  NAME test_ndbjtie_smoke
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} ${Java_JAVA_ARCH_OPT} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}:${NDBJTIE_JAR}"
  "-Djava.library.path=${NDBCLIENT_DIR}"
  "test.NdbJTieSmokeTest"
)

SET(UNLOAD_TEST "com.mysql.jtie.test.MyLoadUnloadTest")

ADD_TEST(
  NAME test_ndbjtie_unload_mutils
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} ${Java_JAVA_ARCH_OPT} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}:${NDBJTIE_JAR}:${UNLOAD_TEST_JAR}"
  "-Djava.library.path=${NDBCLIENT_DIR}:${BIN_DIR}"
  "-D${UNLOAD_TEST}.target_package_prefixes=test.,com.mysql.ndbjtie."
  "-D${UNLOAD_TEST}.target_class_name=test.MySqlUtilsTest"
  "-D${UNLOAD_TEST}.target_method_name=test"
  "test.MyLoadUnloadTest"
)

ADD_TEST(
  NAME test_ndbjtie_unload_constants
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} ${Java_JAVA_ARCH_OPT} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}:${NDBJTIE_JAR}:${UNLOAD_TEST_JAR}"
  "-Djava.library.path=${NDBCLIENT_DIR}:${BIN_DIR}"
  "-D${UNLOAD_TEST}.target_package_prefixes=test.,com.mysql.ndbjtie."
  "-D${UNLOAD_TEST}.target_class_name=test.NdbJTieConstantsTest"
  "-D${UNLOAD_TEST}.target_method_name=test"
  "-Dtest.NdbJTieConstantsTest.ndbjtie_jar_file=${NDBJTIE_JAR}"
  "test.MyLoadUnloadTest"
)

ADD_TEST(
  NAME test_ndbjtie_unload_multilib
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} ${Java_JAVA_ARCH_OPT} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}:${NDBJTIE_JAR}:${UNLOAD_TEST_JAR}:${MYJAPI_TEST_JAR}"
  "-Djava.library.path=${NDBCLIENT_DIR}:${MYJAPI_TEST_DIR}:${BIN_DIR}"
  "-D${UNLOAD_TEST}.target_package_prefixes=test.,myjapi.,com.mysql.ndbjtie."
  "-D${UNLOAD_TEST}.target_class_name=test.NdbJTieMultiLibTest"
  "-D${UNLOAD_TEST}.target_method_name=test"
  "test.MyLoadUnloadTest"
)

ADD_TEST(
  NAME test_ndbjtie_unload_smoke
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} ${Java_JAVA_ARCH_OPT} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}:${NDBJTIE_JAR}:${UNLOAD_TEST_JAR}"
  "-Djava.library.path=${NDBCLIENT_DIR}"
  "-D${UNLOAD_TEST}.target_package_prefixes=test.,com.mysql.ndbjtie."
  "-D${UNLOAD_TEST}.target_class_name=test.NdbJTieSmokeTest"
  "-D${UNLOAD_TEST}.target_method_name=test"
  "test.MyLoadUnloadTest"
)
