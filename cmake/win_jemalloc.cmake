# Copyright (c) 2021, 2022, Oracle and/or its affiliates.
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

# Using jemalloc on Windows is optional.
# cmake -DWITH_WIN_JEMALLOC=</path/to/dir_containing_jemalloc_dll>

MACRO (MYSQL_CHECK_WIN_JEMALLOC)
  IF(WIN32 AND WITH_WIN_JEMALLOC)
    SET(JEMALLOC_DLL_NAME "jemalloc.dll")
    FILE(TO_NATIVE_PATH "${WITH_WIN_JEMALLOC}" NATIVE_JEMALLOC_DIR_NAME)
    FIND_FILE(HAVE_JEMALLOC_DLL
      NAMES
      ${JEMALLOC_DLL_NAME}
      PATHS "${NATIVE_JEMALLOC_DIR_NAME}"
      NO_DEFAULT_PATH
    )

    IF(HAVE_JEMALLOC_DLL)
      MY_ADD_CUSTOM_TARGET(copy_jemalloc_dll ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${HAVE_JEMALLOC_DLL}"
        "${CMAKE_BINARY_DIR}/runtime_output_directory/${CMAKE_CFG_INTDIR}/${JEMALLOC_DLL_NAME}"
        )
      MESSAGE(STATUS "INSTALL ${HAVE_JEMALLOC_DLL} to ${INSTALL_BINDIR}")
      INSTALL(FILES
        "${HAVE_JEMALLOC_DLL}"
        DESTINATION "${INSTALL_BINDIR}" COMPONENT SharedLibraries)
    ELSE()
      MESSAGE(FATAL_ERROR
        "Cannot find ${JEMALLOC_DLL_NAME} in ${WITH_WIN_JEMALLOC}")
    ENDIF()
  ENDIF()
ENDMACRO()
