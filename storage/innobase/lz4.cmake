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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

# This is the CMakeLists for InnoDB LZ4 support.

IF(WITH_LZ4)
  # If there is a path specified, don't use the default paths
  FIND_PATH(LZ4_INCLUDE lz4.h PATHS ${WITH_LZ4}/include NO_DEFAULT_PATH)
  FIND_LIBRARY(LZ4_LIB NAMES lz4 PATHS ${WITH_LZ4}/lib NO_DEFAULT_PATH)

  IF(LZ4_INCLUDE)
    INCLUDE_DIRECTORIES(${LZ4_INCLUDE})
  ENDIF ()

  IF(LZ4_LIB)
    LINK_LIBRARIES(${LZ4_LIB})
  ENDIF ()

  IF(NOT (LZ4_INCLUDE AND LZ4_LIB))
    MESSAGE(FATAL_ERROR "Can't find lz4.h or liblz4 in ${WITH_LZ4}")
  ENDIF ()
ELSE()
  CHECK_INCLUDE_FILES(lz4.h LZ4_INCLUDE)
  CHECK_LIBRARY_EXISTS(lz4 LZ4_compress_limitedOutput "" LZ4_LIB)
  IF (LZ4_INCLUDE AND LZ4_LIB)
    LINK_LIBRARIES(lz4)
  ENDIF()
ENDIF()

IF(LZ4_INCLUDE AND LZ4_LIB)
  ADD_DEFINITIONS(-DHAVE_LZ4=1)
ENDIF()
