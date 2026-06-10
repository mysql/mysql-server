# Copyright (c) 2018, 2026, Oracle and/or its affiliates.
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

SET(TESTS
  allocator
  cell_calculator
  cell
  temptable-handler
  result
)

LIST(TRANSFORM TESTS APPEND "-t.cc" OUTPUT_VARIABLE ALL_TEMPTABLE_TESTS)

MYSQL_ADD_EXECUTABLE(merge_temptable_tests-t ${ALL_TEMPTABLE_TESTS}
  ENABLE_EXPORTS
  DEPENDENCIES GenError
  ADD_TEST merge_temptable_tests-t
  EXCLUDE_FROM_GCOV
  LINK_LIBRARIES gunit_large server_unittest_library perfschema
)

# Download TBB from https://www.threadingbuildingblocks.org
#SET(TBB_INCDIR /path/to/tbb43_20150611oss/include)
#SET(TBB_LIBDIR /path/to/tbb43_20150611oss/build/linux_intel64_gcc_cc4.9.1_libc2.12_kernel2.6.39_release)
#TARGET_LINK_LIBRARIES(merge_temptable_tests-t tbb -L${TBB_LIBDIR} -Wl,-rpath=${TBB_LIBDIR})
#INCLUDE_DIRECTORIES(${TBB_INCDIR})

FOREACH(test ${TESTS})
  MYSQL_ADD_EXECUTABLE(${test}-t ${test}-t.cc
    ENABLE_EXPORTS
    SKIP_INSTALL
    EXCLUDE_FROM_ALL
    LINK_LIBRARIES gunit_large server_unittest_library
    DEPENDENCIES GenError
    ADD_GCOV_TEST ${test}
  )
ENDFOREACH()
