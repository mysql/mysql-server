# Copyright (c) 2009, 2018, Oracle and/or its affiliates. All rights reserved.
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

MACRO (MYSQL_USE_BUNDLED_ZLIB)
  SET(BUILD_BUNDLED_ZLIB 1)
  SET(ZLIB_LIBRARY zlib CACHE INTERNAL "Bundled zlib library")
  SET(ZLIB_FOUND  TRUE)
  SET(WITH_ZLIB "bundled" CACHE STRING "Use bundled zlib")
  ADD_SUBDIRECTORY(extra/zlib)
ENDMACRO()

# MYSQL_CHECK_ZLIB_WITH_COMPRESS
#
# Provides the following configure options:
# WITH_ZLIB_BUNDLED
# If this is set,we use bindled zlib
# If this is not set,search for system zlib. 
# if system zlib is not found, use bundled copy
# ZLIB_LIBRARIES, ZLIB_INCLUDE_DIR and ZLIB_SOURCES
# are set after this macro has run

MACRO (MYSQL_CHECK_ZLIB_WITH_COMPRESS)

  IF(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Use bundled zlib on some platforms by default (system one is too
    # old or not existent)
    IF (NOT WITH_ZLIB)
      SET(WITH_ZLIB "bundled"  CACHE STRING "By default use bundled zlib on this platform")
    ENDIF()
  ENDIF()
  
  IF(WITH_ZLIB STREQUAL "bundled")
    MYSQL_USE_BUNDLED_ZLIB()
  ELSE()
    SET(ZLIB_FIND_QUIETLY TRUE)
    INCLUDE(FindZLIB)
    IF(ZLIB_FOUND)
      INCLUDE(CheckFunctionExists)
      SET(SAVE_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
      SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} z)
      CHECK_FUNCTION_EXISTS(crc32 HAVE_CRC32)
      CHECK_FUNCTION_EXISTS(compressBound HAVE_COMPRESSBOUND)
      CHECK_FUNCTION_EXISTS(deflateBound HAVE_DEFLATEBOUND)
      SET(CMAKE_REQUIRED_LIBRARIES ${SAVE_CMAKE_REQUIRED_LIBRARIES})
      IF(HAVE_CRC32 AND HAVE_COMPRESSBOUND AND HAVE_DEFLATEBOUND)
        SET(ZLIB_LIBRARY ${ZLIB_LIBRARIES} CACHE INTERNAL "System zlib library")
        SET(WITH_ZLIB "system" CACHE STRING
          "Which zlib to use (possible values are 'bundled' or 'system')")
        SET(ZLIB_SOURCES "")
      ELSE()
        SET(ZLIB_FOUND FALSE CACHE INTERNAL "Zlib found but not usable")
        MESSAGE(STATUS "system zlib found but not usable")
      ENDIF()
    ENDIF()
    IF(NOT ZLIB_FOUND)
      MYSQL_USE_BUNDLED_ZLIB()
    ENDIF()
  ENDIF()
ENDMACRO()
