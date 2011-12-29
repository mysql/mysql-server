# Copyright (c) 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# MYSQL_STORAGE_ENGINE Macro creates a project to build storage engine
# library. 
#
# Parameters:
# engine - storage engine name.
# variable ENGINE_BUILD_TYPE should be set to "STATIC" or "DYNAMIC"
# Remarks:
# ${engine}_SOURCES  variable containing source files to produce the library must set before
# calling this macro
# ${engine}_LIBS variable containing extra libraries to link with may be set


MACRO(MYSQL_PLUGIN engine)
IF(NOT SOURCE_SUBLIBS)
  # Add common include directories
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include)
  STRING(TOUPPER ${engine} engine)
  IF(${ENGINE_BUILD_TYPE} STREQUAL "STATIC")
    ADD_LIBRARY(${${engine}_LIB} ${${engine}_SOURCES})
    ADD_DEPENDENCIES(${${engine}_LIB} GenError)
    IF(${engine}_LIBS)
      TARGET_LINK_LIBRARIES(${${engine}_LIB} ${${engine}_LIBS})
    ENDIF(${engine}_LIBS)
    MESSAGE("build ${engine} as static library (${${engine}_LIB}.lib)")
  ELSEIF(${ENGINE_BUILD_TYPE} STREQUAL "DYNAMIC")
    ADD_DEFINITIONS(-DMYSQL_DYNAMIC_PLUGIN)
    ADD_VERSION_INFO(${${engine}_LIB} SHARED ${engine}_SOURCES)
    ADD_LIBRARY(${${engine}_LIB} MODULE ${${engine}_SOURCES})
    TARGET_LINK_LIBRARIES (${${engine}_LIB}  mysqlservices)
    IF(${engine}_LIBS)
      TARGET_LINK_LIBRARIES(${${engine}_LIB} ${${engine}_LIBS})
    ENDIF(${engine}_LIBS)
    # Install the plugin
    MYSQL_INSTALL_TARGETS(${${engine}_LIB} DESTINATION lib/plugin COMPONENT Server)
    MESSAGE("build ${engine} as DLL (${${engine}_LIB}.dll)")
  ENDIF(${ENGINE_BUILD_TYPE} STREQUAL "STATIC")
ENDIF(NOT SOURCE_SUBLIBS)
ENDMACRO(MYSQL_PLUGIN)

MACRO(MYSQL_STORAGE_ENGINE engine)
IF(NOT SOURCE_SUBLIBS)
  MYSQL_PLUGIN(${engine})
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/zlib ${CMAKE_SOURCE_DIR}/sql
                      ${CMAKE_SOURCE_DIR}/regex ${CMAKE_BINARY_DIR}/sql
                      ${CMAKE_SOURCE_DIR}/extra/yassl/include)
  IF(${ENGINE_BUILD_TYPE} STREQUAL "STATIC")
    ADD_DEFINITIONS(-DWITH_${engine}_STORAGE_ENGINE -DMYSQL_SERVER)
  ELSEIF(${ENGINE_BUILD_TYPE} STREQUAL "DYNAMIC")
    TARGET_LINK_LIBRARIES (${${engine}_LIB} mysqld)
  ENDIF(${ENGINE_BUILD_TYPE} STREQUAL "STATIC")
ENDIF(NOT SOURCE_SUBLIBS)
ENDMACRO(MYSQL_STORAGE_ENGINE)
