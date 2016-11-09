# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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

##       ################################################################
## LZ4   ################################################################
##       ################################################################

# If both are set (e.g., -DLZ4_LIBRARY=lz4 -DLZ4_INCLUDE_DIR=/usr/include)
# then we need not do anything else. However, if only one of them is set
# then we need to bail out.
IF ((LZ4_LIBRARY AND NOT LZ4_INCLUDE_DIR) OR
    (LZ4_INCLUDE_DIR AND NOT LZ4_LIBRARY))
  MESSAGE(FATAL_ERROR
          "LZ4_LIBRARY and LZ4_INCLUDE_DIR cannot be set one without the other.")

## If none of them is set, then we need to use the bundled LZ4
##
## Note: if this is built embedded in MySQL, then these two variable will
## have been set already, thence this is not done.
ELSEIF (NOT LZ4_LIBRARY AND NOT LZ4_INCLUDE_DIR)
  #
  # Use the bundled one if LZ4_LIBRARY is not set
  #
  SET(LZ4_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/extra/lz4)
  SET(LZ4_LIBRARY lz4_lib)

  # Add a target to build the library
  ADD_CONVENIENCE_LIBRARY(${LZ4_LIBRARY}
    lz4/lz4.c
    lz4/lz4frame.c
    lz4/lz4hc.c
    lz4/xxhash.c
  )

  # Suppress the warnings in lz4
  IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
      CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    SET_TARGET_PROPERTIES(${LZ4_LIBRARY} PROPERTIES COMPILE_FLAGS
      "-Wno-conversion -Wno-sign-conversion")
  ENDIF()
ENDIF()

SET (GCS_LZ4_LIBRARY ${LZ4_LIBRARY} CACHE INTERNAL "")
SET (GCS_LZ4_INCLUDE_DIR ${LZ4_INCLUDE_DIR} CACHE INTERNAL "")

MESSAGE(STATUS "GCS_LZ4_INCLUDE_DIR: ${GCS_LZ4_INCLUDE_DIR}")
MESSAGE(STATUS "GCS_LZ4_LIBRARY: ${GCS_LZ4_LIBRARY}")

##              ################################################################
## End of LZ4   ################################################################
##              ################################################################
