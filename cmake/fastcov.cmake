# Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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

# Targets below assume we have gcc and gcov version >= 9

# There is no coverage for the NDBCLUSTER plugin, so disable it.
# Alternatively: use -DWITH_NDB=1, and run cluster test suites also.

# The default mtr test suite has limited coverage of replication,
# and of some plugins.

# cmake <path> -DWITH_DEBUG=1 -DWITH_SYSTEM_LIBS=1 -DENABLE_GCOV=1
#              -DWITH_NDBCLUSTER_STORAGE_ENGINE=0
# make
# make fastcov-clean
# <run some tests>
# make fastcov-report
# make fastcov-html
# open in browser:  ${CMAKE_BINARY_DIR}/code_coverage/index.html

FIND_PROGRAM(FASTCOV_EXECUTABLE NAMES fastcov.py fastcov)

IF(NOT FASTCOV_EXECUTABLE)
  MESSAGE(WARNING "Could not find fastcov.py or fastcov")
  RETURN()
ENDIF()

IF(NOT CMAKE_COMPILER_IS_GNUCXX)
  MESSAGE(WARNING "You should upgrade to gcc version >= 10")
  RETURN()
ENDIF()

IF(ALTERNATIVE_GCC)
  GET_FILENAME_COMPONENT(GCC_B_PREFIX ${ALTERNATIVE_GCC} DIRECTORY)
  MESSAGE(STATUS "Looking for gcov in ${GCC_B_PREFIX}")
  FIND_PROGRAM(GCOV_EXECUTABLE gcov
    NO_DEFAULT_PATH
    PATHS "${GCC_B_PREFIX}")
  # Ensure that fastcov can find tools in PATH.
  IF(GCOV_EXECUTABLE)
    SET(FASTCOV_PATH_PREFIX
      ${CMAKE_COMMAND} -E env "PATH=${GCC_B_PREFIX}:$ENV{PATH}"
      )
  ENDIF()
ENDIF()

FIND_PROGRAM(GCOV_EXECUTABLE NAMES gcov)
IF(NOT GCOV_EXECUTABLE)
  MESSAGE(FATAL_ERROR "gcov not found")
ENDIF()

