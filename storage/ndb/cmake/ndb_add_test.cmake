# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

# Macro to add unit tests

INCLUDE(cmake_parse_arguments)

FUNCTION(NDB_ADD_TEST)
  # Parse arguments passed to ADD_TEST
  MYSQL_PARSE_ARGUMENTS(ARG
    "LIBS"
    ""
    ${ARGN}
  )

  # Check switch WITH_UNIT_TESTS and build only when requested
  IF(NOT WITH_UNIT_TESTS)
    RETURN()
  ENDIF()

  # Extracting the executable from DEFAULT_ARGS
  LIST(GET ARG_DEFAULT_ARGS 0 EXEC)
  LIST(REMOVE_AT ARG_DEFAULT_ARGS 0)
  # Setting the source
  SET(SRC ${ARG_DEFAULT_ARGS})

  # Adding executable for the test
  ADD_EXECUTABLE(${EXEC} ${SRC})
  # Moving all unit test executables to build_dir/unittest/ndb
  SET_TARGET_PROPERTIES(${EXEC} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/unittest/ndb)

  # Linking the libraries
  IF(ARG_LIBS)
    TARGET_LINK_LIBRARIES(${EXEC} ${ARG_LIBS})
  ENDIF()
  # Generating the test name and adding the test to ensure
  # it is picked up by CTest
  string (REPLACE "-t" "" NAME "${EXEC}")
  string (REPLACE "-" "_" FLAG_NAME "${NAME}")
  ADD_TEST(${NAME} "${CMAKE_BINARY_DIR}/unittest/ndb/${EXEC}")

  # Automatically generating flag and setting it
  string (TOUPPER ${FLAG_NAME} TEST_UC)
  SET_TARGET_PROPERTIES(${EXEC}
                      PROPERTIES COMPILE_FLAGS "-DTEST_${TEST_UC}")
  
  # Link the unit test program with mytap(and thus implicitly mysys)
  TARGET_LINK_LIBRARIES(${EXEC} mytap)

ENDFUNCTION()
