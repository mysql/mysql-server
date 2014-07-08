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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# neither wildcards (nor line breaks) for clean targets supported by cmake
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
  "test_mutils.sh.log;test_mutils.cmd.log;test_ndbjtie_constants.sh.log;test_ndbjtie_constants.cmd.log;test_ndbjtie_multilib.sh.log;test_ndbjtie_multilib.cmd.log;test_ndbjtie_smoke.sh.log;test_ndbjtie_smoke.cmd.log;test_unload_mutils.sh.log;test_unload_mutils.cmd.log;test_unload_ndbjtie_constants.sh.log;test_unload_ndbjtie_constants.cmd.log;test_unload_ndbjtie_multilib.sh.log;test_unload_ndbjtie_multilib.cmd.log;test_unload_ndbjtie_smoke.sh.log;test_unload_ndbjtie_smoke.cmd.log;")

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

ADD_EXECUTABLE(ndbjtie_unit_tests-t ndbjtie_unit_tests-t.cpp)

IF(HAVE_JDK)

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
  # Set any special linker flags for shared library
  SET_TARGET_PROPERTIES(libndbjtie_unit_tests
    PROPERTIES LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
  SET_TARGET_PROPERTIES(libndbjtie_unit_tests PROPERTIES
	OUTPUT_NAME "ndbjtie_unit_tests")
  # no other library dependencies
  #TARGET_LINK_LIBRARIES(libndbjtie_unit_tests ...)

ENDIF(HAVE_JDK)

IF(WIN32)

  # build the unit-test scripts for win
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_mutils.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_mutils.cmd
		 @ONLY NEWLINE_STYLE WIN32)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_ndbjtie_constants.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_ndbjtie_constants.cmd
		 @ONLY NEWLINE_STYLE WIN32)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_ndbjtie_multilib.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_ndbjtie_multilib.cmd
		 @ONLY NEWLINE_STYLE WIN32)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_ndbjtie_smoke.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_ndbjtie_smoke.cmd
		 @ONLY NEWLINE_STYLE WIN32)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_unload_mutils.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_unload_mutils.cmd
		 @ONLY NEWLINE_STYLE WIN32)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_unload_ndbjtie_constants.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_unload_ndbjtie_constants.cmd
		 @ONLY NEWLINE_STYLE WIN32)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_unload_ndbjtie_multilib.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_unload_ndbjtie_multilib.cmd
		 @ONLY NEWLINE_STYLE WIN32)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_unload_ndbjtie_smoke.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_unload_ndbjtie_smoke.cmd
		 @ONLY NEWLINE_STYLE WIN32)

ELSE(WIN32)

  # build the unit-test scripts for *nix
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_mutils.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_mutils.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_mutils ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_mutils.sh"
    || echo "") # don't break the build if file was deleted)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_ndbjtie_constants.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_ndbjtie_constants.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_ndbjtie_constants ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_ndbjtie_constants.sh"
    || echo "") # don't break the build if file was deleted)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_ndbjtie_multilib.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_ndbjtie_multilib.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_ndbjtie_multilib ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_ndbjtie_multilib.sh"
    || echo "") # don't break the build if file was deleted)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_ndbjtie_smoke.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_ndbjtie_smoke.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_ndbjtie_smoke ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_ndbjtie_smoke.sh"
    || echo "") # don't break the build if file was deleted)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_unload_mutils.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_unload_mutils.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_unload_mutils ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_unload_mutils.sh"
    || echo "") # don't break the build if file was deleted)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_unload_ndbjtie_constants.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_unload_ndbjtie_constants.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_unload_ndbjtie_constants ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_unload_ndbjtie_constants.sh"
    || echo "") # don't break the build if file was deleted)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_unload_ndbjtie_multilib.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_unload_ndbjtie_multilib.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_unload_ndbjtie_multilib ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_unload_ndbjtie_multilib.sh"
    || echo "") # don't break the build if file was deleted)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_unload_ndbjtie_smoke.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_unload_ndbjtie_smoke.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_unload_ndbjtie_smoke ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_unload_ndbjtie_smoke.sh"
    || echo "") # don't break the build if file was deleted)

ENDIF(WIN32)
