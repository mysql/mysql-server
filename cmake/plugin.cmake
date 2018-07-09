# Copyright (c) 2009, 2018, Oracle and/or its affiliates. All rights reserved.
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


GET_FILENAME_COMPONENT(MYSQL_CMAKE_SCRIPT_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/cmake_parse_arguments.cmake)

# MYSQL_ADD_PLUGIN(plugin_name source1...sourceN
# [STORAGE_ENGINE]
# [MANDATORY|DEFAULT]
# [STATIC_ONLY|MODULE_ONLY]
# [MODULE_OUTPUT_NAME module_name]
# [STATIC_OUTPUT_NAME static_name]
# [NOT_FOR_EMBEDDED]
# [RECOMPILE_FOR_EMBEDDED]
# [LINK_LIBRARIES lib1...libN]
# [DEPENDENCIES target1...targetN]

# MANDATORY   : not actually a plugin, always builtin
# DEFAULT     : builtin as static by default
# MODULE_ONLY : build only as shared library

# Append collections files for the plugin to the common files
# Make sure we don't copy twice if running cmake again

MACRO(PLUGIN_APPEND_COLLECTIONS plugin)
  SET(fcopied "${CMAKE_CURRENT_SOURCE_DIR}/tests/collections/FilesCopied")
  IF(NOT EXISTS ${fcopied})
    FILE(GLOB collections ${CMAKE_CURRENT_SOURCE_DIR}/tests/collections/*)
    FOREACH(cfile ${collections})
      FILE(READ ${cfile} contents)
      GET_FILENAME_COMPONENT(fname ${cfile} NAME)
      FILE(APPEND ${CMAKE_SOURCE_DIR}/mysql-test/collections/${fname} "${contents}")
      FILE(APPEND ${fcopied} "${fname}\n")
      MESSAGE(STATUS "Appended ${cfile}")
    ENDFOREACH()
  ENDIF()
ENDMACRO()

MACRO(MYSQL_ADD_PLUGIN)
  MYSQL_PARSE_ARGUMENTS(ARG
    "LINK_LIBRARIES;DEPENDENCIES;MODULE_OUTPUT_NAME;STATIC_OUTPUT_NAME"
    "STORAGE_ENGINE;STATIC_ONLY;MODULE_ONLY;CLIENT_ONLY;MANDATORY;DEFAULT;DISABLED;NOT_FOR_EMBEDDED;RECOMPILE_FOR_EMBEDDED;TEST_ONLY"
    ${ARGN}
  )
  
  # Add common include directories
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include 
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/libbinlogevents/include
                    ${CMAKE_SOURCE_DIR}/sql/auth
                    ${CMAKE_SOURCE_DIR}/regex
                    ${SSL_INCLUDE_DIRS}
                    )

  LIST(GET ARG_DEFAULT_ARGS 0 plugin) 
  SET(SOURCES ${ARG_DEFAULT_ARGS})
  LIST(REMOVE_AT SOURCES 0)
  STRING(TOUPPER ${plugin} plugin)
  STRING(TOLOWER ${plugin} target)
  
  # Figure out whether to build plugin
  IF(WITH_PLUGIN_${plugin})
    SET(WITH_${plugin} 1)
  ENDIF()

  IF(WITH_MAX_NO_NDB)
    SET(WITH_MAX 1)
    SET(WITHOUT_NDBCLUSTER 1)
  ENDIF()

  IF(ARG_DEFAULT)
    IF(NOT DEFINED WITH_${plugin} AND 
       NOT DEFINED WITH_${plugin}_STORAGE_ENGINE)
      SET(WITH_${plugin} 1)
    ENDIF()
  ENDIF()
  
  IF(WITH_${plugin}_STORAGE_ENGINE 
    OR WITH_{$plugin}
    OR WITH_ALL 
    OR WITH_MAX 
    AND NOT WITHOUT_${plugin}_STORAGE_ENGINE
    AND NOT WITHOUT_${plugin}
    AND NOT ARG_MODULE_ONLY)
     
    SET(WITH_${plugin} 1)
  ELSEIF(WITHOUT_${plugin}_STORAGE_ENGINE OR WITH_NONE OR ${plugin}_DISABLED)
    SET(WITHOUT_${plugin} 1)
    SET(WITH_${plugin}_STORAGE_ENGINE 0)
    SET(WITH_${plugin} 0)
  ENDIF()
  
  
  IF(ARG_MANDATORY)
    SET(WITH_${plugin} 1)
  ENDIF()

  
  IF(ARG_STORAGE_ENGINE)
    SET(with_var "WITH_${plugin}_STORAGE_ENGINE" )
  ELSE()
    SET(with_var "WITH_${plugin}")
  ENDIF()
  
  IF(NOT ARG_DEPENDENCIES)
    SET(ARG_DEPENDENCIES)
  ENDIF()
  SET(BUILD_PLUGIN 1)
  # Build either static library or module
  IF (WITH_${plugin} AND NOT ARG_MODULE_ONLY)
    ADD_LIBRARY(${target} STATIC ${SOURCES})
    SET_TARGET_PROPERTIES(${target}
      PROPERTIES COMPILE_DEFINITIONS "MYSQL_SERVER")
    # Collect all static libraries in the same directory
    SET_TARGET_PROPERTIES(${target} PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/archive_output_directory)

    # Keep track of known convenience libraries, in a global scope.
    # Static plugins may be linked into the mysqlserver library (embedded)
    SET(KNOWN_CONVENIENCE_LIBRARIES
      ${KNOWN_CONVENIENCE_LIBRARIES} ${target} CACHE INTERNAL "" FORCE)

    # Generate a cmake file which will save the name of the library.
    CONFIGURE_FILE(
      ${MYSQL_CMAKE_SCRIPT_DIR}/save_archive_location.cmake.in
      ${CMAKE_BINARY_DIR}/archive_output_directory/lib_location_${target}.cmake
      @ONLY)
    ADD_CUSTOM_COMMAND(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      -DTARGET_NAME=${target}
      -DTARGET_LOC=$<TARGET_FILE:${target}>
      -DCFG_INTDIR=${CMAKE_CFG_INTDIR}
      -P ${CMAKE_BINARY_DIR}/archive_output_directory/lib_location_${target}.cmake
      )

    DTRACE_INSTRUMENT(${target})
    ADD_DEPENDENCIES(${target} GenError ${ARG_DEPENDENCIES})
    IF(WITH_EMBEDDED_SERVER AND NOT ARG_NOT_FOR_EMBEDDED)
      # Embedded library should contain PIC code and be linkable
      # to shared libraries (on systems that need PIC)
      IF(ARG_RECOMPILE_FOR_EMBEDDED OR NOT _SKIP_PIC)
        # Recompile some plugins for embedded
        ADD_CONVENIENCE_LIBRARY(${target}_embedded ${SOURCES})
        DTRACE_INSTRUMENT(${target}_embedded)   
        IF(ARG_RECOMPILE_FOR_EMBEDDED)
          SET_TARGET_PROPERTIES(${target}_embedded 
            PROPERTIES COMPILE_DEFINITIONS "MYSQL_SERVER;EMBEDDED_LIBRARY")
        ENDIF()
        ADD_DEPENDENCIES(${target}_embedded GenError)
      ENDIF()
    ENDIF()
    
    IF(ARG_STATIC_OUTPUT_NAME)
      SET_TARGET_PROPERTIES(${target} PROPERTIES 
      OUTPUT_NAME ${ARG_STATIC_OUTPUT_NAME})
    ENDIF()

    # Update mysqld dependencies
    SET (MYSQLD_STATIC_PLUGIN_LIBS ${MYSQLD_STATIC_PLUGIN_LIBS} 
      ${target} ${ARG_LINK_LIBRARIES} CACHE INTERNAL "" FORCE)

    # Update mysqld dependencies (embedded)
    IF(NOT ARG_NOT_FOR_EMBEDDED)
      SET (MYSQLD_STATIC_EMBEDDED_PLUGIN_LIBS ${MYSQLD_STATIC_EMBEDDED_PLUGIN_LIBS} 
        ${target} ${ARG_LINK_LIBRARIES} CACHE INTERNAL "" FORCE)
    ENDIF()

    IF(ARG_MANDATORY)
      SET(${with_var} ON CACHE INTERNAL "Link ${plugin} statically to the server" 
       FORCE)
    ELSE()	
      SET(${with_var} ON CACHE BOOL "Link ${plugin} statically to the server" 
       FORCE)
    ENDIF()

    SET(THIS_PLUGIN_REFERENCE " builtin_${target}_plugin,")
    IF(ARG_NOT_FOR_EMBEDDED)
      SET(THIS_PLUGIN_REFERENCE "
#ifndef EMBEDDED_LIBRARY
  ${THIS_PLUGIN_REFERENCE}
#endif
")
    ENDIF()
    SET(PLUGINS_IN_THIS_SCOPE
      "${PLUGINS_IN_THIS_SCOPE}${THIS_PLUGIN_REFERENCE}")

    IF(ARG_MANDATORY)
      SET (mysql_mandatory_plugins  
        "${mysql_mandatory_plugins} ${PLUGINS_IN_THIS_SCOPE}" 
        PARENT_SCOPE)
    ELSE()
      SET (mysql_optional_plugins  
        "${mysql_optional_plugins} ${PLUGINS_IN_THIS_SCOPE}"
        PARENT_SCOPE)
    ENDIF()

  ELSEIF(NOT WITHOUT_${plugin} AND NOT ARG_STATIC_ONLY  AND NOT WITHOUT_DYNAMIC_PLUGINS)
    IF(NOT ARG_MODULE_OUTPUT_NAME)
      IF(ARG_STORAGE_ENGINE)
        SET(ARG_MODULE_OUTPUT_NAME "ha_${target}")
      ELSE()
        SET(ARG_MODULE_OUTPUT_NAME "${target}")
      ENDIF()
    ENDIF()

    ADD_VERSION_INFO(${target} MODULE SOURCES)
    ADD_LIBRARY(${target} MODULE ${SOURCES}) 
    DTRACE_INSTRUMENT(${target})
    SET_TARGET_PROPERTIES (${target} PROPERTIES PREFIX ""
      COMPILE_DEFINITIONS "MYSQL_DYNAMIC_PLUGIN")
    IF(NOT ARG_CLIENT_ONLY)
      TARGET_LINK_LIBRARIES (${target} mysqlservices)
    ENDIF()

    GET_TARGET_PROPERTY(LINK_FLAGS ${target} LINK_FLAGS)
    IF(NOT LINK_FLAGS)
      # Avoid LINK_FLAGS-NOTFOUND
      SET(LINK_FLAGS)
    ENDIF()
    SET_TARGET_PROPERTIES(${target} PROPERTIES
      LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS} ${LINK_FLAGS} "
    )

    # Plugin uses symbols defined in mysqld executable.
    # Some operating systems like Windows and OSX and are pretty strict about 
    # unresolved symbols. Others are less strict and allow unresolved symbols
    # in shared libraries. On Linux for example, CMake does not even add 
    # executable to the linker command line (it would result into link error). 
    # Thus we skip TARGET_LINK_LIBRARIES on Linux, as it would only generate
    # an additional dependency.
    # Use MYSQL_PLUGIN_IMPORT for static data symbols to be exported.
    IF(NOT ARG_CLIENT_ONLY)
      IF(WIN32 OR APPLE)
        TARGET_LINK_LIBRARIES (${target} mysqld ${ARG_LINK_LIBRARIES})
      ENDIF()
    ENDIF()
    ADD_DEPENDENCIES(${target} GenError ${ARG_DEPENDENCIES})

     IF(NOT ARG_MODULE_ONLY)
      # set cached variable, e.g with checkbox in GUI
      SET(${with_var} OFF CACHE BOOL "Link ${plugin} statically to the server" 
       FORCE)
     ENDIF()
    SET_TARGET_PROPERTIES(${target} PROPERTIES 
      OUTPUT_NAME "${ARG_MODULE_OUTPUT_NAME}")  

    # Help INSTALL_DEBUG_TARGET to locate the plugin
    SET_TARGET_PROPERTIES(${target} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )

    # Install dynamic library
    SET(INSTALL_COMPONENT Server)
    IF(ARG_TEST_ONLY)
      SET(INSTALL_COMPONENT Test)
    ENDIF()
    MYSQL_INSTALL_TARGETS(${target}
      DESTINATION ${INSTALL_PLUGINDIR}
      COMPONENT ${INSTALL_COMPONENT})
    INSTALL_DEBUG_TARGET(${target}
      DESTINATION ${INSTALL_PLUGINDIR}/debug
      COMPONENT ${INSTALL_COMPONENT})
    # Add installed files to list for RPMs
    FILE(APPEND ${CMAKE_BINARY_DIR}/support-files/plugins.files
            "%attr(755, root, root) %{_prefix}/${INSTALL_PLUGINDIR}/${ARG_MODULE_OUTPUT_NAME}.so\n"
            "%attr(755, root, root) %{_prefix}/${INSTALL_PLUGINDIR}/debug/${ARG_MODULE_OUTPUT_NAME}.so\n")
    # For internal testing in PB2, append collections files
    IF(DEFINED ENV{PB2WORKDIR})
      PLUGIN_APPEND_COLLECTIONS(${plugin})
    ENDIF()
  ELSE()
    IF(WITHOUT_${plugin})
      # Update cache variable
      STRING(REPLACE "WITH_" "WITHOUT_" without_var ${with_var})
      SET(${without_var} ON CACHE BOOL "Don't build ${plugin}" 
       FORCE)
    ENDIF()
    SET(BUILD_PLUGIN 0)
  ENDIF()

  IF(BUILD_PLUGIN AND ARG_LINK_LIBRARIES)
    TARGET_LINK_LIBRARIES (${target} ${ARG_LINK_LIBRARIES})
  ENDIF()

ENDMACRO()


# Add all CMake projects under storage  and plugin 
# subdirectories, configure sql_builtin.cc
MACRO(CONFIGURE_PLUGINS)
  FILE(GLOB dirs_storage ${CMAKE_SOURCE_DIR}/storage/*)
  FILE(GLOB dirs_plugin ${CMAKE_SOURCE_DIR}/plugin/*)
  IF(WITH_RAPID)
    FILE(GLOB dirs_rapid_plugin ${CMAKE_SOURCE_DIR}/rapid/plugin/*)
  ENDIF(WITH_RAPID)
  
  FOREACH(dir ${dirs_storage} ${dirs_plugin} ${dirs_rapid_plugin})
    IF (EXISTS ${dir}/CMakeLists.txt)
      ADD_SUBDIRECTORY(${dir})
    ENDIF()
  ENDFOREACH()
ENDMACRO()
