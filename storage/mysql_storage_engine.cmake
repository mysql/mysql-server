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


MACRO(MYSQL_STORAGE_ENGINE engine)
IF(NOT SOURCE_SUBLIBS)
  # Add common include directories
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/zlib
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include)
  STRING(TOUPPER ${engine} engine)
  STRING(TOLOWER ${engine} libname)
  IF(${ENGINE_BUILD_TYPE} STREQUAL "STATIC")
    ADD_DEFINITIONS(-DWITH_${engine}_STORAGE_ENGINE -DMYSQL_SERVER)
    #Create static library. The name of the library is <storage_engine>.lib
    ADD_LIBRARY(${libname} ${${engine}_SOURCES})
    ADD_DEPENDENCIES(${libname} GenError)
    IF(${engine}_LIBS)
      TARGET_LINK_LIBRARIES(${libname} ${${engine}_LIBS})
    ENDIF(${engine}_LIBS)
    MESSAGE("build ${engine} as static library")
  ELSEIF(${ENGINE_BUILD_TYPE} STREQUAL "DYNAMIC")
    ADD_DEFINITIONS(-DMYSQL_DYNAMIC_PLUGIN)
    #Create a DLL.The name of the dll is ha_<storage_engine>.dll
    #The dll is linked to the mysqld executable
    SET(dyn_libname ha_${libname})
    ADD_LIBRARY(${dyn_libname} SHARED ${${engine}_SOURCES})
    TARGET_LINK_LIBRARIES (${dyn_libname}  mysqld)
    IF(${engine}_LIBS)
      TARGET_LINK_LIBRARIES(${dyn_libname} ${${engine}_LIBS})
    ENDIF(${engine}_LIBS)
    MESSAGE("build ${engine} as DLL")
  ENDIF(${ENGINE_BUILD_TYPE} STREQUAL "STATIC")
ENDIF(NOT SOURCE_SUBLIBS)
ENDMACRO(MYSQL_STORAGE_ENGINE)
