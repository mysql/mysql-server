# Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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
# This program is distributed in the hope that it will be useful,  but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
# the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

INCLUDE(GenerateExportHeader)
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/bison.cmake)

FIND_PACKAGE(BISON REQUIRED 2.3)

BISON_TARGET(match_parser
  parser.yy
  ${CMAKE_CURRENT_BINARY_DIR}/parser.cc
  COMPILE_FLAGS "-t --no-lines ${BISON_FLAGS_WARNINGS}"
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h
  VERBOSE
  )

# Needed for some Bison versions.
MY_CHECK_CXX_COMPILER_WARNING("-Wimplicit-fallthrough" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(${BISON_match_parser_OUTPUT_SOURCE}
    COMPILE_FLAGS "${HAS_WARN_FLAG}")
ENDIF()

SET(guidelines_SRC
  ${BISON_match_parser_OUTPUT_SOURCE}
  rules_parser.cc
  rpn.cc
  routing_guidelines.cc
  utils.cc
  )

ADD_LIBRARY(routing_guidelines-objects OBJECT ${guidelines_SRC})
TARGET_LINK_LIBRARIES(routing_guidelines-objects
  PUBLIC harness_stdx router_utils
  PRIVATE extra::rapidjson)
# Needed so items tagged with ROUTING_GUIDELINES_EXPORT
# (to be exposed by the shared version of the library)
# are not skipped during build
TARGET_COMPILE_DEFINITIONS(routing_guidelines-objects
  PUBLIC routing_guidelines_EXPORTS)
TARGET_INCLUDE_DIRECTORIES(routing_guidelines-objects PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/
  ${CMAKE_CURRENT_BINARY_DIR}/../include/
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR})

# Converting an OBJECT library to STATIC or SHARED does not work for Xcode,
# use the OBJECT library everywhere.
# We don't support creating packages from Xcode projects anyways,
# so we don't need the INSTALL stuff below.
# See also ADD_CONVENIENCE_LIBRARY and MERGE_CONVENIENCE_LIBRARIES.
IF(APPLE_XCODE)
  ADD_LIBRARY(routing_guidelines ALIAS routing_guidelines-objects)
  ADD_LIBRARY(routing_guidelines-static ALIAS routing_guidelines-objects)
  GENERATE_EXPORT_HEADER(routing_guidelines
    EXPORT_FILE_NAME
    ${CMAKE_CURRENT_BINARY_DIR}/../include/mysqlrouter/routing_guidelines_export.h)
  RETURN()
ENDIF()

IF(LIBFUZZER_COMPILE_FLAGS)
  ADD_LIBRARY(routing_guidelines-fuzzed OBJECT ${guidelines_SRC})
  # Build it only if needed by other targets.
  SET_PROPERTY(TARGET routing_guidelines-fuzzed PROPERTY EXCLUDE_FROM_ALL TRUE)
  TARGET_LINK_LIBRARIES(routing_guidelines-fuzzed
    $<TARGET_PROPERTY:routing_guidelines-objects,LINK_LIBRARIES>)
  TARGET_INCLUDE_DIRECTORIES(routing_guidelines-fuzzed PUBLIC
    $<TARGET_PROPERTY:routing_guidelines-objects,INCLUDE_DIRECTORIES>)
  TARGET_COMPILE_OPTIONS(routing_guidelines-fuzzed
    PUBLIC "${LIBFUZZER_COMPILE_FLAGS}")
ENDIF()

ROUTER_ADD_SHARED_LIBRARY(routing_guidelines
  $<TARGET_OBJECTS:routing_guidelines-objects>

  OUTPUT_NAME "mysqlrouter_routing_guidelines"

  LINK_LIBRARIES
  PUBLIC
  harness-library
  router_utils
  )

# static library for tests
ADD_LIBRARY(routing_guidelines-static STATIC
  $<TARGET_OBJECTS:routing_guidelines-objects>)
TARGET_INCLUDE_DIRECTORIES(routing_guidelines-static PUBLIC
  $<TARGET_PROPERTY:routing_guidelines-objects,INCLUDE_DIRECTORIES>
  )
TARGET_COMPILE_OPTIONS(routing_guidelines-static PUBLIC
  $<TARGET_PROPERTY:routing_guidelines-objects,COMPILE_OPTIONS>
  )
TARGET_COMPILE_DEFINITIONS(routing_guidelines-static PUBLIC
  $<TARGET_PROPERTY:routing_guidelines-objects,COMPILE_DEFINITIONS>
  )
