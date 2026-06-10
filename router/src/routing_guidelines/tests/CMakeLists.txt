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
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE_DIRECTORIES(
  ${GMOCK_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/tests/
  )

ADD_TEST_FILE(test_parser.cc
  MODULE routing_guidelines
  LIB_DEPENDS routing_guidelines-static
  )

ADD_TEST_FILE(test_classifier.cc
  MODULE routing_guidelines
  EXTRA_SOURCES ../src/routing_simulator.cc
  LIB_DEPENDS routing_guidelines-static;
  )
TARGET_INCLUDE_DIRECTORIES(routertest_routing_guidelines_classifier PRIVATE
  ${MySQLRouter_SOURCE_DIR}/src/harness/include/
  )

FILE(GLOB TEST_FILES RELATIVE
  ${CMAKE_CURRENT_SOURCE_DIR}/data data/simulator/*.json)
COPY_TEST_FILES(${CMAKE_CURRENT_SOURCE_DIR}/data "${TEST_FILES}")

IF(LIBFUZZER_COMPILE_FLAGS)
  MYSQL_ADD_EXECUTABLE(routertest_fuzz_routing_guidelines_parser
    fuzz_parser.cc
    $<TARGET_OBJECTS:routing_guidelines-fuzzed>
    COMPONENT Router
    LINK_LIBRARIES
      harness-library  # string_format
      router_utils     # routing_guidelines_version_from_string
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    SKIP_INSTALL
    )
  TARGET_INCLUDE_DIRECTORIES(routertest_fuzz_routing_guidelines_parser
    PRIVATE $<TARGET_PROPERTY:routing_guidelines-objects,INCLUDE_DIRECTORIES>)

  LIBFUZZER_ADD_TEST(routertest_fuzz_routing_guidelines_parser
    INITIAL_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus_parser)
ENDIF()
