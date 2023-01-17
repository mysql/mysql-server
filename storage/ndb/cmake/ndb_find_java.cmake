# Copyright (c) 2022, 2023, Oracle and/or its affiliates.
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

#
# Check for Java and JDK needed by ndbjtie and clusterj
#

# Print value of JAVA_HOME if set
IF(DEFINED ENV{JAVA_HOME})
  MESSAGE(STATUS "Looking for Java in JAVA_HOME=" $ENV{JAVA_HOME} " "
                 "and standard locations")
ELSE()
  MESSAGE(STATUS "Looking for Java in standard locations")
ENDIF()

FIND_PACKAGE(Java 1.8 COMPONENTS Development)
IF(NOT JAVA_FOUND)
  IF(DEFINED ENV{JAVA_HOME})
    # Could not find Java in the specific location set by JAVA_HOME
    # or in standard paths, don't search further
    MESSAGE(FATAL_ERROR "Could NOT find Java: neither in specified "
                        "JAVA_HOME=" $ENV{JAVA_HOME} " or standard location")
  ENDIF()

  #
  # Continue looking for Java in some additional
  # well known locations
  #

  # Prefer Java with same bit size as current build
  SET(_bit_suffix)
  IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(_bit_suffix "-64")
  ENDIF()

  # Use well known standard base
  SET(_base_path /usr/local/java/)
  IF(WINDOWS)
    SET(_base_path C:\\java\\)
  ENDIF()

  # Search for version in specified order
  SET(_preferred_versions 1.8)

  FOREACH(_version ${_preferred_versions})
    SET(_path ${_base_path}jdk${_version}${_bit_suffix})
    MESSAGE(STATUS "Looking for Java in ${_path}...")
    SET(ENV{JAVA_HOME} ${_path})
    FIND_PACKAGE(Java ${_version} COMPONENTS Development)
    IF(JAVA_FOUND)
      # Found java, no need to search further
      MESSAGE(STATUS "Found Java in ${_path}")
      BREAK()
    ENDIF()
  ENDFOREACH()

  IF(NOT JAVA_FOUND)
    # Could not find Java in well known locations either
    MESSAGE(FATAL_ERROR "Could NOT find suitable version of Java")
  ENDIF()

ENDIF()

MESSAGE(STATUS "Java_VERSION: ${Java_VERSION}")
MESSAGE(STATUS "Java_VERSION_STRING: ${Java_VERSION_STRING}")
MESSAGE(STATUS "JAVA_RUNTIME: ${JAVA_RUNTIME}")
MESSAGE(STATUS "JAVA_COMPILE: ${JAVA_COMPILE}")
MESSAGE(STATUS "JAVA_ARCHIVE: ${JAVA_ARCHIVE}")
NDB_REQUIRE_VARIABLE(JAVA_RUNTIME)
NDB_REQUIRE_VARIABLE(JAVA_COMPILE)
NDB_REQUIRE_VARIABLE(JAVA_ARCHIVE)

# Help FindJNI by setting JAVA_HOME (if not already set)
# to point at the java found above
IF(NOT DEFINED ENV{JAVA_HOME})
  # Convert to realpath
  GET_FILENAME_COMPONENT(java_home ${JAVA_COMPILE} REALPATH)
  # Remove filename
  GET_FILENAME_COMPONENT(java_home ${java_home} PATH)
  # Remove dir
  GET_FILENAME_COMPONENT(java_home ${java_home} PATH)
  MESSAGE(STATUS "Setting JAVA_HOME=${java_home}")
  SET(ENV{JAVA_HOME} ${java_home})
ENDIF()

FIND_PACKAGE(JNI REQUIRED)
MESSAGE(STATUS "JNI_FOUND: ${JNI_FOUND}")
MESSAGE(STATUS "JNI_INCLUDE_DIRS: ${JNI_INCLUDE_DIRS}")
MESSAGE(STATUS "JNI_LIBRARIES: ${JNI_LIBRARIES}")
NDB_REQUIRE_VARIABLE(JNI_INCLUDE_DIRS)

INCLUDE(ndb_java_macros)
SET(WITH_CLASSPATH ${WITH_CLASSPATH} CACHE STRING
  "Enable the classpath for MySQL Cluster Java Connector")
