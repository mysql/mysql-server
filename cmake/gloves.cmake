# Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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

################################################################################
#                                                                              #
#  GLOVES_ADD_PLUGIN({core_name} SOURCES {sources})                            #
#  Adds a new MySQL Plugin based on Gloves infrastructure                      #
#                                                                              #
#    Arguments:                                                                #
#      core_name  - name of the plugin to build                                #
#      sources    - source code files of plugin binary                         #
#                                                                              #
#    Example:                                                                  #
#      SET (NAME "new_plugin")                                                 #
#      SET (SOURCES [list_of_plugin_sources])                                  #
#      GLOVES_ADD_PLUGIN (${NAME} SOURCES ${SOURCES})                          #
#                                                                              #
################################################################################

FUNCTION(GLOVES_ADD_PLUGIN CORE_NAME)

  # parse function arguments, and prepare variables
  CMAKE_PARSE_ARGUMENTS(ARG "" "" "SOURCES" ${ARGN})
  STRING(TOUPPER MYSQL_${CORE_NAME} COMPONENT_TAG)

  MYSQL_ADD_PLUGIN(
    ${CORE_NAME} ${ARG_SOURCES}
    LINK_LIBRARIES gloves-plugin
    MODULE_ONLY
  )

  # MYSQL_ADD_PLUGIN may have decided not to build it
  IF(NOT TARGET ${CORE_NAME})
    RETURN()
  ENDIF()

  TARGET_COMPILE_DEFINITIONS(
    ${CORE_NAME} PUBLIC
    MYSQL_SERVER
    PLUGIN_GLOVE
    GLOVE_PFS_CATEGORY="plugin-${CORE_NAME}"
    LOG_COMPONENT_TAG="${COMPONENT_TAG}"
  )

  TARGET_INCLUDE_DIRECTORIES(
    ${CORE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/server
    ${GLOVES_FOLDER}
    ${GLOVES_FOLDER}/tools/services
  )

ENDFUNCTION()


################################################################################
#                                                                              #
#  GLOVES_ADD_UNIT_TESTS({core_name} LIB_SOURCES {sources} TESTS {tests})      #
#  Adds Google Unit tests based on Gloves infrastructure                       #
#                                                                              #
#    Arguments:                                                                #
#      core_name  - name of the feature we're testing                          #
#      sources    - common source code files for unit tests                    #
#      tests      - names of the tests to build                                #
#                                                                              #
#    Note:                                                                     #
#      Test source files need to be in form "name-t.cc"                        #
#      Test source files need to be in the /unit subfolder                     #
#                                                                              #
#    Example:                                                                  #
#      SET (CORE_NAME "new_core")                                              #
#      SET (LIB_SOURCES [list_of_core_sources])                                #
#      SET (TESTS test1 test2 test3)                                           #
#      GLOVES_ADD_UNIT_TESTS (                                                 #
#        ${CORE_NAME}                                                          #
#        LIB_SOURCES ${SOURCES}                                                #
#        TESTS       ${TESTS}                                                  #
#      )                                                                       #
#                                                                              #
################################################################################

FUNCTION(GLOVES_ADD_UNIT_TESTS CORE_NAME)

  # MYSQL_ADD_PLUGIN may have decided not to build it
  IF(NOT TARGET ${CORE_NAME})
    RETURN()
  ENDIF()

  IF(NOT WITH_UNIT_TESTS)
    RETURN()
  ENDIF()

  # parse function arguments
  CMAKE_PARSE_ARGUMENTS(ARG "" "" "LIB_SOURCES;TESTS" ${ARGN})

  # prepare library to reuse among unit tests
  SET(LIBRARY "${CORE_NAME}-unit-test-library")
  ADD_LIBRARY(${LIBRARY} STATIC ${ARG_LIB_SOURCES})
  ADD_DEPENDENCIES(${LIBRARY} GenError)
  ADD_DEPENDENCIES(${LIBRARY} gloves-test)
  TARGET_INCLUDE_DIRECTORIES(
    ${LIBRARY} PUBLIC
    ${GLOVES_FOLDER}
    ${GLOVES_FOLDER}/tools/test
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/test
  )
  TARGET_LINK_LIBRARIES(${LIBRARY} PRIVATE gloves-test)

  # add unit tests
  FOREACH(TEST ${ARG_TESTS})

    # derive target and source name
    SET(TARGET "${CORE_NAME}_${TEST}-t")
    SET(SOURCE "unit/${TEST}-t.cc")

    # add unit test executable
    MYSQL_ADD_EXECUTABLE(
      ${TARGET} ${SOURCE}
      ADD_TEST "${CORE_NAME}_${TEST}"
      LINK_LIBRARIES ${LIBRARY} gtest gmock
    )

    TARGET_INCLUDE_DIRECTORIES(${TARGET} PRIVATE ${GMOCK_INCLUDE_DIRS})

  ENDFOREACH()

ENDFUNCTION()
