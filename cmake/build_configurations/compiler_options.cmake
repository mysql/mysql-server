# Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCXXCompilerFlag)
INCLUDE(cmake/compiler_bugs.cmake)
INCLUDE(cmake/floating_point.cmake)

IF(SIZEOF_VOIDP EQUAL 4)
  SET(32BIT 1)
ENDIF()
IF(SIZEOF_VOIDP EQUAL 8)
  SET(64BIT 1)
ENDIF()
 
# Compiler options
IF(UNIX)  

  IF(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    SET(SECTIONS_FLAG "-ffunction-sections -fdata-sections")
  ELSE()
    SET(SECTIONS_FLAG)
  ENDIF()

  # Default GCC flags
  IF(CMAKE_COMPILER_IS_GNUCC)
    SET(COMMON_C_FLAGS               "-fno-omit-frame-pointer")
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      STRING_PREPEND(COMMON_C_FLAGS  "-fno-inline ")
    ENDIF()
    # Disable floating point expression contractions to avoid result differences
    IF(HAVE_C_FLOATING_POINT_FUSED_MADD)
      SET(COMMON_C_FLAGS "${COMMON_C_FLAGS} -ffp-contract=off")
    ENDIF()
    IF(NOT DISABLE_SHARED)
      STRING_PREPEND(COMMON_C_FLAGS  "-fPIC ")
    ENDIF()
  ENDIF()
  IF(CMAKE_COMPILER_IS_GNUCXX)
    SET(COMMON_CXX_FLAGS               "-std=c++11 -fno-omit-frame-pointer")
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      STRING_PREPEND(COMMON_CXX_FLAGS  "-fno-inline ")
    ENDIF()
    # Disable floating point expression contractions to avoid result differences
    IF(HAVE_CXX_FLOATING_POINT_FUSED_MADD)
      STRING_APPEND(COMMON_CXX_FLAGS " -ffp-contract=off")
    ENDIF()
    IF(NOT DISABLE_SHARED)
      STRING_PREPEND(COMMON_CXX_FLAGS "-fPIC ")
    ENDIF()
  ENDIF()

  # Default Clang flags
  IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
    SET(COMMON_C_FLAGS               "-fno-omit-frame-pointer")
    IF(NOT DISABLE_SHARED)
      STRING_PREPEND(COMMON_C_FLAGS  "-fPIC ")
    ENDIF()
  ENDIF()
  IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    SET(COMMON_CXX_FLAGS               "-std=c++11 -fno-omit-frame-pointer")
    IF(NOT DISABLE_SHARED)
      STRING_PREPEND(COMMON_CXX_FLAGS  "-fPIC ")
    ENDIF()
  ENDIF()

  # Solaris flags
  IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    # Link mysqld with mtmalloc on Solaris 10 and later
    SET(WITH_MYSQLD_LDFLAGS "-lmtmalloc" CACHE STRING "")

    IF(CMAKE_C_COMPILER_ID MATCHES "SunPro")
      SET(SUNPRO_FLAGS     "")
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xbuiltin=%all")
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xlibmil")
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xatomic=studio")
      IF(NOT DISABLE_SHARED)
        SET(SUNPRO_FLAGS   "${SUNPRO_FLAGS} -KPIC")
      ENDIF()
      IF(CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
        SET(SUNPRO_FLAGS   "${SUNPRO_FLAGS} -nofstore")
      ENDIF()

      SET(COMMON_C_FLAGS            "${SUNPRO_FLAGS}")
      SET(COMMON_CXX_FLAGS          "-std=c++11 ${SUNPRO_FLAGS}")

      # Reduce size of debug binaries, by omitting function declarations.
      # Note that we cannot set "-xdebuginfo=no%decl" during feature tests.
      # Linking errors for merge_large_tests-t with Studio 12.6
      # -g0 is the same as -g, except that inlining is enabled.
      IF(${CC_MINOR_VERSION} EQUAL 15)
        STRING_APPEND(CMAKE_C_FLAGS_DEBUG          " -g0 -xdebuginfo=no%decl")
        STRING_APPEND(CMAKE_CXX_FLAGS_DEBUG        " -g0 -xdebuginfo=no%decl")
      ELSE()
        STRING_APPEND(CMAKE_C_FLAGS_DEBUG          " -xdebuginfo=no%decl")
        STRING_APPEND(CMAKE_CXX_FLAGS_DEBUG        " -xdebuginfo=no%decl")
      ENDIF()
      STRING_APPEND(CMAKE_C_FLAGS_RELWITHDEBINFO   " -xdebuginfo=no%decl")
      STRING_APPEND(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -xdebuginfo=no%decl")

      # Bugs in SunPro, compile/link error unless we add some debug info.
      # Errors seem to be related to TLS functions.
      STRING_APPEND(CMAKE_CXX_FLAGS_MINSIZEREL
        " -g0 -xdebuginfo=no%line,no%param,no%decl,no%variable,no%tagtype")
      STRING_APPEND(CMAKE_CXX_FLAGS_RELEASE
        " -g0 -xdebuginfo=no%line,no%param,no%decl,no%variable,no%tagtype")
    ENDIF()
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

STRING_APPEND(CMAKE_C_FLAGS   " ${COMMON_C_WORKAROUND_FLAGS}")
STRING_APPEND(CMAKE_CXX_FLAGS " ${COMMON_CXX_WORKAROUND_FLAGS}")
