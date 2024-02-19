# Copyright (c) 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE(ExternalProject)

SET(TCMALLOC_SOURCE_DIR
  "${CMAKE_SOURCE_DIR}/extra/gperftools/gperftools-2.15"
  )

FIND_PROGRAM(MAKE_EXECUTABLE make)

SET(TCMALLOC_CMAKE_ARGS
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  -DGPERFTOOLS_BUILD_STATIC=OFF
  -Dgperftools_build_benchmark=OFF
  -Dgperftools_enable_libunwind=OFF
  -DBUILD_TESTING=OFF
  -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/tcmalloc
  -DCMAKE_INSTALL_LIBDIR=${CMAKE_BINARY_DIR}/library_output_directory
)

IF(LINUX_RHEL AND ALTERNATIVE_GCC)
  LIST(APPEND TCMALLOC_CMAKE_ARGS -DCMAKE_C_FLAGS="-B${GCC_B_PREFIX}")
  LIST(APPEND TCMALLOC_CMAKE_ARGS -DCMAKE_CXX_FLAGS="-B${GCC_B_PREFIX}")
ENDIF()

IF(ALTERNATIVE_LD)
  LIST(APPEND TCMALLOC_CMAKE_ARGS -DCMAKE_LINKER=${ALTERNATIVE_LD})
ENDIF()

IF(ALTERNATIVE_AR)
  LIST(APPEND TCMALLOC_CMAKE_ARGS -DCMAKE_AR=${ALTERNATIVE_AR})
ENDIF()

IF(ALTERNATIVE_RANLIB)
  LIST(APPEND TCMALLOC_CMAKE_ARGS -DCMAKE_RANLIB=${ALTERNATIVE_RANLIB})
ENDIF()

EXTERNALPROJECT_ADD(tcmalloc_ext
  SOURCE_DIR ${TCMALLOC_SOURCE_DIR}
  CMAKE_ARGS ${TCMALLOC_CMAKE_ARGS}
  BUILD_BYPRODUCTS
  "${CMAKE_BINARY_DIR}/lib/libtcmalloc.so"
  "${CMAKE_BINARY_DIR}/lib/libtcmalloc.so.9.16.5"
  "${CMAKE_BINARY_DIR}/lib/libtcmalloc_debug.so"
  "${CMAKE_BINARY_DIR}/lib/libtcmalloc_debug.so.9.16.5"
  )

# Cannot use INSTALL_PRIVATE_LIBRARY because these are not targets.
INSTALL(FILES
  "${CMAKE_BINARY_DIR}/lib/libtcmalloc.so"
  "${CMAKE_BINARY_DIR}/lib/libtcmalloc.so.9.16.5"
  DESTINATION "${INSTALL_PRIV_LIBDIR}" COMPONENT SharedLibraries
  PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
)

ADD_LIBRARY(my_tcmalloc SHARED IMPORTED GLOBAL)
SET_TARGET_PROPERTIES(my_tcmalloc PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
  IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/lib/libtcmalloc.so")

ADD_LIBRARY(my_tcmalloc_debug SHARED IMPORTED GLOBAL)
SET_TARGET_PROPERTIES(my_tcmalloc_debug PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
  IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/lib/libtcmalloc_debug.so")

ADD_DEPENDENCIES(my_tcmalloc tcmalloc_ext)
ADD_DEPENDENCIES(my_tcmalloc_debug tcmalloc_ext)
