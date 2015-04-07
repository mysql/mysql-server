# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

# cmake -DWITH_LZ4=system|bundled
# bundled is the default

MACRO (FIND_SYSTEM_LZ4)
  FIND_PATH(PATH_TO_LZ4 NAMES lz4frame.h)
  FIND_LIBRARY(LZ4_SYSTEM_LIBRARY NAMES lz4)
  IF (PATH_TO_LZ4 AND LZ4_SYSTEM_LIBRARY)
    SET(SYSTEM_LZ4_FOUND 1)
    SET(LZ4_INCLUDE_DIR ${PATH_TO_LZ4})
    SET(LZ4_LIBRARY ${LZ4_SYSTEM_LIBRARY})
    MESSAGE(STATUS "LZ4_INCLUDE_DIR ${LZ4_INCLUDE_DIR}")
    MESSAGE(STATUS "LZ4_LIBRARY ${LZ4_LIBRARY}")
  ENDIF()
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_LZ4)
  SET(BUILD_BUNDLED_LZ4 1)
  SET(LZ4_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/extra/lz4)
  SET(LZ4_LIBRARY lz4_lib)
ENDMACRO()

IF (NOT WITH_LZ4)
  SET(WITH_LZ4 "bundled" CACHE STRING "By default use bundled lz4 library")
ENDIF()

MACRO (MYSQL_CHECK_LZ4)
  IF (WITH_LZ4 STREQUAL "bundled")
    MYSQL_USE_BUNDLED_LZ4()
  ELSEIF(WITH_LZ4 STREQUAL "system")
    FIND_SYSTEM_LZ4()
    IF (NOT SYSTEM_LZ4_FOUND)
      MESSAGE(FATAL_ERROR "Cannot find system lz4 libraries.") 
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "WITH_LZ4 must be bundled or system")
  ENDIF()
ENDMACRO()
