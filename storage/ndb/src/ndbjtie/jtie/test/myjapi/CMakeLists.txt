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


# the Java API wrapper and test
SET(JAVA_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi/A.java
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi/B0.java
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi/B1.java
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi/CI.java
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi/D0.java
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi/D1.java
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi/D2.java
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi/E.java
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi/MyJapi.java
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi/MyJapiCtypes.java
  ${CMAKE_CURRENT_SOURCE_DIR}/test/MyJapiTest.java
  )

SET(CLASSPATH
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/test/myjapi/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/target/classes)

SET(JARS
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/jtie.jar)

CREATE_JAR(jtie-test-myjapi ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  MERGE_JARS ${JARS}
  DEPENDENCIES jtie.jar)

INCLUDE_DIRECTORIES(
  ${JNI_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/utils
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/jtie
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/jtie/test/myapi)

# enable safety behaviour, expected by a myjapi subtest
STRING_APPEND(CMAKE_CXX_FLAGS " -DJTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE")

# the Java API's JNI implementation
ADD_LIBRARY(libmyjapi SHARED myjapi_lib.cpp)
# Set any special linker flags for shared library
SET_TARGET_PROPERTIES(libmyjapi
  PROPERTIES LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
SET_TARGET_PROPERTIES(libmyjapi PROPERTIES OUTPUT_NAME "myjapi")
TARGET_LINK_LIBRARIES(libmyjapi libmyapi)

# This is needed for java-based unit tests.
IF(MY_COMPILER_IS_CLANG AND WITH_UBSAN)
  TARGET_LINK_LIBRARIES(libmyjapi -lubsan)
ENDIF()

IF(APPLE)
  SET_TARGET_PROPERTIES(libmyjapi PROPERTIES
    MACOSX_RPATH ON
  )
ENDIF()

set(JAR "${JAVA_NDB_VERSION}.jar")
set(JTIE_MYJAPI_JAR "${CMAKE_CURRENT_BINARY_DIR}/jtie-test-myjapi-${JAR}")

ADD_TEST(
  NAME test_jtie_myjapi
  COMMAND
  ${Java_JAVA_EXECUTABLE_PATH} "-ea" "-Xcheck:jni"
  "-classpath" "${JTIE_MYJAPI_JAR}"
  "-Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${JAVA_SUBDIR}"
  "test.MyJapiTest"
)
SET_TESTS_PROPERTIES(test_jtie_myjapi PROPERTIES LABELS "NDB")
