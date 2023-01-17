# Copyright (c) 2006, 2023, Oracle and/or its affiliates.
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

IF (WIN32)
  MYSQL_ADD_EXECUTABLE(mysqltest_safe_process safe_process_win.cc
    COMPONENT Test)
  MYSQL_ADD_EXECUTABLE(mysqltest_safe_kill safe_kill_win.cc
    COMPONENT Test)
ELSE()
  MYSQL_ADD_EXECUTABLE(mysqltest_safe_process safe_process.cc
    COMPONENT Test)
ENDIF()

IF(LINUX AND HAVE_ASAN)
  MYSQL_CHECK_RPC()
ENDIF()
# Sun RPC, and XDR, is being removed from glibc, and into a separate libtirpc
# library. This is not compatible with libasan. The interceptor functions
# inserted into the code will segfault.
# As a workaround, do LD_PRELOAD=/lib64/libtirpc.so
# For dynamically linked libasan (default for gcc), we must preload that as well
IF(HAVE_ASAN AND TIRPC_FOUND)
  TARGET_INCLUDE_DIRECTORIES(mysqltest_safe_process
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

  ADD_CUSTOM_COMMAND(
    OUTPUT asan_test.c
    COMMAND ${CMAKE_COMMAND} -E echo
      "extern int xdr_void(); int main() { xdr_void(); return 0; }"
      > asan_test.c
    VERBATIM
    )
  SET_SOURCE_FILES_PROPERTIES(asan_test.c PROPERTIES GENERATED TRUE)

  ADD_EXECUTABLE(asan_test asan_test.c)
  TARGET_LINK_LIBRARIES(asan_test ${TIRPC_LIBRARIES})
  ADD_CUSTOM_COMMAND(
    OUTPUT ldd_asan_test_result
    DEPENDS asan_test
    COMMAND ldd asan_test > ldd_asan_test_result)

  ADD_CUSTOM_TARGET(extract_asan_library_name
    DEPENDS ldd_asan_test_result
    COMMAND ${CMAKE_COMMAND}
    -DINFILE=${CMAKE_CURRENT_BINARY_DIR}/ldd_asan_test_result
    -DOUTFILE=${CMAKE_CURRENT_BINARY_DIR}/asan_library_name.h
    -P ${CMAKE_CURRENT_SOURCE_DIR}/read_ldd_output.cmake
    )

  ADD_DEPENDENCIES(mysqltest_safe_process extract_asan_library_name)
ENDIF()

IF(INSTALL_MYSQLTESTDIR)
  INSTALL(FILES Base.pm
    DESTINATION "${INSTALL_MYSQLTESTDIR}/lib/My/SafeProcess" COMPONENT Test
    )
ENDIF()
