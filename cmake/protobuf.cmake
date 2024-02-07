# Copyright (c) 2015, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
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

SET(BUNDLED_PROTO_SRCDIR ${CMAKE_SOURCE_DIR}/extra/protobuf/protobuf-24.4/src)
SET(BUNDLED_ABSEIL_SRCDIR ${CMAKE_SOURCE_DIR}/extra/abseil/abseil-cpp-20230802.1)

MACRO(MYSQL_USE_BUNDLED_PROTOBUF)
  SET(WITH_PROTOBUF "bundled" CACHE STRING
    "Bundled protoc and protobuf library")
  # Reset variables set by FindProtobuf.cmake
  FOREACH(protovar
      INCLUDE_DIR
      LIBRARY
      LIBRARY_DEBUG
      LIBRARY_RELEASE
      LITE_LIBRARY
      LITE_LIBRARY_DEBUG
      LITE_LIBRARY_RELEASE
      PROTOC_EXECUTABLE
      PROTOC_LIBRARY
      PROTOC_LIBRARY_DEBUG
      PROTOC_LIBRARY_RELEASE)
    UNSET(Protobuf_${protovar})
    UNSET(Protobuf_${protovar} CACHE)
    UNSET(PROTOBUF_${protovar})
    UNSET(PROTOBUF_${protovar} CACHE)
  ENDFOREACH()
  UNSET(FIND_PACKAGE_MESSAGE_DETAILS_Protobuf)
  UNSET(FIND_PACKAGE_MESSAGE_DETAILS_Protobuf CACHE)

  # Do not set PROTOBUF_LIBRARY et.al., all binaries should link with
  # ext::protobuf  ext::protobuf-lite  ext::libprotoc
  SET(PROTOBUF_FOUND 1 CACHE INTERNAL "")
  SET(PROTOBUF_INCLUDE_DIR ${BUNDLED_PROTO_SRCDIR} CACHE INTERNAL "")
  SET(PROTOBUF_INCLUDE_DIRS ${BUNDLED_PROTO_SRCDIR} CACHE INTERNAL "")
  SET(PROTOBUF_PROTOC_EXECUTABLE protoc CACHE INTERNAL "")
  INCLUDE_DIRECTORIES(BEFORE SYSTEM ${BUNDLED_PROTO_SRCDIR})
ENDMACRO(MYSQL_USE_BUNDLED_PROTOBUF)

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

  IF(WITH_PROTOBUF STREQUAL "system" AND NOT PROTOBUF_PROTOC_LIBRARY)
    MESSAGE(WARNING "The protoc library could not be found")
  ENDIF()

  IF(NOT PROTOBUF_FOUND OR
      NOT PROTOBUF_PROTOC_EXECUTABLE OR
      (WITH_PROTOBUF STREQUAL "system" AND NOT PROTOBUF_PROTOC_LIBRARY))
    MESSAGE(FATAL_ERROR "Use bundled protobuf, or install missing packages")
  ENDIF()

  IF(WITH_PROTOBUF STREQUAL "bundled")
    # Do this after add_library in extra/protobuf cmake code:
    # ADD_LIBRARY(ext::libprotobuf ALIAS libprotobuf)
    # ADD_LIBRARY(ext::libprotobuf-lite ALIAS libprotobuf-lite)
    # ADD_LIBRARY(ext::libprotoc ALIAS libprotoc)
  ELSE()
    # We cannot use the IMPORTED libraries defined by FIND_PACKAGE above,
    # protobuf::libprotobuf may have INTERFACE properties like -std=gnu++11
    # and that will break the build since we use -std=c++20
    # <cmake source root>/Modules/FindProtobuf.cmake may do:
    # set_property(TARGET protobuf::libprotobuf APPEND PROPERTY
    #              INTERFACE_COMPILE_FEATURES cxx_std_11
    #             )
    ADD_LIBRARY(ext::libprotobuf UNKNOWN IMPORTED)
    SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIR}")
    SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
      IMPORTED_LOCATION "${PROTOBUF_LIBRARY}")

    ADD_LIBRARY(ext::libprotobuf-lite UNKNOWN IMPORTED)
    SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIR}")
    SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
      IMPORTED_LOCATION "${PROTOBUF_LITE_LIBRARY}")

    ADD_LIBRARY(ext::libprotoc UNKNOWN IMPORTED)
    SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIR}")
    SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
      IMPORTED_LOCATION "${Protobuf_PROTOC_LIBRARY}")
  ENDIF()

  FIND_PROTOBUF_VERSION()

  # Version 22 and up depend on ~65 abseil .dylibs.
  IF(APPLE AND WITH_PROTOBUF STREQUAL "system" AND
      PB_MINOR_VERSION VERSION_GREATER 21)
    # list(FILTER <list> {INCLUDE | EXCLUDE} REGEX <regex>)
    FIND_OBJECT_DEPENDENCIES("${PROTOBUF_LIBRARY}" protobuf_dependencies)
    LIST(FILTER protobuf_dependencies INCLUDE REGEX "${HOMEBREW_HOME}.*")
    SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
      INTERFACE_LINK_LIBRARIES "${protobuf_dependencies}"
      )
    FIND_OBJECT_DEPENDENCIES("${PROTOBUF_LITE_LIBRARY}" lite_dependencies)
    LIST(FILTER lite_dependencies  INCLUDE REGEX "${HOMEBREW_HOME}.*")
    SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
      INTERFACE_LINK_LIBRARIES "${lite_dependencies}"
      )
    FIND_OBJECT_DEPENDENCIES("${Protobuf_PROTOC_LIBRARY}" protoc_dependencies)
    LIST(FILTER protoc_dependencies INCLUDE REGEX "${HOMEBREW_HOME}.*")
    SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
      INTERFACE_LINK_LIBRARIES "${protoc_dependencies}"
      )
  ENDIF()

  IF("${PROTOBUF_VERSION}" VERSION_LESS "${MIN_PROTOBUF_VERSION_REQUIRED}")
    COULD_NOT_FIND_PROTOBUF()
  ENDIF()
  ECHO_PROTOBUF_VARIABLES()
ENDMACRO()

INCLUDE(${CMAKE_SOURCE_DIR}/cmake/protobuf_proto_compile.cmake)
