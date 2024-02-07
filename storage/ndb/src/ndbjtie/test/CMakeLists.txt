# Copyright (c) 2010, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
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
SET(JAVA_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/test/JTieTestBase.java
  ${CMAKE_CURRENT_SOURCE_DIR}/test/MySqlUtilsTest.java
  ${CMAKE_CURRENT_SOURCE_DIR}/test/NdbJTieConstantsTest.java
  ${CMAKE_CURRENT_SOURCE_DIR}/test/NdbJTieMultiLibTest.java
  ${CMAKE_CURRENT_SOURCE_DIR}/test/NdbJTieSmokeTest.java
  )

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
  ${JNI_INCLUDE_DIRS}
  )

# enable safety behaviour
STRING_APPEND(CMAKE_CXX_FLAGS " -DJTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE")

# shared library for testing
ADD_LIBRARY(libndbjtie_unit_tests SHARED
  ndbjtie_unit_tests_consts.cpp
  ndbjtie_unit_tests_lib.cpp
  )

SET_TARGET_PROPERTIES(libndbjtie_unit_tests PROPERTIES
  LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}"
  OUTPUT_NAME "ndbjtie_unit_tests")

SET_TARGET_PROPERTIES(libndbjtie_unit_tests PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)

IF(WIN32)
  # This must be a cmake bug on windows ...
  # Anyways, with this the .dll ends up in the desired directory.
  SET_TARGET_PROPERTIES(libndbjtie_unit_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/library_output_directory)
ENDIF()

IF(APPLE)
  SET_TARGET_PROPERTIES(libndbjtie_unit_tests PROPERTIES
    MACOSX_RPATH ON
  )
ENDIF()

#### Tests ###

SET(BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}")
SET(TEST_JAR "${BIN_DIR}/ndbjtie-test-${JAVA_NDB_VERSION}.jar")

GET_FILENAME_COMPONENT(NDBJTIE_DIR "${BIN_DIR}" PATH)
SET(NDBJTIE_JAR "${NDBJTIE_DIR}/ndbjtie-${JAVA_NDB_VERSION}.jar")

GET_FILENAME_COMPONENT(NDBCLIENT_DIR "${NDBJTIE_DIR}" PATH)

SET(MYJAPI_TEST_DIR "${NDBJTIE_DIR}/jtie/test/myjapi")
SET(MYJAPI_TEST_JAR
  "${MYJAPI_TEST_DIR}/jtie-test-myjapi-${JAVA_NDB_VERSION}.jar")

SET(UNLOAD_TEST_DIR "${NDBJTIE_DIR}/jtie/test/unload")
SET(UNLOAD_TEST_JAR
  "${UNLOAD_TEST_DIR}/jtie-test-unload-${JAVA_NDB_VERSION}.jar")

# Needed on Windows, to find SSL .dlls at runtime.
IF(WIN32)
  SET(JAVATEST_WORKING_DIRECTORY
    WORKING_DIRECTORY
    "${CMAKE_BINARY_DIR}/runtime_output_directory/${JAVA_SUBDIR}")
ENDIF()

MESSAGE(STATUS "JAVATEST_WORKING_DIRECTORY is ${JAVATEST_WORKING_DIRECTORY}")

ADD_TEST(
  NAME test_ndbjtie_mutils
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}${JAVA_SEPARATOR}${NDBJTIE_JAR}"
  "-Djava.library.path=${CMAKE_BINARY_DIR}/library_output_directory/${JAVA_SUBDIR}"
  "test.MySqlUtilsTest"
  ${JAVATEST_WORKING_DIRECTORY}
)

ADD_TEST(
  NAME test_ndbjtie_constants
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}${JAVA_SEPARATOR}${NDBJTIE_JAR}"
  "-Djava.library.path=${CMAKE_BINARY_DIR}/library_output_directory/${JAVA_SUBDIR}${JAVA_SEPARATOR}${BIN_DIR}"
  "-Dtest.NdbJTieConstantsTest.ndbjtie_jar_file=${NDBJTIE_JAR}"
  "test.NdbJTieConstantsTest"
  ${JAVATEST_WORKING_DIRECTORY}
)

ADD_TEST(
  NAME test_ndbjtie_multilib
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}${JAVA_SEPARATOR}${NDBJTIE_JAR}${JAVA_SEPARATOR}${MYJAPI_TEST_JAR}"
  "-Djava.library.path=${CMAKE_BINARY_DIR}/library_output_directory/${JAVA_SUBDIR}${JAVA_SEPARATOR}${MYJAPI_TEST_DIR}/${JAVA_SUBDIR}${JAVA_SEPARATOR}${BIN_DIR}"
  "test.NdbJTieMultiLibTest"
  ${JAVATEST_WORKING_DIRECTORY}
)

