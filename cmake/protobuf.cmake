# Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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
# Usage:
#
#  cmake -DWITH_PROTOBUF="bundled"|"system"
#
#  Default is "bundled"
#  Other values will be ignored, and we fall back to "bundled"
#

# Bundled version is currently 3.19.4
# Lowest checked system version is 3.5.0 on Oracle Linux 8.
# Older versions may generate code which breaks the -Werror build.
SET(MIN_PROTOBUF_VERSION_REQUIRED "3.5.0")

MACRO(FIND_PROTOBUF_VERSION)
  # Verify protobuf version number. Version information looks like:
  # // The current version, represented as a single integer to make comparison
  # // easier:  major * 10^6 + minor * 10^3 + micro
  # #define GOOGLE_PROTOBUF_VERSION 3012004
  FILE(STRINGS "${PROTOBUF_INCLUDE_DIR}/google/protobuf/stubs/common.h"
    PROTOBUF_VERSION_NUMBER
    REGEX "^#define[\t ]+GOOGLE_PROTOBUF_VERSION[\t ][0-9]+.*"
    )
  STRING(REGEX MATCH
    ".*VERSION[\t ]([0-9]+).*" V_NUM "${PROTOBUF_VERSION_NUMBER}")

  MATH(EXPR PB_MAJOR_VERSION "${CMAKE_MATCH_1} / 1000000")
  MATH(EXPR MINOR_MICRO "${CMAKE_MATCH_1} - (1000000 * ${PB_MAJOR_VERSION})")
  MATH(EXPR PB_MINOR_VERSION "${MINOR_MICRO} / 1000")
  MATH(EXPR PB_MICRO_VERSION "${MINOR_MICRO} - (1000 * ${PB_MINOR_VERSION})")

  SET(PROTOBUF_VERSION
    "${PB_MAJOR_VERSION}.${PB_MINOR_VERSION}.${PB_MICRO_VERSION}")
  SET(PROTOBUF_VERSION "${PROTOBUF_VERSION}" CACHE INTERNAL
    "PROTOBUF major.minor.micro")
  MESSAGE(STATUS
    "PROTOBUF_VERSION (${WITH_PROTOBUF}) is ${PROTOBUF_VERSION}")
ENDMACRO(FIND_PROTOBUF_VERSION)

MACRO(ECHO_PROTOBUF_VARIABLES)
  MESSAGE(STATUS "PROTOBUF_INCLUDE_DIR ${PROTOBUF_INCLUDE_DIR}")
  MESSAGE(STATUS "PROTOBUF_LIBRARY ${PROTOBUF_LIBRARY}")
  MESSAGE(STATUS "PROTOBUF_LITE_LIBRARY ${PROTOBUF_LITE_LIBRARY}")
  MESSAGE(STATUS "PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_PROTOC_EXECUTABLE}")
ENDMACRO()

MACRO(COULD_NOT_FIND_PROTOBUF)
  ECHO_PROTOBUF_VARIABLES()
  MESSAGE(WARNING
    "Could not find (the correct version of) protobuf.\n"
    "MySQL currently requires at least protobuf "
    "version ${MIN_PROTOBUF_VERSION_REQUIRED}")
  MESSAGE(FATAL_ERROR
    "You can build with the bundled sources"
    )
ENDMACRO()

SET(BUNDLED_PROTO_SRCDIR ${CMAKE_SOURCE_DIR}/extra/protobuf/protobuf-3.19.4/src)

MACRO(MYSQL_USE_BUNDLED_PROTOBUF)
  SET(WITH_PROTOBUF "bundled" CACHE STRING
    "Bundled protoc and protobuf library")
  # Set the same variables as FindProtobuf.cmake (First reset all)
  FOREACH(protovar
      INCLUDE_DIR
      LIBRARY_DEBUG
      LIBRARY_RELEASE
      LITE_LIBRARY_DEBUG
      LITE_LIBRARY_RELEASE
      PROTOC_EXECUTABLE
      PROTOC_LIBRARY_DEBUG
      PROTOC_LIBRARY_RELEASE)
    UNSET(Protobuf_${protovar})
    UNSET(Protobuf_${protovar} CACHE)
  ENDFOREACH()
  UNSET(FIND_PACKAGE_MESSAGE_DETAILS_Protobuf)
  UNSET(FIND_PACKAGE_MESSAGE_DETAILS_Protobuf CACHE)

  SET(PROTOBUF_FOUND 1 CACHE INTERNAL "")
  SET(PROTOBUF_INCLUDE_DIR ${BUNDLED_PROTO_SRCDIR} CACHE INTERNAL "")
  SET(PROTOBUF_INCLUDE_DIRS ${BUNDLED_PROTO_SRCDIR} CACHE INTERNAL "")
  SET(PROTOBUF_LIBRARY libprotobuf CACHE INTERNAL "")
  SET(PROTOBUF_LIBRARY_DEBUG libprotobuf CACHE INTERNAL "")
  SET(PROTOBUF_LIBRARIES protobuf CACHE INTERNAL "")
  SET(PROTOBUF_PROTOC_EXECUTABLE protoc CACHE INTERNAL "")
  SET(PROTOBUF_PROTOC_LIBRARY libprotoc CACHE INTERNAL "")
  SET(PROTOBUF_PROTOC_LIBRARY_DEBUG libprotoc CACHE INTERNAL "")
  SET(PROTOBUF_LITE_LIBRARY libprotobuf-lite CACHE INTERNAL "")
  SET(PROTOBUF_LITE_LIBRARY_DEBUG libprotobuf-lite CACHE INTERNAL "")
  INCLUDE_DIRECTORIES(BEFORE SYSTEM ${BUNDLED_PROTO_SRCDIR})
ENDMACRO()

MACRO(MYSQL_CHECK_PROTOBUF)
  IF (NOT WITH_PROTOBUF OR
      NOT WITH_PROTOBUF STREQUAL "system")
    SET(WITH_PROTOBUF "bundled")
  ENDIF()

  IF(WITH_PROTOBUF STREQUAL "bundled")
    MYSQL_USE_BUNDLED_PROTOBUF()
  ELSE()
    FIND_PACKAGE(Protobuf)
  ENDIF()

  IF(NOT PROTOBUF_FOUND)
    MESSAGE(WARNING "Protobuf libraries/headers could not be found")
  ENDIF()

  IF(NOT PROTOBUF_PROTOC_EXECUTABLE)
    MESSAGE(WARNING "The protoc executable could not be found")
  ENDIF()

  IF(NOT PROTOBUF_PROTOC_LIBRARY)
    MESSAGE(WARNING "The protoc library could not be found")
  ENDIF()

  IF(NOT PROTOBUF_FOUND OR
      NOT PROTOBUF_PROTOC_EXECUTABLE OR
      NOT PROTOBUF_PROTOC_LIBRARY)
    MESSAGE(FATAL_ERROR "Use bundled protobuf, or install missing packages")
  ENDIF()

  FIND_PROTOBUF_VERSION()
  IF("${PROTOBUF_VERSION}" VERSION_LESS "${MIN_PROTOBUF_VERSION_REQUIRED}")
    COULD_NOT_FIND_PROTOBUF()
  ENDIF()
  ECHO_PROTOBUF_VARIABLES()
ENDMACRO()

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/protobuf_proto_compile.cmake)
