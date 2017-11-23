# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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
  # - built in the default RUNTIME_OUTPUT_DIRECTORY
  # - adds test to be run by ctest
  # - skips install ot the unittest binary
  MYSQL_ADD_EXECUTABLE(${EXEC} ${SRC} ADD_TEST ${EXEC})

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
