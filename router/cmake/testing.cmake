# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

SET(_TEST_RUNTIME_DIR ${PROJECT_BINARY_DIR}/tests)

MACRO(ROUTERTEST_GET_TARGET OUTVAR FIL MODUL)
  GET_FILENAME_COMPONENT(test_target ${FIL} NAME_WE)
  STRING(REGEX REPLACE "^test_" "" test_target ${test_target})
  SET(${OUTVAR} "routertest_${MODUL}_${test_target}")
ENDMACRO()

FUNCTION(add_test_file FILE)
  SET(one_value_args MODULE LABEL ENVIRONMENT)
  SET(multi_value_args LIB_DEPENDS INCLUDE_DIRS SYSTEM_INCLUDE_DIRS DEPENDS EXTRA_SOURCES)
  cmake_parse_arguments(TEST "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  IF(NOT TEST_MODULE)
    MESSAGE(FATAL_ERROR "Module name missing for test file ${FILE}")
  ENDIF()

  GET_FILENAME_COMPONENT(test_ext ${FILE} EXT)
  GET_FILENAME_COMPONENT(runtime_dir ${FILE} PATH)  # Not using DIRECTORY because of CMake >=2.8.11 requirement

  SET(runtime_dir ${PROJECT_BINARY_DIR}/tests/${TEST_MODULE})

  IF(test_ext STREQUAL ".cc")
    # Tests written in C++
    ROUTERTEST_GET_TARGET(test_target ${FILE} ${TEST_MODULE})

    SET(test_name "${test_target}")
    MYSQL_ADD_EXECUTABLE(${test_target} ${FILE} ${TEST_EXTRA_SOURCES} ADD_TEST ${test_name})

    IF(MYSQL_ROUTER_BUILD_ALL_TARGET)
      ADD_DEPENDENCIES(${MYSQL_ROUTER_BUILD_ALL_TARGET} ${test_target})
    ENDIF()
    TARGET_LINK_LIBRARIES(${test_target}
      gtest gmock gtest_main gmock_main routertest_helpers
      router_lib harness-library
      ${CMAKE_THREAD_LIBS_INIT})
    FOREACH(libtarget ${TEST_LIB_DEPENDS})
      #add_dependencies(${test_target} ${libtarget})
      TARGET_LINK_LIBRARIES(${test_target} ${libtarget})
    ENDFOREACH()
    IF(TEST_DEPENDS)
      ADD_DEPENDENCIES(${test_target} ${TEST_DEPENDS})
    ENDIF()
    FOREACH(include_dir ${TEST_SYSTEM_INCLUDE_DIRS})
      TARGET_INCLUDE_DIRECTORIES(${test_target} SYSTEM PUBLIC ${include_dir})
    ENDFOREACH()
    FOREACH(include_dir ${TEST_INCLUDE_DIRS})
      TARGET_INCLUDE_DIRECTORIES(${test_target} PUBLIC ${include_dir})
    ENDFOREACH()

    IF(WITH_VALGRIND)
      FIND_PROGRAM(VALGRIND valgrind)
      SET(TEST_WRAPPER ${VALGRIND} --error-exitcode=1)
      SET(TEST_ENV_PREFIX "${TEST_ENV_PREFIX};WITH_VALGRIND=1")
    ENDIF()

    SET(TEST_ENV_PREFIX "CMAKE_SOURCE_DIR=${MySQLRouter_SOURCE_DIR};CMAKE_BINARY_DIR=${MySQLRouter_BINARY_DIR}")

    IF(WIN32)
      # PATH's separator ";" needs to be escaped as CMAKE's test-env is also separated by ; ...
      STRING(REPLACE ";" "\\;" ESC_ENV_PATH "$ENV{PATH}")
      set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT
        "${TEST_ENV_PREFIX};PATH=$<TARGET_FILE_DIR:harness-library>\;$<TARGET_FILE_DIR:http_common>\;$<TARGET_FILE_DIR:duktape>\;${ESC_ENV_PATH};${TEST_ENVIRONMENT}")
    ELSE()
      set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT
        "${TEST_ENV_PREFIX};LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH};DYLD_LIBRARY_PATH=$ENV{DYLD_LIBRARY_PATH};${TEST_ENVIRONMENT}")
    ENDIF()
  ELSE()
    MESSAGE(ERROR "Unknown test type; file '${FILE}'")
  ENDIF()

ENDFUNCTION(add_test_file)

