# Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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

FUNCTION(FIND_MAVEN)
  IF (MAVEN_FOUND)
    MESSAGE(STATUS "Using cached maven: ${MAVEN_EXECUTABLE} (${MAVEN_VERSION})")
    RETURN()
  ENDIF()
  ## Lab defines MAVEN_HOME_WIN on Windows and MAVEN_HOME on unices.
  IF (WIN32)
    IF (DEFINED ENV{MAVEN_HOME_WIN})
      FIND_PROGRAM(MAVEN_EXECUTABLE mvn
        NO_DEFAULT_PATH
        PATHS "$ENV{MAVEN_HOME_WIN}/bin")
    ELSE()
      FIND_PROGRAM(MAVEN_EXECUTABLE mvn)
    ENDIF()
  ELSE()
    IF (DEFINED ENV{MAVEN_HOME})
      FIND_PROGRAM(MAVEN_EXECUTABLE mvn
        NO_DEFAULT_PATH
        PATHS "$ENV{MAVEN_HOME}/bin")
    ELSE()
      FIND_PROGRAM(MAVEN_EXECUTABLE mvn
        ## Include the standard lab maven installation
        PATHS /usr/global/share/java/maven/bin)
    ENDIF()
  ENDIF()

  IF(MAVEN_EXECUTABLE)
    EXECUTE_PROCESS(COMMAND ${MAVEN_EXECUTABLE} --version
      OUTPUT_VARIABLE MAVEN_VERSION_OUTPUT
      ERROR_VARIABLE MAVEN_VERSION_OUTPUT)
    STRING(REGEX MATCH "[0-9]+.[0-9]+.[0-9]+"
      _maven_version "${MAVEN_VERSION_OUTPUT}")
    SET(MAVEN_VERSION "${_maven_version}" CACHE STRING "")
    MESSAGE(STATUS "Found maven: ${MAVEN_EXECUTABLE} (${MAVEN_VERSION})")
    SET(MAVEN_FOUND "TRUE" CACHE BOOL "")
  ENDIF()
ENDFUNCTION()

FUNCTION(WARN_MISSING_MAVEN)
  IF (NOT MAVEN_FOUND)
    MESSAGE(WARNING "Maven not found.\n"
      " Either\n"
      "  1) set MAVEN_HOME/MAVEN_HOME_WIN to point to a maven installation,\n"
      "  2) install a maven package:\n"
      "    RHEL/Fedora/Oracle linux : yum install maven\n"
      "    Debian/Ubuntu: apt install maven\n"
      "    SLES/openSUSE: zypper install maven\n"
      "  3) or download from https://maven.apache.org/download.cgi and follow instructions")
  ENDIF()
ENDFUNCTION()
