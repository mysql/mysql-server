# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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
