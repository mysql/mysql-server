# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

# cmake -DWITH_LZMA=system|bundled
# bundled is the default

MACRO (FIND_SYSTEM_LZMA)
  FIND_PATH(PATH_TO_LZMA NAMES lzma/lzma.h)
  FIND_LIBRARY(LZMA_SYSTEM_LIBRARY NAMES lzma)
  IF (PATH_TO_LZMA AND LZMA_SYSTEM_LIBRARY)
    SET(SYSTEM_LZMA_FOUND 1)
    SET(LZMA_INCLUDE_DIR ${PATH_TO_LZMA})
    SET(LZMA_LIBRARY ${LZMA_SYSTEM_LIBRARY})
    MESSAGE(STATUS "LZMA_INCLUDE_DIR ${LZMA_INCLUDE_DIR}")
    MESSAGE(STATUS "LZMA_LIBRARY ${LZMA_LIBRARY}")
  ENDIF()
ENDMACRO()

MACRO (MYSQL_USE_BUNDLED_LZMA)
  SET(WITH_LZMA "bundled" CACHE STRING "By default use bundled lzma library")
  SET(BUILD_BUNDLED_LZMA 1)
  SET(LZMA_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/extra/lzma)
  SET(LZMA_LIBRARY lzma_lib)
  MESSAGE(STATUS "LZMA_INCLUDE_DIR ${LZMA_INCLUDE_DIR}")
  MESSAGE(STATUS "LZMA_LIBRARY ${LZMA_LIBRARY}")
ENDMACRO()

IF (NOT WITH_LZMA)
  SET(WITH_LZMA "bundled" CACHE STRING "By default use bundled lzma library")
ENDIF()

MACRO (MYSQL_CHECK_LZMA)
  IF (WITH_LZMA STREQUAL "bundled")
    MYSQL_USE_BUNDLED_LZMA()
  ELSEIF(WITH_LZMA STREQUAL "system")
    FIND_SYSTEM_LZMA()
    IF (NOT SYSTEM_LZMA_FOUND)
      MESSAGE(FATAL_ERROR "Cannot find system lzma libraries.") 
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "WITH_LZMA must be bundled or system")
  ENDIF()
ENDMACRO()
