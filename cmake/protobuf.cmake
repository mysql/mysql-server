# Copyright (c) 2015, 2025, Oracle and/or its affiliates.
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

# Bundled version is currently 4.24.4
# Lowest checked system version is 3.5.0 on Oracle Linux 8.
# Older versions may generate code which breaks the -Werror build.
SET(MIN_PROTOBUF_VERSION_REQUIRED "3.5.0")

MACRO(FIND_PROTOBUF_VERSION PROTUBUF_SEARCH_DIRS)
  FIND_PATH(PROTOBUF_VERSION_DIR NAMES google/protobuf/stubs/common.h ${PROTUBUF_SEARCH_DIRS})

  # Verify protobuf version number. Version information looks like:
  # // The current version, represented as a single integer to make comparison
  # // easier:  major * 10^6 + minor * 10^3 + micro
  # #define GOOGLE_PROTOBUF_VERSION 3012004
  FILE(STRINGS "${PROTOBUF_VERSION_DIR}/google/protobuf/stubs/common.h"
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

SET(BUNDLED_GRPC_SRCDIR
  "${CMAKE_SOURCE_DIR}/internal/extra/grpc/grpc-1.60.0")
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

MACRO(MYSQL_USE_PKGCONF_PROTOBUF)
  FIND_PACKAGE(PkgConfig REQUIRED)

  IF(NOT TARGET ext::libprotobuf)
    PKG_CHECK_MODULES(PROTOBUF protobuf REQUIRED)

    IF(BUILD_SHARED_LIBS)
      ADD_LIBRARY(ext::libprotobuf SHARED IMPORTED)
      SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
        INTERFACE_LINK_LIBRARIES "${PROTOBUF_LIBRARIES}")

      FIND_LIBRARY(PROTOBUF_IMPORTED_LOCATION NAMES ${PROTOBUF_LIBRARIES} REQUIRED)
      SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
        IMPORTED_LOCATION "${PROTOBUF_IMPORTED_LOCATION}")
    ELSE()
      ADD_LIBRARY(ext::libprotobuf STATIC IMPORTED)
      SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
        INTERFACE_LINK_LIBRARIES "${PROTOBUF_STATIC_LIBRARIES}")

      FIND_LIBRARY(PROTOBUF_IMPORTED_LOCATION NAMES ${PROTOBUF_STATIC_LIBRARIES} REQUIRED)
      SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
        IMPORTED_LOCATION "${PROTOBUF_IMPORTED_LOCATION}")
    ENDIF()

    SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${PROTOBUF_INCLUDE_DIRS}")

    SET(PROTOBUF_INCLUDE_DIR "${PROTOBUF_INCLUDE_DIRS}" CACHE INTERNAL "")
  ENDIF()

  IF(NOT TARGET ext::libprotobuf-lite)
    PKG_CHECK_MODULES(PROTOBUF_LITE protobuf-lite REQUIRED)

    IF(BUILD_SHARED_LIBS)
      ADD_LIBRARY(ext::libprotobuf-lite SHARED IMPORTED)
      SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
        INTERFACE_LINK_LIBRARIES "${PROTOBUF_LITE_LIBRARIES}")

      FIND_LIBRARY(PROTOBUF_LITE_IMPORTED_LOCATION NAMES ${PROTOBUF_LITE_LIBRARIES} REQUIRED)
      SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
        IMPORTED_LOCATION "${PROTOBUF_LITE_IMPORTED_LOCATION}")
    ELSE()
      ADD_LIBRARY(ext::libprotobuf-lite STATIC IMPORTED)
      SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
        INTERFACE_LINK_LIBRARIES "${PROTOBUF_LITE_STATIC_LIBRARIES}")

      FIND_LIBRARY(PROTOBUF_LITE_IMPORTED_LOCATION NAMES ${PROTOBUF_LITE_STATIC_LIBRARIES} REQUIRED)
      SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
        IMPORTED_LOCATION "${PROTOBUF_LITE_IMPORTED_LOCATION}")
    ENDIF()

    SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${PROTOBUF_LITE_INCLUDE_DIRS}")
  ENDIF()

  IF(NOT TARGET ext::libprotoc)
    IF(BUILD_SHARED_LIBS)
      ADD_LIBRARY(ext::libprotoc SHARED IMPORTED)
    ELSE()
      ADD_LIBRARY(ext::libprotoc STATIC IMPORTED)
    ENDIF()

    FIND_PROGRAM(PROTOC_EXECUTABLE protoc REQUIRED)
    SET(PROTOBUF_PROTOC_EXECUTABLE "${PROTOC_EXECUTABLE}")
    SET(PROTOBUF_PROTOC_EXECUTABLE "${PROTOBUF_PROTOC_EXECUTABLE}" CACHE INTERNAL "")

    PKG_CHECK_MODULES(PROTOC protoc)
    IF(PROTOC_FOUND)
      IF(BUILD_SHARED_LIBS)
        SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
          INTERFACE_LINK_LIBRARIES "${PROTOC_LIBRARIES}")
        SET(PROTOBUF_PROTOC_LIBRARY "${PROTOC_LIBRARIES}")
        SET(PROTOBUF_PROTOC_LIBRARY "${PROTOBUF_PROTOC_LIBRARY}" CACHE INTERNAL "")

        FIND_LIBRARY(PROTOC_IMPORTED_LOCATION NAMES ${PROTOC_LIBRARIES} REQUIRED)
        SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
          IMPORTED_LOCATION "${PROTOC_IMPORTED_LOCATION}")
      ELSE()
        SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
          INTERFACE_LINK_LIBRARIES "${PROTOC_STATIC_LIBRARIES}")
        SET(PROTOBUF_PROTOC_LIBRARY "${PROTOC_STATIC_LIBRARIES}")
        SET(PROTOBUF_PROTOC_LIBRARY "${PROTOBUF_PROTOC_LIBRARY}" CACHE INTERNAL "")

        FIND_LIBRARY(PROTOC_IMPORTED_LOCATION NAMES ${PROTOC_STATIC_LIBRARIES} REQUIRED)
        SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
          IMPORTED_LOCATION "${PROTOC_IMPORTED_LOCATION}")
      ENDIF()

      SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${PROTOC_INCLUDE_DIRS}")
    ELSE()
      PKG_CHECK_MODULES(PROTOBUF protobuf REQUIRED)

      FIND_LIBRARY(PROTOBUF_PROTOC_LIBRARY protoc REQUIRED)

      SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
        INTERFACE_LINK_LIBRARIES "${PROTOBUF_PROTOC_LIBRARY}")

      SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${PROTOBUF_INCLUDE_DIRS}")

      SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
        IMPORTED_LOCATION "${PROTOBUF_PROTOC_LIBRARY}")
    ENDIF()
  ENDIF()
