# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
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

FILE(GLOB JAVA_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/test/*.java)

SET(CLASSPATH
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/test/unload/target/classes)

CREATE_JAR(jtie-test-unload ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH})

SET(JAR "${JAVA_NDB_VERSION}.jar")
SET(JTIE_TEST_UNLOAD_JAR "${CMAKE_CURRENT_BINARY_DIR}/jtie-test-unload-${JAR}")
GET_FILENAME_COMPONENT(PARENT ${CMAKE_CURRENT_BINARY_DIR} PATH)
SET(JTIE_MYJAPI_DIR "${PARENT}/myjapi")
SET(JTIE_MYJAPI_JAR "${JTIE_MYJAPI_DIR}/jtie-test-myjapi-${JAR}")

IF(WITH_ASAN)
  MESSAGE(STATUS "Skipping java tests for ASAN builds")
  RETURN()
ENDIF()

ADD_TEST(
  NAME test_jtie_unload
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} "-ea" "-Xcheck:jni"
  "-classpath" "${JTIE_TEST_UNLOAD_JAR}${JAVA_SEPERATOR}${JTIE_MYJAPI_JAR}"
  "-Djava.library.path=${JTIE_MYJAPI_DIR}/${JAVA_SUBDIR}"
  "-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_package_prefixes=test.,myjapi."
  "-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_class_name=test.MyJapiTest"
  "-Dcom.mysql.jtie.test.MyLoadUnloadTest.target_method_name=test"
  "test.MyLoadUnloadTest"
)
