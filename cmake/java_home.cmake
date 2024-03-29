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

# Output from this file:
# JAVA_HOME        The value of JAVA HOME (in cache)
# JAVA_EXECUTABLE  The java executable (in cache)
# JAVAC_EXECUTABLE The javac executable (in cache)
# JAVA_VERSION     The found java version (in cache)
# JAVA_FOUND       TRUE if java found (in cache scope)

# NOTE: This module produces another set of variables than FindJava
# (which is invoked if you do FIND_PACKAGE(java)).
#
# TODO: Merge this file and cmake/java.cmake into one that is suitable
# for all Java usage when building mysql.

## Local function to check a Java binary for version number.

FUNCTION(CHECK_JAVA_VERSION _exec_path _wanted_version
    _found_required_java _found_java_version)
  IF(NOT _exec_path)
    RETURN()
  ENDIF()
  MESSAGE(STATUS "Checking java version for ${_exec_path}")
  EXECUTE_PROCESS(COMMAND ${_exec_path} -version
    OUTPUT_VARIABLE _vout
    ERROR_VARIABLE _vout)
  ## Assumes first found integers are version. This is correct for all
  ## relevant java versions today.
  STRING(REGEX MATCH "[0-9]+.[0-9]+.[0-9]+"
    _found_version "${_vout}")
  IF ("${_found_version}" VERSION_LESS ${_wanted_version})
    ## Not usable
  ELSE()
    SET(${_found_required_java} "TRUE" PARENT_SCOPE)
    SET(${_found_java_version} ${_found_version} PARENT_SCOPE)
  ENDIF()
ENDFUNCTION()


# Function that will ensure that JAVA_HOME is set to a Java version we
# can use.

FUNCTION(CHECK_JAVA_HOME _wanted_version)
  IF (JAVA_HOME_OK)
    MESSAGE(STATUS "Using cached java: ${JAVA_EXECUTABLE} (${JAVA_VERSION})")
    RETURN()
  ENDIF()

  ## Store major version for warning text.
  STRING(REGEX MATCH "[0-9]+"
    _major_version "${_wanted_version}")
  SET(JAVA_MAJOR_VERSION "${_major_version}" CACHE INTERNAL "Major version of Java")

  IF(DEFINED ENV{JAVA_HOME})
    IF (WIN32)
      SET(_java_exec "$ENV{JAVA_HOME}/bin/java.exe")
    ELSE()
      SET(_java_exec "$ENV{JAVA_HOME}/bin/java")
    ENDIF()
    IF (EXISTS "${_java_exec}")
      CHECK_JAVA_VERSION(${_java_exec} ${_wanted_version}
        _found_required_java _found_java_version)
      IF (NOT _found_required_java)
        MESSAGE(WARNING "JAVA_HOME does not point to Java ${_wanted_version} or higher")
      ENDIF()
    ELSE()
      MESSAGE(WARNING "JAVA_HOME=$ENV{JAVA_HOME} is not a proper java home")
    ENDIF()
  ELSE()
    MESSAGE(WARNING "JAVA_HOME must be set")
  ENDIF()

  ## Wrap up by setting the results
  IF (_found_required_java)
    IF (WIN32)
      SET(_javac_exec "$ENV{JAVA_HOME}/bin/javac.exe")
    ELSE()
      SET(_javac_exec "$ENV{JAVA_HOME}/bin/javac")
    ENDIF()
    ## The check that we also have a compiler
    IF (EXISTS "${_javac_exec}")
      SET(JAVA_EXECUTABLE "${_java_exec}" CACHE FILEPATH "Java program")
      SET(JAVAC_EXECUTABLE "${_javac_exec}" CACHE FILEPATH "Java compiler")
      SET(JAVA_HOME_OK "TRUE" CACHE BOOL "")
      SET(JAVA_VERSION "${_found_java_version}" CACHE STRING "Java version")
      SET(JAVA_HOME "$ENV{JAVA_HOME}" CACHE FILEPATH "Value of JAVA_HOME env")
      MESSAGE(STATUS "Found java: ${JAVA_EXECUTABLE} (${_found_java_version})")
    ELSE()
      MESSAGE(WARNING "Found ${JAVA_EXECUTABLE} (${_found_java_version}) but no java compiler")
    ENDIF()
  ENDIF()
ENDFUNCTION()

FUNCTION(WARN_NOT_USABLE_JAVA_HOME)
  IF (NOT JAVA_HOME_OK)
    MESSAGE(WARNING "JAVA_HOME was not pointing to a Java ${JAVA_MAJOR_VERSION} directory.\n"
      "  Set JAVA_HOME to point to a Java ${JAVA_MAJOR_VERSION} directory.\n"
      "    On lab computers: /usr/global/java/jdk-${JAVA_MAJOR_VERSION}.\n"
      "    On Ubuntu/Oracle Linux/RedHat : /usr/lib/jvm/some-path-to-jdk-${JAVA_MAJOR_VERSION}.\n"
      "  If you don't have Java ${JAVA_MAJOR_VERSION}, install a Java ${JAVA_MAJOR_VERSION} package first:\n"
      "    RHEL/Fedora/Oracle linux : yum install java-${JAVA_MAJOR_VERSION}-openjdk-devel\n"
      "    Debian/Ubuntu: apt install openjdk-${JAVA_MAJOR_VERSION}-jdk-headless\n"
      "    SLES/openSUSE: zypper install java-${JAVA_MAJOR_VERSION}-openjdk-devel\n"
      "    or download from https://www.oracle.com/java/technologies/javase-jdk${JAVA_MAJOR_VERSION}-downloads.html and follow instructions")
  ENDIF()
ENDFUNCTION()