ENDMACRO(MYSQL_USE_PKGCONF_PROTOBUF)

MACRO(MYSQL_CHECK_PROTOBUF)
  IF (NOT WITH_PROTOBUF OR
      NOT (WITH_PROTOBUF STREQUAL "pkgconf" OR WITH_PROTOBUF STREQUAL "system"))
    SET(WITH_PROTOBUF "bundled")
  ENDIF()

  IF(WITH_PROTOBUF STREQUAL "bundled")
    MYSQL_USE_BUNDLED_PROTOBUF()
  ELSEIF(WITH_PROTOBUF STREQUAL "pkgconf")
    MYSQL_USE_PKGCONF_PROTOBUF()
  ELSE()
    # In case we want grpc, it must be loaded *before* Protobuf,
    # otherwise we get
    # CMake Error at ..../cmake/protobuf/protobuf-targets.cmake:42 (message):
    #   "some (but not all) targets in this export set were already defined".
    IF(WITH_INTERNAL)
      FIND_PACKAGE(gRPC QUIET)
    ENDIF()
    FIND_PACKAGE(Protobuf)
  ENDIF()

  IF(NOT PROTOBUF_FOUND)
    MESSAGE(WARNING "Protobuf libraries/headers could not be found")
  ENDIF()

  IF(NOT PROTOBUF_PROTOC_EXECUTABLE)
    MESSAGE(WARNING "The protoc executable could not be found")
  ENDIF()

  IF(NOT PROTOBUF_PROTOC_LIBRARY AND
      (WITH_PROTOBUF STREQUAL "pkgconf" OR WITH_PROTOBUF STREQUAL "system"))
    MESSAGE(WARNING "The protoc library could not be found")
  ENDIF()

  IF(NOT PROTOBUF_FOUND OR
      NOT PROTOBUF_PROTOC_EXECUTABLE OR
      (NOT PROTOBUF_PROTOC_LIBRARY AND
        (WITH_PROTOBUF STREQUAL "pkgconf" OR WITH_PROTOBUF STREQUAL "system")))
    MESSAGE(FATAL_ERROR "Use bundled protobuf, or install missing packages")
  ENDIF()

  IF(WITH_PROTOBUF STREQUAL "bundled")
    # Do this after add_library in extra/protobuf cmake code:
    # ADD_LIBRARY(ext::libprotobuf ALIAS libprotobuf)
    # ADD_LIBRARY(ext::libprotobuf-lite ALIAS libprotobuf-lite)
    # ADD_LIBRARY(ext::libprotoc ALIAS libprotoc)
  ELSEIF(NOT WITH_PROTOBUF STREQUAL "pkgconf")
    # We cannot use the IMPORTED libraries defined by FIND_PACKAGE above,
    # protobuf::libprotobuf may have INTERFACE properties like -std=gnu++11
    # and that will break the build since we use -std=c++20
    # <cmake source root>/Modules/FindProtobuf.cmake may do:
    # set_property(TARGET protobuf::libprotobuf APPEND PROPERTY
    #              INTERFACE_COMPILE_FEATURES cxx_std_11
    #             )
    # INTERFACE_LINK_LIBRARIES will be needed once this is built
    # with protobuf 22 and above (lots of abseil libs).
    IF(NOT TARGET ext::libprotobuf)
      ADD_LIBRARY(ext::libprotobuf UNKNOWN IMPORTED)
    ENDIF()
    SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIR}")
    SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
      IMPORTED_LOCATION "${PROTOBUF_LIBRARY}")
    IF(LINUX)
      FIND_LIBRARY_DEPENDENCIES("${PROTOBUF_LIBRARY}" protobuf_dependencies)
      SET_TARGET_PROPERTIES(ext::libprotobuf PROPERTIES
        INTERFACE_LINK_LIBRARIES "${protobuf_dependencies}")
    ENDIF()

    IF(NOT TARGET ext::libprotobuf-lite)
      ADD_LIBRARY(ext::libprotobuf-lite UNKNOWN IMPORTED)
    ENDIF()
    SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIR}")
    SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
      IMPORTED_LOCATION "${PROTOBUF_LITE_LIBRARY}")
    IF(LINUX)
      FIND_LIBRARY_DEPENDENCIES("${PROTOBUF_LITE_LIBRARY}" lite_dependencies)
      SET_TARGET_PROPERTIES(ext::libprotobuf-lite PROPERTIES
        INTERFACE_LINK_LIBRARIES "${lite_dependencies}")
    ENDIF()

    IF(NOT TARGET ext::libprotoc)
      ADD_LIBRARY(ext::libprotoc UNKNOWN IMPORTED)
    ENDIF()
    SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIR}")
    SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
      IMPORTED_LOCATION "${Protobuf_PROTOC_LIBRARY}")
    IF(LINUX)
      FIND_LIBRARY_DEPENDENCIES(
        "${Protobuf_PROTOC_LIBRARY}" protoc_dependencies)
      SET_TARGET_PROPERTIES(ext::libprotoc PROPERTIES
        INTERFACE_LINK_LIBRARIES "${protoc_dependencies}")
    ENDIF()

    FIND_PROTOBUF_VERSION(${PROTOBUF_INCLUDE_DIR})
  ENDIF()

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
