# Copyright (c) 2012, 2022, Oracle and/or its affiliates.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

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
    SET(COMMON_C_FLAGS "-fabi-version=2 -fno-omit-frame-pointer -fno-strict-aliasing")
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      STRING_PREPEND(COMMON_C_FLAGS "-fno-inline ")
    ENDIF()
    # Disable floating point expression contractions to avoid result differences
    IF(HAVE_C_FLOATING_POINT_FUSED_MADD)
      IF(HAVE_C_FP_CONTRACT_FLAG)
        SET(COMMON_C_FLAGS "${COMMON_C_FLAGS} -ffp-contract=off")
      ELSE()
        SET(C_NO_EXPENSIVE_OPTIMIZATIONS TRUE)
      ENDIF()
    ENDIF()
    IF(C_NO_EXPENSIVE_OPTIMIZATIONS)
      SET(COMMON_C_FLAGS "${COMMON_C_FLAGS} -fno-expensive-optimizations")
    ENDIF()
    IF(NOT DISABLE_SHARED)
      STRING_PREPEND(COMMON_C_FLAGS  "-fPIC ")
    ENDIF()
  ENDIF()
  IF(CMAKE_COMPILER_IS_GNUCXX)
    SET(COMMON_CXX_FLAGS               "-fabi-version=2 -fno-omit-frame-pointer -fno-strict-aliasing")
    # GCC 6 has C++14 as default, set it explicitly to the old default.
    EXECUTE_PROCESS(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                    OUTPUT_VARIABLE GXX_VERSION)
    IF(GXX_VERSION VERSION_EQUAL 6.0 OR GXX_VERSION VERSION_GREATER 6.0)
      STRING_PREPEND(COMMON_CXX_FLAGS "-std=gnu++03 ")
    ENDIF()
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      STRING_PREPEND(COMMON_CXX_FLAGS "-fno-inline ")
    ENDIF()
    # Disable floating point expression contractions to avoid result differences
    IF(HAVE_CXX_FLOATING_POINT_FUSED_MADD)
      IF(HAVE_CXX_FP_CONTRACT_FLAG)
	STRING_APPEND(COMMON_CXX_FLAGS " -ffp-contract=off")
      ELSE()
        SET(CXX_NO_EXPENSIVE_OPTIMIZATIONS TRUE)
      ENDIF()
    ENDIF()
    IF(CXX_NO_EXPENSIVE_OPTIMIZATIONS)
      STRING_APPEND(COMMON_CXX_FLAGS " -fno-expensive-optimizations")
    ENDIF()
    IF(NOT DISABLE_SHARED)
      STRING_PREPEND(COMMON_CXX_FLAGS "-fPIC ")
    ENDIF()

  ENDIF()

  # Default Clang flags
  IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
    SET(COMMON_C_FLAGS "-fno-omit-frame-pointer -fno-strict-aliasing")
    IF(NOT DISABLE_SHARED)
      STRING_PREPEND(COMMON_C_FLAGS  "-fPIC ")
    ENDIF()
  ENDIF()
  IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    SET(COMMON_CXX_FLAGS "-fno-omit-frame-pointer -fno-strict-aliasing")
    IF(CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL 6.0 OR
        CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
      IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
        STRING_PREPEND(COMMON_CXX_FLAGS "-std=gnu++03 ")
      ENDIF()
    ENDIF()
    IF(NOT DISABLE_SHARED)
      STRING_PREPEND(COMMON_CXX_FLAGS  "-fPIC ")
    ENDIF()
  ENDIF()

  # Solaris flags
  IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    # Link mysqld with mtmalloc on Solaris 10 and later
    SET(WITH_MYSQLD_LDFLAGS "-lmtmalloc" CACHE STRING "")

    # Possible changes to the defaults set above for gcc/linux.
    # Vectorized code dumps core in 32bit mode.
    IF(CMAKE_COMPILER_IS_GNUCC AND 32BIT)
      CHECK_C_COMPILER_FLAG("-ftree-vectorize" HAVE_C_FTREE_VECTORIZE)
      IF(HAVE_C_FTREE_VECTORIZE)
        STRING_APPEND(COMMON_C_FLAGS " -fno-tree-vectorize")
      ENDIF()
    ENDIF()
    IF(CMAKE_COMPILER_IS_GNUCXX AND 32BIT)
      CHECK_CXX_COMPILER_FLAG("-ftree-vectorize" HAVE_CXX_FTREE_VECTORIZE)
      IF(HAVE_CXX_FTREE_VECTORIZE)
        STRING_APPEND(COMMON_CXX_FLAGS " -fno-tree-vectorize")
      ENDIF()
    ENDIF()

    IF(CMAKE_C_COMPILER_ID MATCHES "SunPro")
      # Reduce size of debug binaries, by omitting function declarations.
      SET(SUNPRO_FLAGS     "-xdebuginfo=no%decl")
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xbuiltin=%all")
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xlibmil")
      # Link with the libatomic library in /usr/lib
      # This prevents dependencies on libstatomic
      # This was introduced with developerstudio12.5
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xatomic=gcc")

      IF(CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
        SET(SUNPRO_FLAGS   "${SUNPRO_FLAGS} -nofstore")
      ENDIF()

      SET(COMMON_C_FLAGS            "-g ${SUNPRO_FLAGS}")
      SET(COMMON_CXX_FLAGS          "-g0 ${SUNPRO_FLAGS} -std=c++03")
      # For SunPro, append our own flags rather than prepending below.
      # We need -g0 and the misc -x flags above to reduce the size of binaries.
      STRING_APPEND(CMAKE_C_FLAGS_DEBUG            " ${COMMON_C_FLAGS}")
      STRING_APPEND(CMAKE_CXX_FLAGS_DEBUG          " ${COMMON_CXX_FLAGS}")
      STRING_APPEND(CMAKE_C_FLAGS_RELWITHDEBINFO   " -xO3 ${COMMON_C_FLAGS}")
      STRING_APPEND(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -xO3 ${COMMON_CXX_FLAGS}")
      STRING_APPEND(CMAKE_C_FLAGS_RELEASE          " -xO3 ${COMMON_C_FLAGS}")
      STRING_APPEND(CMAKE_CXX_FLAGS_RELEASE        " -xO3 ${COMMON_CXX_FLAGS}")
      STRING_APPEND(CMAKE_C_FLAGS_MINSIZEREL       " -xO3 ${COMMON_C_FLAGS}")
      STRING_APPEND(CMAKE_CXX_FLAGS_MINSIZEREL     " -xO3 ${COMMON_CXX_FLAGS}")
      SET(COMMON_C_FLAGS "")
      SET(COMMON_CXX_FLAGS "")
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

  # We need -O3 by default for RelWithDebInfo in order to avoid
  # performance regressions from earlier releases.
  # To disable this (and everything else in this file),
  # do 'cmake -DWITH_DEFAULT_COMPILER_OPTIONS=NO'.
  IF(LINUX)
    FOREACH(flag
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        )
      STRING(REPLACE "-O2"  "-O3" "${flag}" "${${flag}}")
    ENDFOREACH()
  ENDIF()

ENDIF()

SET(CMAKE_C_FLAGS_DEBUG
      "${CMAKE_C_FLAGS_DEBUG} ${COMMON_C_WORKAROUND_FLAGS}")
SET(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} ${COMMON_CXX_WORKAROUND_FLAGS}")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO
      "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${COMMON_C_WORKAROUND_FLAGS}")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${COMMON_CXX_WORKAROUND_FLAGS}")
SET(CMAKE_C_FLAGS_RELEASE
      "${CMAKE_C_FLAGS_RELEASE} ${COMMON_C_WORKAROUND_FLAGS}")
SET(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${COMMON_CXX_WORKAROUND_FLAGS}")
