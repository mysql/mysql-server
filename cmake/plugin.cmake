# Copyright (c) 2009, 2012, Oracle and/or its affiliates. All rights reserved.
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
# [STATIC_ONLY|DYNAMIC_ONLY]
# [MODULE_OUTPUT_NAME module_name]
# [STATIC_OUTPUT_NAME static_name]
# [RECOMPILE_FOR_EMBEDDED]
# [LINK_LIBRARIES lib1...libN]
# [DEPENDENCIES target1...targetN]

MACRO(MYSQL_ADD_PLUGIN)
  MYSQL_PARSE_ARGUMENTS(ARG
    "LINK_LIBRARIES;DEPENDENCIES;MODULE_OUTPUT_NAME;STATIC_OUTPUT_NAME;COMPONENT"
    "STORAGE_ENGINE;STATIC_ONLY;MODULE_ONLY;MANDATORY;DEFAULT;DISABLED;RECOMPILE_FOR_EMBEDDED"
    ${ARGN}
  )
  
  # Add common include directories
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include 
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${SSL_INCLUDE_DIRS}
                    ${ZLIB_INCLUDE_DIR})

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
    OR WITH_${plugin}
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
  
  IF(NOT ARG_MODULE_OUTPUT_NAME)
    IF(ARG_STORAGE_ENGINE)
      SET(ARG_MODULE_OUTPUT_NAME "ha_${target}")
    ELSE()
      SET(ARG_MODULE_OUTPUT_NAME "${target}")
    ENDIF()
  ENDIF()

  # Build either static library or module
  IF (WITH_${plugin} AND NOT ARG_MODULE_ONLY)

    IF(CMAKE_GENERATOR MATCHES "Makefiles")
      # If there is a shared library from previous shared build,
      # remove it. This is done just for mysql-test-run.pl 
      # so it does not try to use stale shared lib as plugin 
      # in test.
      FILE(REMOVE 
        ${CMAKE_CURRENT_BINARY_DIR}/${ARG_MODULE_OUTPUT_NAME}${CMAKE_SHARED_MODULE_SUFFIX})
    ENDIF()

    ADD_LIBRARY(${target} STATIC ${SOURCES})
    DTRACE_INSTRUMENT(${target})
    ADD_DEPENDENCIES(${target} GenError ${ARG_DEPENDENCIES})
    RESTRICT_SYMBOL_EXPORTS(${target})
    IF(WITH_EMBEDDED_SERVER)
      # Embedded library should contain PIC code and be linkable
      # to shared libraries (on systems that need PIC)
      IF(ARG_RECOMPILE_FOR_EMBEDDED OR NOT _SKIP_PIC)
        # Recompile some plugins for embedded
        ADD_CONVENIENCE_LIBRARY(${target}_embedded ${SOURCES})
        DTRACE_INSTRUMENT(${target}_embedded)   
        IF(ARG_RECOMPILE_FOR_EMBEDDED)
          SET_TARGET_PROPERTIES(${target}_embedded 
            PROPERTIES COMPILE_DEFINITIONS "EMBEDDED_LIBRARY")
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

    IF(ARG_MANDATORY)
      SET(${with_var} ON CACHE INTERNAL "Link ${plugin} statically to the server" 
       FORCE)
    ELSE()	
      SET(${with_var} ON CACHE BOOL "Link ${plugin} statically to the server" 
       FORCE)
    ENDIF()

    IF(ARG_MANDATORY)
      SET (mysql_mandatory_plugins  
        "${mysql_mandatory_plugins} builtin_maria_${target}_plugin," 
      PARENT_SCOPE)
    ELSE()
      SET (mysql_optional_plugins  
        "${mysql_optional_plugins} builtin_maria_${target}_plugin,"
      PARENT_SCOPE)
    ENDIF()
  ELSEIF(NOT WITHOUT_${plugin} AND NOT ARG_STATIC_ONLY  AND NOT WITHOUT_DYNAMIC_PLUGINS)
  
    ADD_VERSION_INFO(${target} MODULE SOURCES)
    ADD_LIBRARY(${target} MODULE ${SOURCES}) 
    DTRACE_INSTRUMENT(${target})
    SET_TARGET_PROPERTIES (${target} PROPERTIES PREFIX ""
      COMPILE_DEFINITIONS "MYSQL_DYNAMIC_PLUGIN")
    TARGET_LINK_LIBRARIES (${target} mysqlservices)

    # Plugin uses symbols defined in mysqld executable.
    # Some operating systems like Windows and OSX and are pretty strict about 
    # unresolved symbols. Others are less strict and allow unresolved symbols
    # in shared libraries. On Linux for example, CMake does not even add 
    # executable to the linker command line (it would result into link error). 
    # Thus we skip TARGET_LINK_LIBRARIES on Linux, as it would only generate
    # an additional dependency.
    IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
      TARGET_LINK_LIBRARIES (${target} mysqld ${ARG_LINK_LIBRARIES})
    ENDIF()
    ADD_DEPENDENCIES(${target} GenError ${ARG_DEPENDENCIES})

     IF(NOT ARG_MODULE_ONLY)
      # set cached variable, e.g with checkbox in GUI
      SET(${with_var} OFF CACHE BOOL "Link ${plugin} statically to the server" 
       FORCE)
     ENDIF()
    SET_TARGET_PROPERTIES(${target} PROPERTIES 
      OUTPUT_NAME "${ARG_MODULE_OUTPUT_NAME}")  
    # Install dynamic library
    IF(ARG_COMPONENT)
      IF(RPM AND NOT CPACK_COMPONENTS_ALL MATCHES ${ARG_COMPONENT})
        SET(CPACK_COMPONENTS_ALL ${CPACK_COMPONENTS_ALL} ${ARG_COMPONENT} PARENT_SCOPE)
        SET(CPACK_RPM_${ARG_COMPONENT}_PACKAGE_REQUIRES "MariaDB-server" PARENT_SCOPE)

        # workarounds for cmake issues #13248 and #12864:
        SET(CPACK_RPM_${ARG_COMPONENT}_USER_FILELIST ${ignored} PARENT_SCOPE)
        SET(CPACK_RPM_${ARG_COMPONENT}_PACKAGE_PROVIDES "cmake_bug_13248" PARENT_SCOPE)
        SET(CPACK_RPM_${ARG_COMPONENT}_PACKAGE_OBSOLETES "cmake_bug_13248" PARENT_SCOPE)
      ENDIF()
    ELSE()
      SET(ARG_COMPONENT Server)
    ENDIF()
    MYSQL_INSTALL_TARGETS(${target} DESTINATION ${INSTALL_PLUGINDIR} COMPONENT ${ARG_COMPONENT})
    #INSTALL_DEBUG_TARGET(${target} DESTINATION ${INSTALL_PLUGINDIR}/debug COMPONENT ${ARG_COMPONENT})
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

  GET_FILENAME_COMPONENT(subpath ${CMAKE_CURRENT_SOURCE_DIR} NAME)
  IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mysql-test")
    INSTALL_MYSQL_TEST("${CMAKE_CURRENT_SOURCE_DIR}/mysql-test/" "plugin/${subpath}")
  ENDIF()

ENDMACRO()


# Add all CMake projects under storage  and plugin 
# subdirectories, configure sql_builtins.cc
MACRO(CONFIGURE_PLUGINS)
  FILE(GLOB dirs_storage ${CMAKE_SOURCE_DIR}/storage/*)
  FILE(GLOB dirs_plugin ${CMAKE_SOURCE_DIR}/plugin/*)
  FOREACH(dir ${dirs_storage} ${dirs_plugin})
    IF (EXISTS ${dir}/CMakeLists.txt)
      ADD_SUBDIRECTORY(${dir})
    ENDIF()
  ENDFOREACH()
ENDMACRO()
