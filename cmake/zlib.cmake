# Copyright (c) 2009, 2022, Oracle and/or its affiliates.
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

# Usage:
#  cmake -DWITH_ZLIB="bundled"|"system"
#
# Default is "bundled".
# The default should be "system" on non-windows platforms,
# but we need at least version 1.2.11, and that's not available on
# all the platforms we need to support.

# Security bug fixes required from:
SET(MIN_ZLIB_VERSION_REQUIRED "1.2.13")


MACRO(FIND_ZLIB_VERSION)
  FOREACH(version_part
      ZLIB_VER_MAJOR
      ZLIB_VER_MINOR
      ZLIB_VER_REVISION
      )
    FILE(STRINGS "${ZLIB_INCLUDE_DIR}/zlib.h" ${version_part}
      REGEX "^#[\t ]*define[\t ]+${version_part}[\t ]+([0-9]+).*")
    STRING(REGEX REPLACE
      "^.*${version_part}[\t ]+([0-9]+).*" "\\1"
      ${version_part} "${${version_part}}")
  ENDFOREACH()
  SET(ZLIB_VERSION "${ZLIB_VER_MAJOR}.${ZLIB_VER_MINOR}.${ZLIB_VER_REVISION}")
  SET(ZLIB_VERSION "${ZLIB_VERSION}" CACHE INTERNAL "ZLIB major.minor.step")
  MESSAGE(STATUS "ZLIB_VERSION (${WITH_ZLIB}) is ${ZLIB_VERSION}")
  MESSAGE(STATUS "ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIR}")
  MESSAGE(STATUS "ZLIB_LIBRARY ${ZLIB_LIBRARY}")
ENDMACRO()

MACRO (FIND_SYSTEM_ZLIB)
  # In case we are changing from "bundled" to "system".
  IF(DEFINED ZLIB_LIBRARY AND ZLIB_LIBRARY STREQUAL zlib)
    UNSET(ZLIB_LIBRARY)
    UNSET(ZLIB_LIBRARY CACHE)
  ENDIF()
  FIND_PACKAGE(ZLIB)
  IF(ZLIB_FOUND)
    SET(ZLIB_LIBRARY ${ZLIB_LIBRARIES} CACHE INTERNAL "System zlib library")
    IF(NOT ZLIB_INCLUDE_DIR STREQUAL "/usr/include")
      # In case of -DCMAKE_PREFIX_PATH=</path/to/custom/zlib>
      INCLUDE_DIRECTORIES(BEFORE SYSTEM ${ZLIB_INCLUDE_DIR})
    ENDIF()
  ENDIF()
ENDMACRO()

MACRO (RESET_ZLIB_VARIABLES)
  # Reset whatever FIND_PACKAGE may have left behind.
  FOREACH(zlibvar
      INCLUDE_DIR
      INCLUDE_DIRS
      LIBRARY
      LIBRARIES
      LIBRARY_DEBUG
      LIBRARY_RELEASE)
    UNSET(ZLIB_${zlibvar})
    UNSET(ZLIB_${zlibvar} CACHE)
    UNSET(ZLIB_${zlibvar}-ADVANCED CACHE)
  ENDFOREACH()
  UNSET(FIND_PACKAGE_MESSAGE_DETAILS_ZLIB)
  UNSET(FIND_PACKAGE_MESSAGE_DETAILS_ZLIB CACHE)
ENDMACRO()

SET(ZLIB_VERSION_DIR "zlib-1.2.13")
SET(BUNDLED_ZLIB_PATH ${CMAKE_SOURCE_DIR}/extra/zlib/${ZLIB_VERSION_DIR})

MACRO (MYSQL_USE_BUNDLED_ZLIB)
  RESET_ZLIB_VARIABLES()

  SET(ZLIB_INCLUDE_DIR ${BUNDLED_ZLIB_PATH})
  SET(ZLIB_LIBRARY zlib CACHE INTERNAL "Bundled zlib library")
  SET(WITH_ZLIB "bundled" CACHE STRING "Use bundled zlib")
  INCLUDE_DIRECTORIES(BEFORE SYSTEM
    ${CMAKE_SOURCE_DIR}/extra/zlib/${ZLIB_VERSION_DIR}
    ${CMAKE_BINARY_DIR}/extra/zlib/${ZLIB_VERSION_DIR}
    )
  ADD_SUBDIRECTORY(extra/zlib/${ZLIB_VERSION_DIR})

  # Add support for bundled curl.
  IF(NOT CMAKE_VERSION VERSION_LESS 3.4)
    ADD_LIBRARY(ZLIB::ZLIB UNKNOWN IMPORTED)
    SET_TARGET_PROPERTIES(ZLIB::ZLIB PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}")
    SET_PROPERTY(TARGET ZLIB::ZLIB APPEND PROPERTY
      IMPORTED_LOCATION
      "${CMAKE_BINARY_DIR}/archive_output_directory/libzlib.a")
  ENDIF()
ENDMACRO()


MACRO (MYSQL_CHECK_ZLIB)

  IF(NOT WITH_ZLIB)
    SET(WITH_ZLIB "bundled"
      CACHE STRING "By default use bundled zlib on this platform")
  ENDIF()
  
  IF(WITH_ZLIB STREQUAL "bundled")
    MYSQL_USE_BUNDLED_ZLIB()
  ELSEIF(WITH_ZLIB STREQUAL "system")
    FIND_SYSTEM_ZLIB()
    IF(NOT ZLIB_FOUND)
      MESSAGE(FATAL_ERROR "Cannot find system zlib libraries.")
    ENDIF()
  ELSE()
    RESET_ZLIB_VARIABLES()
    MESSAGE(FATAL_ERROR "WITH_ZLIB must be bundled or system")
  ENDIF()
  FIND_ZLIB_VERSION()
  IF(ZLIB_VERSION VERSION_LESS MIN_ZLIB_VERSION_REQUIRED)
    MESSAGE(FATAL_ERROR
      "ZLIB version must be at least ${MIN_ZLIB_VERSION_REQUIRED}, "
      "found ${ZLIB_VERSION}.\nPlease use -DWITH_ZLIB=bundled")
  ENDIF()
ENDMACRO()
