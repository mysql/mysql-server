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

# Macro to add unit tests

# NDB_ADD_TEST(EXEC sources... LIBS libraries...)

FUNCTION(NDB_ADD_TEST EXEC_ARG)
  # Parse arguments passed to ADD_TEST
  CMAKE_PARSE_ARGUMENTS(ARG
    ""
    ""
    "LIBS"
    ${ARGN}
  )

  # Check switch WITH_UNIT_TESTS and build only when requested
  IF(NOT WITH_UNIT_TESTS)
    RETURN()
  ENDIF()

  SET(EXEC ${EXEC_ARG})
  SET(SRC ${ARG_UNPARSED_ARGUMENTS})

  # Adding executable for the test
  # - built in the default RUNTIME_OUTPUT_DIRECTORY
  # - adds test to be run by ctest
  # - skips install ot the unittest binary
  MYSQL_ADD_EXECUTABLE(${EXEC} ${SRC}
    ADD_TEST ${EXEC}
    ENABLE_EXPORTS
    )
  SET_TESTS_PROPERTIES(${EXEC} PROPERTIES LABELS "NDB")

  # Add additional libraries
  IF(ARG_LIBS)
    TARGET_LINK_LIBRARIES(${EXEC} ${ARG_LIBS})
  ENDIF()

  # Automatically generating -DTEST_<name> define for including
  # the unit test code
  string (REPLACE "-t" "" NAME "${EXEC}")
  string (REPLACE "-" "_" FLAG_NAME "${NAME}")
  string (TOUPPER ${FLAG_NAME} FLAG_UC)
  SET_TARGET_PROPERTIES(${EXEC}
    PROPERTIES COMPILE_FLAGS "-DTEST_${FLAG_UC}")
  
  # Link the unit test program with mytap(and thus implicitly mysys)
  TARGET_LINK_LIBRARIES(${EXEC} mytap)

ENDFUNCTION()
