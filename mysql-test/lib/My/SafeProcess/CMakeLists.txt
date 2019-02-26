# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
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

IF (NOT INSTALL_MYSQLTESTDIR)
  RETURN()
ENDIF()

SET(INSTALL_ARGS 
  DESTINATION "${INSTALL_MYSQLTESTDIR}/lib/My/SafeProcess" 
  COMPONENT Test
)

IF (WIN32)
  MYSQL_ADD_EXECUTABLE(my_safe_process safe_process_win.cc ${INSTALL_ARGS})
  MYSQL_ADD_EXECUTABLE(my_safe_kill safe_kill_win.cc ${INSTALL_ARGS})
ELSE()
  MYSQL_ADD_EXECUTABLE(my_safe_process safe_process.cc ${INSTALL_ARGS})
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
  TARGET_INCLUDE_DIRECTORIES(my_safe_process
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

  ADD_CUSTOM_COMMAND(
    OUTPUT asan_test.c
    COMMAND ${CMAKE_COMMAND} -E echo "int main() { return 0; }" > asan_test.c
    VERBATIM
    )
  SET_SOURCE_FILES_PROPERTIES(asan_test.c PROPERTIES GENERATED TRUE)

  ADD_EXECUTABLE(asan_test asan_test.c)
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

  ADD_DEPENDENCIES(my_safe_process extract_asan_library_name)
ENDIF()

INSTALL(TARGETS my_safe_process
  DESTINATION "${INSTALL_MYSQLTESTDIR}/lib/My/SafeProcess" COMPONENT Test
)

IF(WIN32)
  INSTALL(TARGETS my_safe_kill
    DESTINATION "${INSTALL_MYSQLTESTDIR}/lib/My/SafeProcess" COMPONENT Test
  )
ENDIF()

INSTALL(FILES Base.pm
  DESTINATION "${INSTALL_MYSQLTESTDIR}/lib/My/SafeProcess" COMPONENT Test
)
