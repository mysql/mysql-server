# Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
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

# cmake -DWITH_LZ4=system|bundled
# bundled is the default

MACRO (FIND_SYSTEM_LZ4)
  FIND_PATH(LZ4_INCLUDE_DIR
    NAMES lz4frame.h)
  FIND_LIBRARY(LZ4_SYSTEM_LIBRARY
    NAMES lz4)
  IF (LZ4_INCLUDE_DIR AND LZ4_SYSTEM_LIBRARY)
    SET(SYSTEM_LZ4_FOUND 1)
    SET(LZ4_LIBRARY ${LZ4_SYSTEM_LIBRARY})
  ENDIF()
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_LZ4)
  SET(WITH_LZ4 "bundled" CACHE STRING "Bundled lz4 library")
  SET(BUILD_BUNDLED_LZ4 1)
  INCLUDE_DIRECTORIES(BEFORE SYSTEM ${CMAKE_SOURCE_DIR}/extra/lz4)
  SET(LZ4_LIBRARY lz4_lib)
  ADD_LIBRARY(lz4_lib STATIC
    ${CMAKE_SOURCE_DIR}/extra/lz4/lz4.c
    ${CMAKE_SOURCE_DIR}/extra/lz4/lz4frame.c
    ${CMAKE_SOURCE_DIR}/extra/lz4/lz4hc.c
    ${CMAKE_SOURCE_DIR}/extra/lz4/xxhash.c
    )
ENDMACRO()

MACRO (MYSQL_CHECK_LZ4)
  IF(NOT WITH_LZ4)
    SET(WITH_LZ4 "bundled" CACHE STRING "By default use bundled lz4 library")
  ENDIF()

  IF(WITH_LZ4 STREQUAL "bundled")
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