EXECUTE_PROCESS(
  COMMAND ${GCOV_EXECUTABLE} --version
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE  stderr
  RESULT_VARIABLE result
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# gcov --version output samples on Linux:
# gcov (Debian 9-20190208-1) 9.0.1 20190208 (experimental)
# gcov (GCC) 8.3.1 20190223 (Red Hat 8.3.1-2)
STRING(REPLACE "\n" ";" GCOV_OUTPUT_LIST "${stdout}")
UNSET(GCOV_VERSION)
LIST(GET GCOV_OUTPUT_LIST 0 FIRST_LINE)
STRING(REGEX MATCH "gcov [(].*[)] ([0-9\.]+).*" XXX ${FIRST_LINE})
IF(CMAKE_MATCH_1)
  SET(GCOV_VERSION "${CMAKE_MATCH_1}")
ENDIF()

IF(GCOV_VERSION AND GCOV_VERSION VERSION_LESS 10)
  MESSAGE(FATAL_ERROR "${GCOV_EXECUTABLE} has version ${GCOV_VERSION}\n"
    "At least version 10 is required")
ENDIF()

IF(WITH_ROUTER)
  # extra/duktape/duktape-2.7.0/src/duktape.c has ~140 #line directives.
  # Just create some empty files, to make fastcov happy.
  SET(DUKTAPE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/extra/duktape/duktape-2.7.0)
  EXECUTE_PROCESS(COMMAND
    grep "\#line [12] " ${DUKTAPE_SOURCE_DIR}/src/duktape.c
    OUTPUT_VARIABLE DUKTAPE_LINES
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  STRING(REPLACE "\n" ";" DUKTAPE_LINES "${DUKTAPE_LINES}")
  FOREACH(LINE ${DUKTAPE_LINES})
    STRING(REGEX MATCH "#line [12] \"(.*)\"" XXX ${LINE})
    SET(DUK_SOURCE_FILE ${CMAKE_MATCH_1})
    IF(CMAKE_GENERATOR MATCHES "Ninja")
      FILE(WRITE ${CMAKE_BINARY_DIR}/${DUK_SOURCE_FILE} "")
    ELSE()
      FILE(WRITE
        ${CMAKE_BINARY_DIR}/router/src/mock_server/src/${DUK_SOURCE_FILE} "")
    ENDIF()
  ENDFOREACH()
ENDIF(WITH_ROUTER)

# We may be running gcov in-source.
IF(NOT THIS_IS_AN_IN_SOURCE_BUILD)
  FOREACH(FILE
      # InnoDB generated parsers are checked in as source.
      ${CMAKE_SOURCE_DIR}/storage/innobase/fts/fts0blex.cc
      ${CMAKE_SOURCE_DIR}/storage/innobase/fts/fts0blex.l
      ${CMAKE_SOURCE_DIR}/storage/innobase/fts/fts0pars.cc
      ${CMAKE_SOURCE_DIR}/storage/innobase/fts/fts0pars.y
      ${CMAKE_SOURCE_DIR}/storage/innobase/fts/fts0tlex.cc
      ${CMAKE_SOURCE_DIR}/storage/innobase/fts/fts0tlex.l
      ${CMAKE_SOURCE_DIR}/storage/innobase/pars/lexyy.cc
      ${CMAKE_SOURCE_DIR}/storage/innobase/pars/pars0grm.cc
      ${CMAKE_SOURCE_DIR}/storage/innobase/pars/pars0grm.y
      ${CMAKE_SOURCE_DIR}/storage/innobase/pars/pars0lex.l
      )
    GET_FILENAME_COMPONENT(filename "${FILE}" NAME)
    IF(CMAKE_GENERATOR MATCHES "Ninja")
      EXECUTE_PROCESS(
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${FILE} ${filename}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    ELSE()
      EXECUTE_PROCESS(
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${FILE} ${filename}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/storage/innobase
        )
    ENDIF()
  ENDFOREACH()
ENDIF()

# Ignore std, boost and 3rd-party code when doing coverage analysis.
SET(FASTCOV_EXCLUDE_LIST "--exclude")
FOREACH(FASTCOV_EXCLUDE
    "/usr/include"
    "/usr/lib"
    "${BOOST_INCLUDE_DIR}"
    ${GMOCK_INCLUDE_DIRS}
    "${CMAKE_SOURCE_DIR}/extra/duktape"
    "${CMAKE_SOURCE_DIR}/extra/lz4"
    "${CMAKE_SOURCE_DIR}/extra/rapidjson"
    )
  LIST(APPEND FASTCOV_EXCLUDE_LIST "${FASTCOV_EXCLUDE}")
ENDFOREACH()

ADD_CUSTOM_TARGET(fastcov-clean
  COMMAND ${FASTCOV_PATH_PREFIX}
          ${FASTCOV_EXECUTABLE} --gcov ${GCOV_EXECUTABLE} --zerocounters
  COMMENT "Running ${FASTCOV_EXECUTABLE} --zerocounters"
  VERBATIM
  )
ADD_CUSTOM_TARGET(fastcov-report
  COMMAND ${FASTCOV_PATH_PREFIX}
          ${FASTCOV_EXECUTABLE} --gcov ${GCOV_EXECUTABLE}
          ${FASTCOV_EXCLUDE_LIST} --lcov -o report.info
  COMMENT "Running ${FASTCOV_EXECUTABLE} --lcov -o report.info"
  VERBATIM
  )
ADD_CUSTOM_TARGET(fastcov-html
  COMMAND genhtml -o code_coverage report.info
  COMMENT "Running genhtml -o code_coverage report.info"
  VERBATIM
  )
