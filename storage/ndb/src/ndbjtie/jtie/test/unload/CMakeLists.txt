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
  "test_unload.sh.log;test_unload.cmd.log")

FILE(GLOB JAVA_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/test/*.java)

SET(CLASSPATH
  ${CMAKE_BINARY_DIR}/storage/ndb/src/ndbjtie/jtie/test/unload/target/classes)

CREATE_JAR(jtie-test-unload ${JAVA_SOURCES}
  CLASSPATH ${CLASSPATH})

IF(WIN32)

  # build the unit-test script for win
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_unload.cmd.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_unload.cmd
		 @ONLY NEWLINE_STYLE WIN32)

ELSE(WIN32)

  # build the unit-test script for *nix
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/test_unload.sh.in
                 ${CMAKE_CURRENT_BINARY_DIR}/test_unload.sh
		 @ONLY NEWLINE_STYLE UNIX)
  ADD_CUSTOM_TARGET(chmod_x_unload ALL
    chmod a+x "${CMAKE_CURRENT_BINARY_DIR}/test_unload.sh"
    || echo "") # don't break the build if file was deleted)

ENDIF(WIN32)
