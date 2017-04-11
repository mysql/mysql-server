# Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.
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

  # Default GCC flags
  IF(CMAKE_COMPILER_IS_GNUCC)
    SET(COMMON_C_FLAGS               "-g -fabi-version=2 -fno-omit-frame-pointer -fno-strict-aliasing")
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      SET(COMMON_C_FLAGS             "-fno-inline ${COMMON_C_FLAGS}")
    ENDIF()
    # Disable expensive-optimization if shift-or-optimization bug effective
    IF(HAVE_C_SHIFT_OR_OPTIMIZATION_BUG)
      SET(C_NO_EXPENSIVE_OPTIMIZATIONS TRUE)
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
    SET(CMAKE_C_FLAGS_DEBUG          "${COMMON_C_FLAGS}")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_C_FLAGS}")
  ENDIF()
  IF(CMAKE_COMPILER_IS_GNUCXX)
    SET(COMMON_CXX_FLAGS               "-g -fabi-version=2 -fno-omit-frame-pointer -fno-strict-aliasing")
    # GCC 6 has C++14 as default, set it explicitly to the old default.
    EXECUTE_PROCESS(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                    OUTPUT_VARIABLE GXX_VERSION)
    IF(GXX_VERSION VERSION_EQUAL 6.0 OR GXX_VERSION VERSION_GREATER 6.0)
      SET(COMMON_CXX_FLAGS             "${COMMON_CXX_FLAGS} -std=gnu++03")
    ENDIF()
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      SET(COMMON_CXX_FLAGS             "-fno-inline ${COMMON_CXX_FLAGS}")
    ENDIF()
    # Disable expensive-optimization if shift-or-optimization bug effective
    IF(HAVE_CXX_SHIFT_OR_OPTIMIZATION_BUG)
      SET(CXX_NO_EXPENSIVE_OPTIMIZATIONS TRUE)
    ENDIF()
    # Disable floating point expression contractions to avoid result differences
    IF(HAVE_CXX_FLOATING_POINT_FUSED_MADD)
      IF(HAVE_CXX_FP_CONTRACT_FLAG)
	SET(COMMON_CXX_FLAGS "${COMMON_CXX_FLAGS} -ffp-contract=off")
      ELSE()
        SET(CXX_NO_EXPENSIVE_OPTIMIZATIONS TRUE)
      ENDIF()
    ENDIF()
    IF(CXX_NO_EXPENSIVE_OPTIMIZATIONS)
      SET(COMMON_CXX_FLAGS "${COMMON_CXX_FLAGS} -fno-expensive-optimizations")
    ENDIF()
    SET(CMAKE_CXX_FLAGS_DEBUG          "${COMMON_CXX_FLAGS}")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_CXX_FLAGS}")
  ENDIF()

  # Default Clang flags
  IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
    SET(COMMON_C_FLAGS               "-g -fno-omit-frame-pointer -fno-strict-aliasing")
    SET(CMAKE_C_FLAGS_DEBUG          "${COMMON_C_FLAGS}")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_C_FLAGS}")
  ENDIF()
  IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    SET(COMMON_CXX_FLAGS               "-g -fno-omit-frame-pointer -fno-strict-aliasing")
    SET(CMAKE_CXX_FLAGS_DEBUG          "${COMMON_CXX_FLAGS}")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_CXX_FLAGS}")
  ENDIF()

  # Solaris flags
  IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    IF(CMAKE_SYSTEM_VERSION VERSION_GREATER "5.9")
      # Link mysqld with mtmalloc on Solaris 10 and later
      SET(WITH_MYSQLD_LDFLAGS "-lmtmalloc" CACHE STRING "")
    ENDIF() 
    # Possible changes to the defaults set above for gcc/linux.
    # Vectorized code dumps core in 32bit mode.
    IF(CMAKE_COMPILER_IS_GNUCC AND 32BIT)
      CHECK_C_COMPILER_FLAG("-ftree-vectorize" HAVE_C_FTREE_VECTORIZE)
      IF(HAVE_C_FTREE_VECTORIZE)
        SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fno-tree-vectorize")
      ENDIF()
    ENDIF()
    IF(CMAKE_COMPILER_IS_GNUCXX AND 32BIT)
      CHECK_CXX_COMPILER_FLAG("-ftree-vectorize" HAVE_CXX_FTREE_VECTORIZE)
      IF(HAVE_CXX_FTREE_VECTORIZE)
        SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-tree-vectorize")
      ENDIF()
    ENDIF()

    IF(CMAKE_C_COMPILER_ID MATCHES "SunPro")
      IF(DEFINED CC_MINOR_VERSION AND CC_MINOR_VERSION GREATER 12)
        SET(SUNPRO_FLAGS     "-xdebuginfo=no%decl")
      ENDIF()
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xbuiltin=%all")
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xlibmil")
      # Link with the libatomic library in /usr/lib
      # This prevents dependencies on libstatomic
      # This was introduced with developerstudio12.5
      IF(CC_MINOR_VERSION GREATER 13)
        SET(SUNPRO_FLAGS   "${SUNPRO_FLAGS} -xatomic=gcc")
      ENDIF()
      IF(CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
        SET(SUNPRO_FLAGS   "${SUNPRO_FLAGS} -nofstore")
      ENDIF()

      SET(COMMON_C_FLAGS            "-g ${SUNPRO_FLAGS}")
      SET(COMMON_CXX_FLAGS          "-g0 ${SUNPRO_FLAGS}")
      SET(CMAKE_C_FLAGS_DEBUG       "${COMMON_C_FLAGS}")
      SET(CMAKE_CXX_FLAGS_DEBUG     "${COMMON_CXX_FLAGS}")
      SET(CMAKE_C_FLAGS_RELWITHDEBINFO   "-xO3 ${COMMON_C_FLAGS}")
      SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-xO3 ${COMMON_CXX_FLAGS}")
    ENDIF()
  ENDIF()
ENDIF()
