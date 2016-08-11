# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

#
# Usage:
#
#  cmake -DWITH_PROTOBUF="bundled"|"system"
#
#  Default is "bundled"
#  Other values will be ignored, and we fall back to "bundled"
#

MACRO(RESET_PROTOBUF_VARIABLES)
  UNSET(PROTOBUF_INCLUDE_DIR CACHE)
  UNSET(PROTOBUF_INCLUDE_DIR)
  UNSET(PROTOBUF_INCLUDE_DIRS CACHE)
  UNSET(PROTOBUF_INCLUDE_DIRS)
  UNSET(PROTOBUF_LIBRARIES CACHE)
  UNSET(PROTOBUF_LIBRARIES)
  UNSET(PROTOBUF_LIBRARY CACHE)
  UNSET(PROTOBUF_LIBRARY)
  UNSET(PROTOBUF_LIBRARY_DEBUG CACHE)
  UNSET(PROTOBUF_LIBRARY_DEBUG)
  UNSET(PROTOBUF_LITE_LIBRARY CACHE)
  UNSET(PROTOBUF_LITE_LIBRARY)
  UNSET(PROTOBUF_LITE_LIBRARY_DEBUG CACHE)
  UNSET(PROTOBUF_LITE_LIBRARY_DEBUG)
  UNSET(PROTOBUF_PROTOC_EXECUTABLE CACHE)
  UNSET(PROTOBUF_PROTOC_EXECUTABLE)
  UNSET(PROTOBUF_PROTOC_LIBRARY_DEBUG CACHE)
  UNSET(PROTOBUF_PROTOC_LIBRARY_DEBUG)
ENDMACRO()

MACRO(ECHO_PROTOBUF_VARIABLES)
  MESSAGE(STATUS "PROTOBUF_INCLUDE_DIR ${PROTOBUF_INCLUDE_DIR}")
  MESSAGE(STATUS "PROTOBUF_LIBRARY ${PROTOBUF_LIBRARY}")
  MESSAGE(STATUS "PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_PROTOC_EXECUTABLE}")
ENDMACRO()

MACRO(COULD_NOT_FIND_PROTOBUF)
  ECHO_PROTOBUF_VARIABLES()
  MESSAGE(STATUS "Could not find (the correct version of) protobuf.")
  MESSAGE(STATUS "MySQL currently requires at least protobuf version 2.5")
  MESSAGE(FATAL_ERROR
    "You can build with the bundled sources"
    )
ENDMACRO()

SET(BUNDLED_PROTO_SRCDIR ${CMAKE_SOURCE_DIR}/extra/protobuf/protobuf-2.6.1/src)

MACRO(MYSQL_USE_BUNDLED_PROTOBUF)
  SET(WITH_PROTOBUF "bundled" CACHE INTERNAL
    "Bundled protoc and protobuf library")
  # Set the same variables as FindProtobuf.cmake
  SET(PROTOBUF_FOUND 1 CACHE INTERNAL "")
  SET(PROTOBUF_INCLUDE_DIR ${BUNDLED_PROTO_SRCDIR} CACHE INTERNAL "")
  SET(PROTOBUF_INCLUDE_DIRS ${BUNDLED_PROTO_SRCDIR} CACHE INTERNAL "")
  SET(PROTOBUF_LIBRARY protobuf CACHE INTERNAL "")
  SET(PROTOBUF_LIBRARY_DEBUG protobuf CACHE INTERNAL "")
  SET(PROTOBUF_LIBRARIES protobuf CACHE INTERNAL "")
  SET(PROTOBUF_PROTOC_EXECUTABLE protoc CACHE INTERNAL "")
  SET(PROTOBUF_PROTOC_LIBRARY protoclib CACHE INTERNAL "")
  SET(PROTOBUF_PROTOC_LIBRARY_DEBUG protoclib CACHE INTERNAL "")
  SET(PROTOBUF_LITE_LIBRARY protobuf-lite CACHE INTERNAL "")
  SET(PROTOBUF_LITE_LIBRARY_DEBUG protobuf-lite CACHE INTERNAL "")
  ADD_SUBDIRECTORY(extra/protobuf)
ENDMACRO()

MACRO(MYSQL_CHECK_PROTOBUF)
  IF (NOT WITH_PROTOBUF OR
      NOT WITH_PROTOBUF STREQUAL "system")
    SET(WITH_PROTOBUF "bundled")
  ENDIF()
  MESSAGE(STATUS "WITH_PROTOBUF=${WITH_PROTOBUF}")
  IF(WITH_PROTOBUF STREQUAL "bundled")
    MYSQL_USE_BUNDLED_PROTOBUF()
  ELSE()
    FIND_PACKAGE(Protobuf)
  ENDIF()

  IF(NOT PROTOBUF_FOUND)
    MESSAGE(WARNING "Protobuf could not be found")
  ENDIF()

  IF(PROTOBUF_FOUND)
    # Verify protobuf version number. Version information looks like:
    # // The current version, represented as a single integer to make comparison
    # // easier:  major * 10^6 + minor * 10^3 + micro
    # #define GOOGLE_PROTOBUF_VERSION 2006000
    FILE(STRINGS "${PROTOBUF_INCLUDE_DIR}/google/protobuf/stubs/common.h"
      PROTOBUF_VERSION_NUMBER
      REGEX "^#define[\t ]+GOOGLE_PROTOBUF_VERSION[\t ][0-9]+.*"
      )
    STRING(REGEX REPLACE
      "^.*GOOGLE_PROTOBUF_VERSION[\t ]([0-9])[0-9][0-9]([0-9])[0-9][0-9].*$"
      "\\1"
      PROTOBUF_MAJOR_VERSION "${PROTOBUF_VERSION_NUMBER}")
    STRING(REGEX REPLACE
      "^.*GOOGLE_PROTOBUF_VERSION[\t ]([0-9])[0-9][0-9]([0-9])[0-9][0-9].*$"
      "\\2"
      PROTOBUF_MINOR_VERSION "${PROTOBUF_VERSION_NUMBER}")

    MESSAGE(STATUS
      "protobuf version is ${PROTOBUF_MAJOR_VERSION}.${PROTOBUF_MINOR_VERSION}")

    IF("${PROTOBUF_MAJOR_VERSION}.${PROTOBUF_MINOR_VERSION}" VERSION_LESS "2.5")
      COULD_NOT_FIND_PROTOBUF()
    ENDIF()
  ENDIF()
ENDMACRO()
