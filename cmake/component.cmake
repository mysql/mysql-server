# Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.
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


GET_FILENAME_COMPONENT(MYSQL_CMAKE_SCRIPT_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/cmake_parse_arguments.cmake)

# MYSQL_ADD_COMPONENT(component source1...sourceN
#
# [STATIC|MODULE|TEST]
#
# [LINK_LIBRARIES lib1...libN]

# STATIC - generate new static library,
# MODULE - generate dynamic library,
# TEST - include library only with test distribution

# Append collections files for the component to the common files
# Make sure we don't copy twice if running cmake again
MACRO(COMPONENT_APPEND_COLLECTIONS)
  SET(fcopied "${CMAKE_CURRENT_SOURCE_DIR}/tests/collections/FilesCopied")
  IF(NOT EXISTS ${fcopied})
    FILE(GLOB collections ${CMAKE_CURRENT_SOURCE_DIR}/tests/collections/*)
    FOREACH(cfile ${collections})
      FILE(READ ${cfile} contents)
      GET_FILENAME_COMPONENT(fname ${cfile} NAME)
      FILE(APPEND ${CMAKE_SOURCE_DIR}/mysql-test/collections/${fname}
        "${contents}")
      FILE(APPEND ${fcopied} "${fname}\n")
      MESSAGE(STATUS "Appended ${cfile}")
    ENDFOREACH()
  ENDIF()
ENDMACRO()

MACRO(MYSQL_ADD_COMPONENT)
  MYSQL_PARSE_ARGUMENTS(ARG
    "LINK_LIBRARIES"
    "STATIC;MODULE;TEST;NO_INSTALL"
    ${ARGN}
  )

  # Add common include directories
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

  LIST(GET ARG_DEFAULT_ARGS 0 component)
  SET(SOURCES ${ARG_DEFAULT_ARGS})
  LIST(REMOVE_AT SOURCES 0)
  STRING(TOUPPER ${component} component)
  STRING(TOLOWER ${component} component_lower)
  STRING(TOLOWER component_${component} target)

  # If not dynamic component, add it to list of built-ins
  IF (ARG_STATIC)
    IF (NOT "${component}" STREQUAL "MYSQL_SERVER")
      MESSAGE(FATAL_ERROR "Only one server built-in component is expected.")
    ENDIF()
  ENDIF()

  # Build either static library or module
  IF (ARG_STATIC)
    SET(kind STATIC)
    SET(BUILD_COMPONENT 1)

    # Update mysqld dependencies
    SET(MYSQLD_STATIC_COMPONENT_LIBS ${MYSQLD_STATIC_COMPONENT_LIBS}
        ${target} ${ARG_LINK_LIBRARIES} CACHE INTERNAL "" FORCE)

  ELSEIF(ARG_MODULE AND NOT DISABLE_SHARED)
    SET(kind MODULE)
    SET(BUILD_COMPONENT 1)
  ELSE()
    SET(BUILD_COMPONENT 0)
  ENDIF()

  IF(BUILD_COMPONENT)
    ADD_VERSION_INFO(${target} ${kind} SOURCES)
    ADD_LIBRARY(${target} ${kind} ${SOURCES})

    # For internal testing in PB2, append collections files
    IF(DEFINED ENV{PB2WORKDIR})
      COMPONENT_APPEND_COLLECTIONS()
    ENDIF()

    IF(ARG_LINK_LIBRARIES)
      TARGET_LINK_LIBRARIES(${target} ${ARG_LINK_LIBRARIES})
    ENDIF()

    SET_TARGET_PROPERTIES(${target} PROPERTIES PREFIX "")
    ADD_DEPENDENCIES(${target} GenError)

    IF (ARG_MODULE)
      # Store all components in the same directory, for easier testing.
      SET_TARGET_PROPERTIES(${target} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/component_output_directory
        )
      IF(NOT ARG_NO_INSTALL)
        # Install dynamic library.
        IF(NOT ARG_TEST)
          SET(INSTALL_COMPONENT Server)
        ELSE()
          SET(INSTALL_COMPONENT Test)
        ENDIF()

        IF(LINUX_INSTALL_RPATH_ORIGIN)
          SET_PROPERTY(TARGET ${target} PROPERTY INSTALL_RPATH "\$ORIGIN/")
        ENDIF()
        MYSQL_INSTALL_TARGETS(${target}
          DESTINATION ${INSTALL_PLUGINDIR}
          COMPONENT ${INSTALL_COMPONENT})
        INSTALL_DEBUG_TARGET(${target}
          DESTINATION ${INSTALL_PLUGINDIR}/debug
          COMPONENT ${INSTALL_COMPONENT})
      ENDIF()
    ENDIF()
  ENDIF()
ENDMACRO()


# Add all CMake projects under components
MACRO(CONFIGURE_COMPONENTS)
  FILE(GLOB dirs_components ${CMAKE_SOURCE_DIR}/components/*)
  FILE(GLOB dirs_components_test ${CMAKE_SOURCE_DIR}/components/test/*)
  FOREACH(dir ${dirs_components} ${dirs_components_test})
    IF (EXISTS ${dir}/CMakeLists.txt)
      ADD_SUBDIRECTORY(${dir})
    ENDIF()
  ENDFOREACH()
ENDMACRO()
