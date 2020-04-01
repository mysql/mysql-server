# Copyright (c) 2009, 2020, Oracle and/or its affiliates. All rights reserved.
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
  # Reset whatever FIND_PACKAGE may have left behind.
  FOREACH(zlibvar
      INCLUDE_DIR
      LIBRARY
      LIBRARY_DEBUG
      LIBRARY_RELEASE)
    UNSET(ZLIB_${zlibvar})
    UNSET(ZLIB_${zlibvar} CACHE)
    UNSET(ZLIB_${zlibvar}-ADVANCED CACHE)
  ENDFOREACH()
  SET(BUILD_BUNDLED_ZLIB 1)
  SET(ZLIB_LIBRARY zlib CACHE INTERNAL "Bundled zlib library")
  SET(ZLIB_FOUND  TRUE)
  SET(WITH_ZLIB "bundled" CACHE STRING "Use bundled zlib")
  INCLUDE_DIRECTORIES(BEFORE SYSTEM
    ${CMAKE_SOURCE_DIR}/extra/zlib
    ${CMAKE_BINARY_DIR}/extra/zlib
    )
  ADD_SUBDIRECTORY(extra/zlib)
ENDMACRO()

# MYSQL_CHECK_ZLIB_WITH_COMPRESS
#
# Usage:
#  cmake -DWITH_ZLIB="bundled"|"system"
#
# Default is "bundled" on windows.
# The default should be "system" on other platforms, but
#   - all RPM/DEB packages require zlib_decompress executable
#   - rpl.rpl_connection_compression times out with system zlib
#   - main.compression fails on several platforms with system zlib
# If the system zlib does not support required features,
# we fall back to "bundled".
MACRO (MYSQL_CHECK_ZLIB_WITH_COMPRESS)

  IF(NOT WITH_ZLIB)
    SET(WITH_ZLIB "bundled"
      CACHE STRING "By default use bundled zlib on this platform")
  ENDIF()
  
  IF(WITH_ZLIB STREQUAL "bundled")
    MYSQL_USE_BUNDLED_ZLIB()
  ELSE()
    FIND_PACKAGE(ZLIB)
    IF(ZLIB_FOUND)
      INCLUDE(CheckFunctionExists)

      CMAKE_PUSH_CHECK_STATE()
      LIST(APPEND CMAKE_REQUIRED_LIBRARIES z)
      CHECK_FUNCTION_EXISTS(crc32 HAVE_CRC32)
      CHECK_FUNCTION_EXISTS(compressBound HAVE_COMPRESSBOUND)
      CHECK_FUNCTION_EXISTS(deflateBound HAVE_DEFLATEBOUND)
      CMAKE_POP_CHECK_STATE()

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
