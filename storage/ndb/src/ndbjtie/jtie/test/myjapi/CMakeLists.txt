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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# neither wildcards (nor line breaks) for clean targets supported by cmake
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
  "test_myjapi.sh.log;test_myjapi.cmd.log")

# the Java API wrapper and test
SET(JAVA_SOURCE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/myjapi
  ${CMAKE_CURRENT_SOURCE_DIR}/test)

SET(JAVA_SOURCES "")
FOREACH(D ${JAVA_SOURCE_DIRS})
  FILE(GLOB tmp ${D}/*.java)
  LIST(APPEND JAVA_SOURCES ${tmp})
ENDFOREACH()

SET(CLASSPATH
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/test/myjapi/target/classes
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/target/classes)

SET(JARS
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/jtie.jar)

CREATE_JAR(jtie-test-myjapi ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH}
  MERGE_JARS ${JARS}
  DEPENDENCIES jtie.jar)

IF(HAVE_JDK)

  INCLUDE_DIRECTORIES(
  	${JNI_INCLUDE_DIRS}
  	${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/utils
  	${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/jtie
  	${CMAKE_SOURCE_DIR}/storage/ndb/src/ndbjtie/jtie/test/myapi)

  # enable safety behaviour, expected by a myjapi subtest
  SET(CMAKE_CXX_FLAGS
  	"${CMAKE_CXX_FLAGS} -DJTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE")

  # the Java API's JNI implementation
  ADD_LIBRARY(libmyjapi SHARED myjapi_lib.cpp)
  # Set any special linker flags for shared library
  SET_TARGET_PROPERTIES(libmyjapi
    PROPERTIES LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
  SET_TARGET_PROPERTIES(libmyjapi PROPERTIES OUTPUT_NAME "myjapi")
  TARGET_LINK_LIBRARIES(libmyjapi libmyapi)

ENDIF(HAVE_JDK)

IF(WIN32)

  # build the unit-test script for win
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_myjapi.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_myjapi.cmd
		 @ONLY NEWLINE_STYLE WIN32)

ELSE(WIN32)

  # build the unit-test script for *nix
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_myjapi.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_myjapi.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_myjapi ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_myjapi.sh"
    || echo "") # don't break the build if file was deleted

ENDIF(WIN32)