FUNCTION(add_test_dir DIR_NAME)
  SET(one_value_args MODULE ENVIRONMENT)
  SET(multi_value_args LIB_DEPENDS INCLUDE_DIRS SYSTEM_INCLUDE_DIRS DEPENDS)
  cmake_parse_arguments(TEST "" "${one_value_args}" "${multi_value_args}" ${ARGN})

  IF(NOT TEST_MODULE)
    MESSAGE(FATAL_ERROR "Module name missing for test folder ${DIR_NAME}")
  ENDIF()

  GET_FILENAME_COMPONENT(abs_path ${DIR_NAME} ABSOLUTE)

  FILE(GLOB test_files RELATIVE ${abs_path}
    ${abs_path}/*.cc)

  FOREACH(test_file ${test_files})
    IF(NOT ${test_file} MATCHES "^helper")
      add_test_file(${abs_path}/${test_file}
        MODULE ${TEST_MODULE}
        ENVIRONMENT ${TEST_ENVIRONMENT}
        LIB_DEPENDS ${TEST_LIB_DEPENDS}
        DEPENDS ${TEST_DEPENDS}
        INCLUDE_DIRS ${TEST_INCLUDE_DIRS}
        SYSTEM_INCLUDE_DIRS ${TEST_SYSTEM_INCLUDE_DIRS}
        )
    ENDIF()
  ENDFOREACH(test_file)

ENDFUNCTION(add_test_dir)

# Copy and configure configuration files templates
# from selected directory to common place in tests/data
FUNCTION(CONFIGURE_TEST_FILE_TEMPLATES SOURCE_PATH)
  SET(OUT_DIR ${PROJECT_BINARY_DIR}/tests/data/)
  FILE(GLOB_RECURSE _templates RELATIVE ${SOURCE_PATH} "*.cfg.in" "*.conf.in")
  IF(NOT BUILD_IS_SINGLE_CONFIG)
    SET(ORIG_HARNESS_PLUGIN_OUTPUT_DIRECTORY ${HARNESS_PLUGIN_OUTPUT_DIRECTORY})
    SET(PLUGIN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_output_directory)
    FOREACH(config ${CMAKE_CONFIGURATION_TYPES})
      FOREACH(_template ${_templates})
        STRING(REGEX REPLACE ".in$" "" _output ${config}/${_template})
        #MESSAGE(STATUS "Generating ${_output} in ${OUT_DIR} from ${_template}")
        SET(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIRECTORY}/${config})
        SET(ROUTER_RUNTIME_DIR ${OUT_DIR}/${config})
        CONFIGURE_FILE(${SOURCE_PATH}/${_template} ${OUT_DIR}/${_output})
      ENDFOREACH()
    ENDFOREACH()
    SET(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${OLD_HARNESS_PLUGIN_OUTPUT_DIRECTORY})
  ELSE()
    SET(ORIG_HARNESS_PLUGIN_OUTPUT_DIRECTORY ${HARNESS_PLUGIN_OUTPUT_DIRECTORY})
    SET(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_output_directory)
    SET(ROUTER_RUNTIME_DIR ${OUT_DIR})
    FOREACH(_template ${_templates})
      STRING(REGEX REPLACE ".in$" "" _output ${_template})
      #MESSAGE(STATUS "Generating ${_output} from ${_template}")
      CONFIGURE_FILE(${SOURCE_PATH}/${_template} ${OUT_DIR}/${_output})
    ENDFOREACH()
    SET(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${OLD_HARNESS_PLUGIN_OUTPUT_DIRECTORY})
  ENDIF()
ENDFUNCTION()

# Copy plain configuration files to common place in tests/data
FUNCTION(COPY_TEST_FILES SOURCE_PATH)
  SET(OUT_DIR ${PROJECT_BINARY_DIR}/tests/data/)
  IF(NOT BUILD_IS_SINGLE_CONFIG)
    FOREACH(config ${CMAKE_CONFIGURATION_TYPES})
      FILE(GLOB_RECURSE _files RELATIVE ${SOURCE_PATH} "*.cfg" "*.conf" "*.ini")
      FOREACH(_file ${_files})
        CONFIGURE_FILE(${SOURCE_PATH}/${_file} ${OUT_DIR}/${config}/${_file} COPYONLY)
      ENDFOREACH()
    ENDFOREACH()
  ELSE()
    # Copy plain configuration files
    FILE(GLOB_RECURSE _files RELATIVE ${SOURCE_PATH} "*.cfg" "*.conf" "*.ini")
    FOREACH(_file ${_files})
      CONFIGURE_FILE(${SOURCE_PATH}/${_file} ${OUT_DIR}/${_file} COPYONLY)
    ENDFOREACH()
    SET(HARNESS_PLUGIN_OUTPUT_DIRECTORY ${OLD_HARNESS_PLUGIN_OUTPUT_DIRECTORY})
  ENDIF()
ENDFUNCTION()

# Create a directory structure inside of tests/data/${DIRECTORY_NAME}
# (needed by some harness tests)
FUNCTION(CREATE_HARNESS_TEST_DIRECTORY_POST_BUILD TARGET DIRECTORY_NAME)
  IF(NOT BUILD_IS_SINGLE_CONFIG)
    SET(OUT_DIR ${PROJECT_BINARY_DIR}/tests/data/)
    FOREACH(config_ ${CMAKE_CONFIGURATION_TYPES})
      add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT_DIR}/${config_}/var/log/${DIRECTORY_NAME})
    ENDFOREACH()
  ELSE()
      SET(OUT_DIR "${PROJECT_BINARY_DIR}/tests/data")
      add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/var/log/${DIRECTORY_NAME}")
  ENDIF()
ENDFUNCTION()