ADD_TEST(
  NAME test_ndbjtie_smoke
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}${JAVA_SEPARATOR}${NDBJTIE_JAR}"
  "-Djava.library.path=${CMAKE_BINARY_DIR}/library_output_directory/${JAVA_SUBDIR}"
  "test.NdbJTieSmokeTest"
  ${JAVATEST_WORKING_DIRECTORY}
)

SET(UNLOAD_TEST "com.mysql.jtie.test.MyLoadUnloadTest")


ADD_TEST(
  NAME test_ndbjtie_unload_mutils
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}${JAVA_SEPARATOR}${NDBJTIE_JAR}${JAVA_SEPARATOR}${UNLOAD_TEST_JAR}"
  "-Djava.library.path=${CMAKE_BINARY_DIR}/library_output_directory/${JAVA_SUBDIR}${JAVA_SEPARATOR}${BIN_DIR}"
  "-D${UNLOAD_TEST}.target_package_prefixes=test.,com.mysql.ndbjtie."
  "-D${UNLOAD_TEST}.target_class_name=test.MySqlUtilsTest"
  "-D${UNLOAD_TEST}.target_method_name=test"
  "test.MyLoadUnloadTest"
  ${JAVATEST_WORKING_DIRECTORY}
)


ADD_TEST(
  NAME test_ndbjtie_unload_constants
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}${JAVA_SEPARATOR}${NDBJTIE_JAR}${JAVA_SEPARATOR}${UNLOAD_TEST_JAR}"
  "-Djava.library.path=${CMAKE_BINARY_DIR}/library_output_directory/${JAVA_SUBDIR}${JAVA_SEPARATOR}${BIN_DIR}"
  "-D${UNLOAD_TEST}.target_package_prefixes=test.,com.mysql.ndbjtie."
  "-D${UNLOAD_TEST}.target_class_name=test.NdbJTieConstantsTest"
  "-D${UNLOAD_TEST}.target_method_name=test"
  "-Dtest.NdbJTieConstantsTest.ndbjtie_jar_file=${NDBJTIE_JAR}"
  "test.MyLoadUnloadTest"
  ${JAVATEST_WORKING_DIRECTORY}
)

ADD_TEST(
  NAME test_ndbjtie_unload_multilib
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}${JAVA_SEPARATOR}${NDBJTIE_JAR}${JAVA_SEPARATOR}${UNLOAD_TEST_JAR}${JAVA_SEPARATOR}${MYJAPI_TEST_JAR}"
  "-Djava.library.path=${CMAKE_BINARY_DIR}/library_output_directory/${JAVA_SUBDIR}${JAVA_SEPARATOR}${MYJAPI_TEST_DIR}/${JAVA_SUBDIR}${JAVA_SEPARATOR}${BIN_DIR}"
  "-D${UNLOAD_TEST}.target_package_prefixes=test.,myjapi.,com.mysql.ndbjtie."
  "-D${UNLOAD_TEST}.target_class_name=test.NdbJTieMultiLibTest"
  "-D${UNLOAD_TEST}.target_method_name=test"
  "test.MyLoadUnloadTest"
  ${JAVATEST_WORKING_DIRECTORY}
)

ADD_TEST(
  NAME test_ndbjtie_unload_smoke
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} "-ea" "-Xcheck:jni"
  "-classpath" "${TEST_JAR}${JAVA_SEPARATOR}${NDBJTIE_JAR}${JAVA_SEPARATOR}${UNLOAD_TEST_JAR}"
  "-Djava.library.path=${CMAKE_BINARY_DIR}/library_output_directory/${JAVA_SUBDIR}"
  "-D${UNLOAD_TEST}.target_package_prefixes=test.,com.mysql.ndbjtie."
  "-D${UNLOAD_TEST}.target_class_name=test.NdbJTieSmokeTest"
  "-D${UNLOAD_TEST}.target_method_name=test"
  "test.MyLoadUnloadTest"
  ${JAVATEST_WORKING_DIRECTORY}
)

SET_TESTS_PROPERTIES(
  test_ndbjtie_constants
  test_ndbjtie_multilib
  test_ndbjtie_mutils
  test_ndbjtie_smoke
  test_ndbjtie_unload_constants
  test_ndbjtie_unload_multilib
  test_ndbjtie_unload_mutils
  test_ndbjtie_unload_smoke
  PROPERTIES LABELS "NDB")
