# Copyright (c) 2019, 2022, Oracle and/or its affiliates.
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

# cmake -DWITH_ZSTD=system|bundled
# bundled is the default

# With earier versions, several compression tests fail.
# With version < 1.0.0 our source code does not build.
SET(MIN_ZSTD_VERSION_REQUIRED "1.2.0")

MACRO (FIND_ZSTD_VERSION)
  FOREACH(version_part
      ZSTD_VERSION_MAJOR
      ZSTD_VERSION_MINOR
      ZSTD_VERSION_RELEASE
      )
    FILE(STRINGS "${ZSTD_INCLUDE_DIR}/zstd.h" ${version_part}
      REGEX "^#[\t ]*define[\t ]+${version_part}[\t ]+([0-9]+).*")
    STRING(REGEX REPLACE
      "^.*${version_part}[\t ]+([0-9]+).*" "\\1"
      ${version_part} "${${version_part}}")
  ENDFOREACH()
  SET(ZSTD_VERSION
    "${ZSTD_VERSION_MAJOR}.${ZSTD_VERSION_MINOR}.${ZSTD_VERSION_RELEASE}")
  SET(ZSTD_VERSION "${ZSTD_VERSION}" CACHE INTERNAL "ZSTD major.minor.step")
  MESSAGE(STATUS "ZSTD_VERSION (${WITH_ZSTD}) is ${ZSTD_VERSION}")
  MESSAGE(STATUS "ZSTD_INCLUDE_DIR ${ZSTD_INCLUDE_DIR}")
  MESSAGE(STATUS "ZSTD_LIBRARY ${ZSTD_LIBRARY}")
ENDMACRO()

MACRO (FIND_SYSTEM_ZSTD)
  FIND_PATH(ZSTD_INCLUDE_DIR
    NAMES zstd.h
    PATH_SUFFIXES include)
  FIND_LIBRARY(ZSTD_SYSTEM_LIBRARY
    NAMES zstd
    PATH_SUFFIXES lib)
  IF (ZSTD_INCLUDE_DIR AND ZSTD_SYSTEM_LIBRARY)
    SET(SYSTEM_ZSTD_FOUND 1)
    SET(ZSTD_LIBRARY ${ZSTD_SYSTEM_LIBRARY})
    IF(NOT ZSTD_INCLUDE_DIR STREQUAL "/usr/include")
      # In case of -DCMAKE_PREFIX_PATH=</path/to/custom/zstd>
      INCLUDE_DIRECTORIES(BEFORE SYSTEM ${ZSTD_INCLUDE_DIR})
    ENDIF()
  ENDIF()
ENDMACRO()

SET(ZSTD_VERSION_DIR "zstd-1.5.0")
SET(BUNDLED_ZSTD_PATH ${CMAKE_SOURCE_DIR}/extra/zstd/${ZSTD_VERSION_DIR}/lib)

MACRO (MYSQL_USE_BUNDLED_ZSTD)
  SET(ZSTD_LIBRARY zstd CACHE INTERNAL "Bundled zlib library")
  SET(WITH_ZSTD "bundled" CACHE STRING "Use bundled zstd")
  SET(ZSTD_INCLUDE_DIR ${BUNDLED_ZSTD_PATH})
  INCLUDE_DIRECTORIES(BEFORE SYSTEM ${ZSTD_INCLUDE_DIR})
  ADD_SUBDIRECTORY(extra/zstd)
ENDMACRO()

MACRO (MYSQL_CHECK_ZSTD)
  IF(NOT WITH_ZSTD)
    SET(WITH_ZSTD "bundled" CACHE STRING "By default use bundled zstd library")
  ENDIF()

  IF(WITH_ZSTD STREQUAL "bundled")
    MYSQL_USE_BUNDLED_ZSTD()
  ELSEIF(WITH_ZSTD STREQUAL "system")
    FIND_SYSTEM_ZSTD()
    IF (NOT SYSTEM_ZSTD_FOUND)
      MESSAGE(FATAL_ERROR "Cannot find system zstd libraries.")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "WITH_ZSTD must be bundled or system")
  ENDIF()
  FIND_ZSTD_VERSION()
  IF(ZSTD_VERSION VERSION_LESS MIN_ZSTD_VERSION_REQUIRED)
    MESSAGE(FATAL_ERROR
      "ZSTD version must be at least ${MIN_ZSTD_VERSION_REQUIRED}, "
      "found ${ZSTD_VERSION}.\nPlease use -DWITH_ZSTD=bundled")
  ENDIF()
ENDMACRO()
