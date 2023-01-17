# Copyright (c) 2013, 2023, Oracle and/or its affiliates.
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

# MYSQL_ADD_COMPONENT(component sources... options/keywords...)

MACRO(MYSQL_ADD_COMPONENT component_arg)
  SET(COMPONENT_OPTIONS
    MODULE_ONLY    # generate dynamic library
    SKIP_INSTALL
    STATIC         # generate new static library
    TEST_ONLY      # include library only with test distribution
    )
  SET(COMPONENT_ONE_VALUE_KW
    )
  SET(COMPONENT_MULTI_VALUE_KW
    LINK_LIBRARIES # lib1 ... libN
    )

  CMAKE_PARSE_ARGUMENTS(ARG
    "${COMPONENT_OPTIONS}"
    "${COMPONENT_ONE_VALUE_KW}"
    "${COMPONENT_MULTI_VALUE_KW}"
    ${ARGN}
    )

  SET(component ${component_arg})
  SET(SOURCES ${ARG_UNPARSED_ARGUMENTS})

  STRING(TOUPPER ${component} component)
  STRING(TOLOWER ${component} component_lower)
  STRING(TOLOWER component_${component} target)

  GET_PROPERTY(CWD_DEFINITIONS DIRECTORY PROPERTY COMPILE_DEFINITIONS)
  LIST(FIND CWD_DEFINITIONS "MYSQL_SERVER" FOUND_DEFINITION)
  IF(NOT FOUND_DEFINITION EQUAL -1)
    MESSAGE(FATAL_ERROR
      "component ${component} has -DMYSQL_SERVER")
  ENDIF()

  # If not dynamic component, add it to list of built-ins
  IF (ARG_STATIC)
    IF (NOT "${component}" STREQUAL "MYSQL_SERVER")
      MESSAGE(FATAL_ERROR "Only one server built-in component is expected.")
    ENDIF()
  ENDIF()

  # Build either static library or module
  IF (ARG_STATIC)
    SET(kind STATIC)
  ELSEIF(ARG_MODULE_ONLY)
    SET(kind MODULE)
  ELSE()
    MESSAGE(FATAL_ERROR "Unknown component type ${target}")
  ENDIF()

  ADD_VERSION_INFO(${target} ${kind} SOURCES)
  ADD_LIBRARY(${target} ${kind} ${SOURCES})

  TARGET_COMPILE_DEFINITIONS(${target} PUBLIC MYSQL_COMPONENT)
  IF(COMPRESS_DEBUG_SECTIONS)
    MY_TARGET_LINK_OPTIONS(${target}
      "LINKER:--compress-debug-sections=zlib")
  ENDIF()

  IF(ARG_LINK_LIBRARIES)
    TARGET_LINK_LIBRARIES(${target} ${ARG_LINK_LIBRARIES})
  ENDIF()

  SET_TARGET_PROPERTIES(${target} PROPERTIES PREFIX "")
  ADD_DEPENDENCIES(${target} GenError)

  IF (ARG_MODULE_ONLY)
    # Store all components in the same directory, for easier testing.
    SET_TARGET_PROPERTIES(${target} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_output_directory
      )

    # For APPLE: adjust path dependecy for SSL shared libraries.
    SET_PATH_TO_CUSTOM_SSL_FOR_APPLE(${target})

    IF(WIN32_CLANG AND WITH_ASAN)
      TARGET_LINK_LIBRARIES(${target}
        "${ASAN_LIB_DIR}/clang_rt.asan_dll_thunk-x86_64.lib")
    ENDIF()

    # To hide the component symbols in the shared object
    IF(UNIX)
      IF(MY_COMPILER_IS_CLANG AND WITH_UBSAN)
        # nothing, clang/ubsan gets confused
        UNSET(COMPONENT_COMPILE_VISIBILITY CACHE)
      ELSE()
        # Use this also for component libraries and tests.
        SET(COMPONENT_COMPILE_VISIBILITY
          "-fvisibility=hidden" CACHE INTERNAL
          "Use -fvisibility=hidden for components" FORCE)
        TARGET_COMPILE_OPTIONS(${target} PRIVATE "-fvisibility=hidden")
      ENDIF()
    ENDIF()

    IF(NOT ARG_SKIP_INSTALL)
      # Install dynamic library.
      IF(ARG_TEST_ONLY)
        SET(INSTALL_COMPONENT Test)
      ELSE()
        SET(INSTALL_COMPONENT Server)
      ENDIF()

      ADD_INSTALL_RPATH_FOR_OPENSSL(${target})
      MYSQL_INSTALL_TARGET(${target}
        DESTINATION ${INSTALL_PLUGINDIR}
        COMPONENT ${INSTALL_COMPONENT})
      INSTALL_DEBUG_TARGET(${target}
        DESTINATION ${INSTALL_PLUGINDIR}/debug
        COMPONENT ${INSTALL_COMPONENT})
    ENDIF()
  ENDIF()

  ADD_DEPENDENCIES(component_all ${target})

ENDMACRO(MYSQL_ADD_COMPONENT)


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
