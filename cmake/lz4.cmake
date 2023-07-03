# Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

# cmake -DWITH_LZ4=system|bundled
# bundled is the default

MACRO(FIND_LZ4_VERSION)
  FOREACH(version_part
      LZ4_VERSION_MAJOR
      LZ4_VERSION_MINOR
      LZ4_VERSION_RELEASE
      )
    FILE(STRINGS "${LZ4_INCLUDE_DIR}/lz4.h" ${version_part}
      REGEX "^#[\t ]*define[\t ]+${version_part}[\t ]+([0-9]+).*")
    STRING(REGEX REPLACE
      "^.*${version_part}[\t ]+([0-9]+).*" "\\1"
      ${version_part} "${${version_part}}")
  ENDFOREACH()
  SET(LZ4_VERSION
    "${LZ4_VERSION_MAJOR}.${LZ4_VERSION_MINOR}.${LZ4_VERSION_RELEASE}")
  MESSAGE(STATUS "LZ4_VERSION (${WITH_LZ4}) is ${LZ4_VERSION}")
  MESSAGE(STATUS "LZ4_INCLUDE_DIR ${LZ4_INCLUDE_DIR}")
  MESSAGE(STATUS "LZ4_LIBRARY ${LZ4_LIBRARY}")
ENDMACRO()

MACRO (FIND_SYSTEM_LZ4)
  FIND_PATH(LZ4_INCLUDE_DIR
    NAMES lz4frame.h)
  FIND_LIBRARY(LZ4_SYSTEM_LIBRARY
    NAMES lz4)
  IF (LZ4_INCLUDE_DIR AND LZ4_SYSTEM_LIBRARY)
    SET(SYSTEM_LZ4_FOUND 1)
    SET(LZ4_LIBRARY ${LZ4_SYSTEM_LIBRARY})
  ENDIF()
ENDMACRO()

SET(LZ4_VERSION "lz4-1.9.4")
SET(BUNDLED_LZ4_PATH "${CMAKE_SOURCE_DIR}/extra/lz4/${LZ4_VERSION}/lib")

MACRO (MYSQL_USE_BUNDLED_LZ4)
  SET(WITH_LZ4 "bundled" CACHE STRING "Bundled lz4 library")
  SET(BUILD_BUNDLED_LZ4 1)
  INCLUDE_DIRECTORIES(BEFORE SYSTEM ${BUNDLED_LZ4_PATH})
  SET(LZ4_INCLUDE_DIR ${BUNDLED_LZ4_PATH})
  SET(LZ4_LIBRARY lz4_lib)
  ADD_LIBRARY(lz4_lib STATIC
    ${BUNDLED_LZ4_PATH}/lz4.c
    ${BUNDLED_LZ4_PATH}/lz4frame.c
    ${BUNDLED_LZ4_PATH}/lz4hc.c
    ${BUNDLED_LZ4_PATH}/xxhash.c
    )
ENDMACRO()

MACRO (MYSQL_CHECK_LZ4)
  IF(NOT WITH_LZ4)
    SET(WITH_LZ4 "bundled" CACHE STRING "By default use bundled lz4 library")
  ENDIF()

  IF(WITH_LZ4 STREQUAL "bundled")
    MYSQL_USE_BUNDLED_LZ4()
  ELSEIF(WITH_LZ4 STREQUAL "system")
    FIND_SYSTEM_LZ4()
    IF (NOT SYSTEM_LZ4_FOUND)
      MESSAGE(FATAL_ERROR "Cannot find system lz4 libraries.") 
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "WITH_LZ4 must be bundled or system")
  ENDIF()
  FIND_LZ4_VERSION()
ENDMACRO()
