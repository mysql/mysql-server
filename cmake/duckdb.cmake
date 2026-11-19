# Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License, version 2.0, as published by the
# Free Software Foundation.
#
# This program is also distributed with certain software (including but not
# limited to OpenSSL) that is licensed under separate terms, as designated in a
# particular file or component or in included license documentation. The authors
# of MySQL hereby grant you an additional permission to link the program and
# your derivative works with the separately licensed software that they have
# included with MySQL.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
# for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

OPTION(WITH_DUCKDB_STORAGE_ENGINE "Enable the embedded DuckDB storage engine"
  ${LINUX})
SET(HAVE_DUCKDB OFF)
SET(DUCKDB_LIBRARY "")

IF(WITH_DUCKDB_STORAGE_ENGINE AND NOT WITHOUT_SERVER AND NOT LINUX)
  MESSAGE(FATAL_ERROR
    "DuckDB currently supports Linux server builds only. "
    "Use -DWITH_DUCKDB_STORAGE_ENGINE=OFF on this platform.")
ENDIF()

MACRO(MYSQL_CHECK_DUCKDB)
  INCLUDE(ExternalProject)
  SET(HAVE_DUCKDB ON)
  SET(DUCKDB_SOURCE_DIR "${CMAKE_SOURCE_DIR}/extra/duckdb")
  SET(DUCKDB_GIT_DESCRIBE "v1.5.1-736-g8a66223119")
  SET(DUCKDB_BINARY_DIR "${CMAKE_BINARY_DIR}/extra/duckdb/${DUCKDB_GIT_DESCRIBE}")
  SET(DUCKDB_INCLUDE_DIR "${DUCKDB_SOURCE_DIR}/src/include")
  SET(MY_DUCKDB_LIB "${DUCKDB_BINARY_DIR}/libduckdb_bundle.a")

  ExternalProject_Add(duckdb_proj
    SOURCE_DIR "${DUCKDB_SOURCE_DIR}"
    BINARY_DIR "${DUCKDB_BINARY_DIR}"
    CMAKE_ARGS
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
      "-DCMAKE_AR=${CMAKE_AR}"
      "-DCMAKE_RANLIB=${CMAKE_RANLIB}"
      "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
      "-DOVERRIDE_GIT_DESCRIBE=${DUCKDB_GIT_DESCRIBE}"
      -DBUILD_SHELL=OFF -DBUILD_UNITTESTS=OFF -DBUILD_PYTHON=OFF
      -DBUILD_JEMALLOC=OFF -DEXTENSION_STATIC_BUILD=ON
      "-DENABLE_SANITIZER=${WITH_ASAN}" "-DENABLE_UBSAN=${WITH_UBSAN}"
      "-DENABLE_THREAD_SANITIZER=${WITH_TSAN}"
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
      COMMAND ${CMAKE_COMMAND}
        "-DDUCKDB_BINARY_DIR=${DUCKDB_BINARY_DIR}"
        "-DDUCKDB_BUNDLE=${MY_DUCKDB_LIB}"
        "-DDUCKDB_AR=${CMAKE_AR}"
        -P "${CMAKE_SOURCE_DIR}/cmake/duckdb_bundle.cmake"
    BUILD_BYPRODUCTS "${MY_DUCKDB_LIB}"
    INSTALL_COMMAND ""
    BUILD_ALWAYS ON
    LOG_CONFIGURE ON LOG_BUILD ON
    LOG_OUTPUT_ON_FAILURE ON)

  ADD_LIBRARY(libduckdb STATIC IMPORTED GLOBAL)
  SET_TARGET_PROPERTIES(libduckdb PROPERTIES
    IMPORTED_LOCATION "${MY_DUCKDB_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${DUCKDB_INCLUDE_DIR}"
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${DUCKDB_INCLUDE_DIR}")
  ADD_DEPENDENCIES(libduckdb duckdb_proj)
  SET(DUCKDB_LIBRARY libduckdb)
ENDMACRO()
