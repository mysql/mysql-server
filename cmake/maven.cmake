# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(java)

IF (WIN32)
  IF (DEFINED ENV{MAVEN_HOME_WIN})
    FIND_PROGRAM(MAVEN_EXECUTABLE
      mvn
      PATHS "$ENV{MAVEN_HOME_WIN}/bin"
      )
  ELSE()
    FIND_PROGRAM(MAVEN_EXECUTABLE
      mvn
      )
  ENDIF()
ELSE()
  IF (DEFINED ENV{MAVEN_HOME})
    FIND_PROGRAM(MAVEN_EXECUTABLE
      mvn
      PATHS "$ENV{MAVEN_HOME}/bin"
      )
  ELSE()
    FIND_PROGRAM(MAVEN_EXECUTABLE
      mvn
      PATHS /usr/global/share/java/maven/bin
      )
  ENDIF()
ENDIF()

IF(MAVEN_EXECUTABLE)
  IF(UNIX)
    FILE(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mavenversion.sh"
      "${MAVEN_EXECUTABLE} --version | head -1\n")
    EXECUTE_PROCESS(COMMAND bash "${CMAKE_CURRENT_BINARY_DIR}/mavenversion.sh"
      OUTPUT_VARIABLE MAVEN_VERSION_OUTPUT)
    FILE(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/mavenversion.sh")
    STRING(STRIP "${MAVEN_VERSION_OUTPUT}" MAVEN_VERSION)
  ENDIF()
  MESSAGE(STATUS "Found maven: ${MAVEN_EXECUTABLE} (${MAVEN_VERSION})")
ELSE()
  MESSAGE(FATAL_ERROR "Maven not found!")
ENDIF()
