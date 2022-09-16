# Copyright (c) 2013, 2022, Oracle and/or its affiliates.
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

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/storage/innobase/include
)

INCLUDE(${CMAKE_SOURCE_DIR}/storage/innobase/innodb.cmake)

IF(WITH_LTO OR CMAKE_COMPILER_FLAG_WITH_LTO)
  IF(MY_COMPILER_IS_GNU AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
    # -Walloc-size-larger-than=PTRDIFF_MAX is enabled by default.
    STRING_APPEND(CMAKE_EXE_LINKER_FLAGS " -Wno-alloc-size-larger-than")
  ENDIF()
ENDIF()

SET(TESTS
  #example
  fil_path
  ha_innodb
  log0log
  mem0mem
  os0file
  os0thread-create
  srv0conc
  sync0rw
  ut0crc32
  ut0lock_free_hash
  ut0math
  ut0mem
  ut0new
  ut0rnd
)

ADD_COMPILE_FLAGS(
  ut0rnd-t.cc
  COMPILE_FLAGS -I${CMAKE_SOURCE_DIR}/extra/lz4 -I${BUNDLED_LZ4_PATH}
  )

# Treat warnings as errors on MSVC for InnoDB's unittests
IF(MSVC AND NOT WIN32_CLANG)
  ADD_COMPILE_OPTIONS("/WX")
ENDIF()

# Fixes "C1128: number of sections exceeded object file format limit" in MSVC
IF(MSVC)
  ADD_COMPILE_FLAGS(
    ut0new-t.cc
    COMPILE_FLAGS "/bigobj"
    )
ENDIF()

SET(ALL_INNODB_TESTS)
FOREACH(test ${TESTS})
  LIST(APPEND ALL_INNODB_TESTS ${test}-t.cc)
ENDFOREACH()

IF(WIN32)
  LIST(APPEND ALL_INNODB_TESTS ../../../sql/nt_servc.cc)
ENDIF()
MYSQL_ADD_EXECUTABLE(merge_innodb_tests-t ${ALL_INNODB_TESTS}
  ENABLE_EXPORTS
  ADD_TEST merge_innodb_tests-t
  LINK_LIBRARIES gunit_large server_unittest_library
  )

# Download TBB from https://www.threadingbuildingblocks.org
#SET(TBB_INCDIR /path/to/tbb43_20150611oss/include)
#SET(TBB_LIBDIR /path/to/tbb43_20150611oss/build/linux_intel64_gcc_cc4.9.1_libc2.12_kernel2.6.39_release)
#TARGET_LINK_LIBRARIES(merge_innodb_tests-t tbb -L${TBB_LIBDIR} -Wl,-rpath=${TBB_LIBDIR})
#INCLUDE_DIRECTORIES(${TBB_INCDIR})

ADD_DEPENDENCIES(merge_innodb_tests-t GenError)

FOREACH(test ${TESTS})
  SET(SRC_FILES ${test}-t.cc)
  IF(WIN32)
    LIST(APPEND SRC_FILES ../../../sql/nt_servc.cc)
  ENDIF()
  MYSQL_ADD_EXECUTABLE(${test}-t ${SRC_FILES}
    ENABLE_EXPORTS
    EXCLUDE_FROM_ALL
    SKIP_INSTALL
    LINK_LIBRARIES gunit_large server_unittest_library
    DEPENDENCIES GenError
    )
ENDFOREACH()

ADD_SUBDIRECTORY(lob)
