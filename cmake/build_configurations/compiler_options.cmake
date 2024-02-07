# Copyright (c) 2012, 2024, Oracle and/or its affiliates.
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

INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCXXCompilerFlag)
INCLUDE(cmake/floating_point.cmake)

SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler options
IF(UNIX)  

  IF(MY_COMPILER_IS_GNU_OR_CLANG AND NOT SOLARIS)
    SET(SECTIONS_FLAG "-ffunction-sections -fdata-sections")
  ELSE()
    SET(SECTIONS_FLAG)
  ENDIF()

  # Default GCC flags
  IF(MY_COMPILER_IS_GNU)
    SET(COMMON_C_FLAGS               "-fno-omit-frame-pointer")
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      STRING_PREPEND(COMMON_C_FLAGS  "-fno-inline ")
    ENDIF()
    # Disable floating point expression contractions to avoid result differences
    IF(HAVE_C_FLOATING_POINT_FUSED_MADD)
      STRING_APPEND(COMMON_C_FLAGS   " -ffp-contract=off")
    ENDIF()

    SET(COMMON_CXX_FLAGS             "-std=c++20 -fno-omit-frame-pointer")
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      STRING_PREPEND(COMMON_CXX_FLAGS  "-fno-inline ")
    ENDIF()
    # Disable floating point expression contractions to avoid result differences
    IF(HAVE_CXX_FLOATING_POINT_FUSED_MADD)
      STRING_APPEND(COMMON_CXX_FLAGS " -ffp-contract=off")
    ENDIF()
  ENDIF()

  # Default Clang flags
  IF(MY_COMPILER_IS_CLANG)
    SET(COMMON_C_FLAGS               "-fno-omit-frame-pointer")
    SET(COMMON_CXX_FLAGS             "-std=c++20 -fno-omit-frame-pointer")
  ENDIF()

  # Faster TLS model
  # libprotobuf-lite.so.24.4: cannot allocate memory in static TLS block
  IF(MY_COMPILER_IS_GNU_OR_CLANG
      AND NOT LINUX_ARM
      AND NOT SOLARIS AND NOT LINUX_RHEL6 AND NOT LINUX_ALPINE)
    STRING_APPEND(COMMON_C_FLAGS     " -ftls-model=initial-exec")
    STRING_APPEND(COMMON_CXX_FLAGS   " -ftls-model=initial-exec")
  ENDIF()

  # Use STRING_PREPEND here, so command-line input can override our defaults.
  STRING_PREPEND(CMAKE_C_FLAGS                  "${COMMON_C_FLAGS} ")
  STRING_PREPEND(CMAKE_C_FLAGS_RELWITHDEBINFO   "${SECTIONS_FLAG} ")
  STRING_PREPEND(CMAKE_C_FLAGS_RELEASE          "${SECTIONS_FLAG} ")
  STRING_PREPEND(CMAKE_C_FLAGS_MINSIZEREL       "${SECTIONS_FLAG} ")

  STRING_PREPEND(CMAKE_CXX_FLAGS                "${COMMON_CXX_FLAGS} ")
  STRING_PREPEND(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${SECTIONS_FLAG} ")
  STRING_PREPEND(CMAKE_CXX_FLAGS_RELEASE        "${SECTIONS_FLAG} ")
  STRING_PREPEND(CMAKE_CXX_FLAGS_MINSIZEREL     "${SECTIONS_FLAG} ")

ENDIF()
