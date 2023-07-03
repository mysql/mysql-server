# Copyright (c) 2009, 2023, Oracle and/or its affiliates.
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


FUNCTION(FIND_ZLIB_VERSION ZLIB_INCLUDE_DIR)
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
ENDFUNCTION(FIND_ZLIB_VERSION)

FUNCTION(FIND_SYSTEM_ZLIB)
  FIND_PACKAGE(ZLIB)
  IF(ZLIB_FOUND)
    ADD_LIBRARY(zlib_interface INTERFACE)
    TARGET_LINK_LIBRARIES(zlib_interface INTERFACE ${ZLIB_LIBRARIES})

    IF(NOT ZLIB_INCLUDE_DIR STREQUAL "/usr/include")
      TARGET_INCLUDE_DIRECTORIES(zlib_interface SYSTEM INTERFACE
        ${ZLIB_INCLUDE_DIR})
    ENDIF()
  ENDIF()
ENDFUNCTION(FIND_SYSTEM_ZLIB)

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

FUNCTION(MYSQL_USE_BUNDLED_ZLIB)
  RESET_ZLIB_VARIABLES()

  ADD_LIBRARY(zlib_interface INTERFACE)
  TARGET_LINK_LIBRARIES(zlib_interface INTERFACE zlib)
  TARGET_INCLUDE_DIRECTORIES(zlib_interface SYSTEM BEFORE INTERFACE
    ${CMAKE_SOURCE_DIR}/extra/zlib/${ZLIB_VERSION_DIR}
    ${CMAKE_BINARY_DIR}/extra/zlib/${ZLIB_VERSION_DIR}
    )

  FIND_ZLIB_VERSION(${BUNDLED_ZLIB_PATH})

  ADD_SUBDIRECTORY(extra/zlib/${ZLIB_VERSION_DIR})

  # Add support for bundled curl.
  ADD_LIBRARY(ZLIB::ZLIB ALIAS zlib_interface)

ENDFUNCTION(MYSQL_USE_BUNDLED_ZLIB)


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

  ADD_LIBRARY(ext::zlib ALIAS zlib_interface)

  IF(ZLIB_VERSION VERSION_LESS MIN_ZLIB_VERSION_REQUIRED)
    MESSAGE(FATAL_ERROR
      "ZLIB version must be at least ${MIN_ZLIB_VERSION_REQUIRED}, "
      "found ${ZLIB_VERSION}.\nPlease use -DWITH_ZLIB=bundled")
  ENDIF()
ENDMACRO()
