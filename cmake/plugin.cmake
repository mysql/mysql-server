# Copyright (C) 2009 Sun Microsystems, Inc
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

# Creates a project to build plugin either as static or shared library
# Parameters:
# plugin - storage engine name.
# variable BUILD_TYPE should be set to "STATIC" or "DYNAMIC"
# Remarks:
# ${PLUGIN}_SOURCES  variable containing source files to produce the 
# library must set before calling this macro

MACRO(MYSQL_PLUGIN plugin)
  # Add common include directories
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include 
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${SSL_INCLUDE_DIRS}
                    ${ZLIB_INCLUDE_DIR})

  STRING(TOUPPER ${plugin} plugin)
  STRING(TOLOWER ${plugin} target)

  IF(NOT ${plugin}_PLUGIN_STATIC AND NOT ${plugin}_PLUGIN_DYNAMIC)
    MESSAGE(FATAL_ERROR   
    "Neither ${plugin}_PLUGIN_STATIC nor ${plugin}_PLUGIN_DYNAMIC is defined.
    Please set at least one of these variables to the name of the output 
	library in CMakeLists.txt prior to calling MYSQL_PLUGIN"
    )
  ENDIF()
  
  IF(WITH_PLUGIN_${plugin})
    SET(WITH_${plugin} 1)
  ENDIF()

  IF(WITH_${plugin}_STORAGE_ENGINE OR WITH_{$plugin}  OR WITH_ALL
   OR WITH_MAX AND NOT WITHOUT_${plugin}_STORAGE_ENGINE AND NOT
   WITHOUT_${plugin})
    SET(WITH_${plugin} 1)
  ELSEIF(WITHOUT_${plugin}_STORAGE_ENGINE OR WITH_NONE OR ${plugin}_DISABLED)
    SET(WITHOUT_${plugin} 1)
    SET(WITH_${plugin}_STORAGE_ENGINE 0)
    SET(WITH_${plugin} 0)
  ENDIF()

  IF(${plugin}_PLUGIN_MANDATORY)
    SET(WITH_${plugin} 1)
  ENDIF()
  
  IF(${plugin} MATCHES NDBCLUSTER AND WITH_MAX_NO_NDB)
    SET(WITH_${plugin} 0)
    SET(WITH_${plugin}_STORAGE_ENGINE 0)
    SET(WITHOUT_${plugin} 1)
    SET(WITHOUT_${plugin}_STORAGE_ENGINE 0)
  ENDIF()
  
  IF(STORAGE_ENGINE)
      SET(with_var "WITH_${plugin}_STORAGE_ENGINE" )
  ELSE()
      SET(with_var "WITH_${plugin}")
  ENDIF()
  

  USE_ABSOLUTE_FILENAMES(${plugin}_SOURCES)
  
  IF (WITH_${plugin} AND ${plugin}_PLUGIN_STATIC)
    ADD_DEFINITIONS(-DMYSQL_SERVER)
    #Create static library.
    ADD_LIBRARY(${target} ${${plugin}_SOURCES})
    DTRACE_INSTRUMENT(${target})   
    ADD_DEPENDENCIES(${target} GenError)
    IF(${plugin}_LIBS)
      TARGET_LINK_LIBRARIES(${target} ${${plugin}_LIBS})
	ENDIF()
    SET_TARGET_PROPERTIES(${target} PROPERTIES 
	  OUTPUT_NAME "${${plugin}_PLUGIN_STATIC}")
    # Update mysqld dependencies
    SET (MYSQLD_STATIC_PLUGIN_LIBS ${MYSQLD_STATIC_PLUGIN_LIBS} 
	  ${target} PARENT_SCOPE)
    SET (mysql_plugin_defs  "${mysql_plugin_defs},builtin_${target}_plugin" 
	  PARENT_SCOPE)
    SET(${with_var} ON CACHE BOOL "Link ${plugin} statically to the server" 
	  FORCE)
  ELSEIF(NOT WITHOUT_${plugin} AND ${plugin}_PLUGIN_DYNAMIC 
    AND NOT WITHOUT_DYNAMIC_PLUGINS)
	
    # Create a shared module.
    ADD_DEFINITIONS(-DMYSQL_DYNAMIC_PLUGIN)
    ADD_LIBRARY(${target} MODULE ${${plugin}_SOURCES})  
    IF(${plugin}_LIBS)
      TARGET_LINK_LIBRARIES(${target} ${${plugin}_LIBS})
	ENDIF()	
    DTRACE_INSTRUMENT(${target})   
    SET_TARGET_PROPERTIES (${target} PROPERTIES PREFIX "")
    TARGET_LINK_LIBRARIES (${target} mysqlservices)
	
	# Plugin uses symbols defined in mysqld executable.
    # Some operating systems like Windows and OSX and are pretty strict about 
	# unresolved symbols. Others are less strict and allow unresolved symbols
    # in shared libraries. On Linux for example, CMake does not even add 
    # executable to the linker command line (it would result into link error). 
    # Thus we skip TARGET_LINK_LIBRARIES on Linux, as it would only generate
    # an additional dependency.
	IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
      TARGET_LINK_LIBRARIES (${target} mysqld)
	ENDIF()
	
    ADD_DEPENDENCIES(${target} GenError)
	
    IF(${plugin}_PLUGIN_DYNAMIC)
      SET_TARGET_PROPERTIES(${target} PROPERTIES 
	    OUTPUT_NAME "${${plugin}_PLUGIN_DYNAMIC}")
    ENDIF()
	
    # Update cache "WITH" variable for plugins that support static linking
    IF(${plugin}_PLUGIN_STATIC)
      SET(${with_var} OFF CACHE BOOL "Link ${plugin} statically to the server"
	    FORCE)
    ENDIF()
	
    # Install dynamic library
    SET(INSTALL_LOCATION lib/plugin)
    INSTALL(TARGETS ${target} DESTINATION ${INSTALL_LOCATION})
    INSTALL_DEBUG_SYMBOLS(${target})
  ELSE()
    IF(STORAGE_ENGINE)
      SET(without_var "WITHOUT_${plugin}_STORAGE_ENGINE")
    ELSE()
      SET(without_var "WITHOUT_${plugin}")
	ENDIF()
    SET(${without_var} ON CACHE BOOL "Link ${plugin} statically to the server"
	  FORCE)
    MARK_AS_ADVANCED(${without_var})
  ENDIF()
ENDMACRO()

MACRO (MYSQL_STORAGE_ENGINE engine)
  SET(STORAGE_ENGINE 1)
  MYSQL_PLUGIN(${engine})
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
  # Special handling for partition(not really pluggable)
  IF(NOT WITHOUT_PARTITION_STORAGE_ENGINE)
   SET (WITH_PARTITION_STORAGE_ENGINE 1)
   SET (mysql_plugin_defs  "${mysql_plugin_defs},builtin_partition_plugin")
  ENDIF(NOT WITHOUT_PARTITION_STORAGE_ENGINE)
  ADD_DEFINITIONS(${STORAGE_ENGINE_DEFS})
  CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/sql/sql_builtin.cc.in
    ${CMAKE_BINARY_DIR}/sql/sql_builtin.cc)
ENDMACRO()
