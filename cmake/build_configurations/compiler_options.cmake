# Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.
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

  # Default GCC flags
  IF(CMAKE_COMPILER_IS_GNUCC)
    SET(COMMON_C_FLAGS               "-g -fno-omit-frame-pointer")
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      SET(COMMON_C_FLAGS             "-fno-inline ${COMMON_C_FLAGS}")
    ENDIF()
    # Disable floating point expression contractions to avoid result differences
    IF(HAVE_C_FLOATING_POINT_FUSED_MADD)
      SET(COMMON_C_FLAGS "${COMMON_C_FLAGS} -ffp-contract=off")
    ENDIF()
    IF(NOT DISABLE_SHARED)
      SET(COMMON_C_FLAGS             "-fPIC ${COMMON_C_FLAGS}")
    ENDIF()
    SET(CMAKE_C_FLAGS_DEBUG          "${COMMON_C_FLAGS}")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -ffunction-sections -fdata-sections ${COMMON_C_FLAGS}")
  ENDIF()
  IF(CMAKE_COMPILER_IS_GNUCXX)
    SET(COMMON_CXX_FLAGS               "-g -fno-omit-frame-pointer -std=c++11")
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      SET(COMMON_CXX_FLAGS             "-fno-inline ${COMMON_CXX_FLAGS}")
    ENDIF()
    # Disable floating point expression contractions to avoid result differences
    IF(HAVE_CXX_FLOATING_POINT_FUSED_MADD)
      SET(COMMON_CXX_FLAGS "${COMMON_CXX_FLAGS} -ffp-contract=off")
    ENDIF()
    IF(NOT DISABLE_SHARED)
      SET(COMMON_CXX_FLAGS             "-fPIC ${COMMON_CXX_FLAGS}")
    ENDIF()
    SET(CMAKE_CXX_FLAGS_DEBUG          "${COMMON_CXX_FLAGS}")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -ffunction-sections -fdata-sections ${COMMON_CXX_FLAGS}")
    # -std=c++11 must be set
    SET(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")
  ENDIF()

  # Default Clang flags
  IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
    SET(COMMON_C_FLAGS               "-g -fno-omit-frame-pointer")
    IF(NOT DISABLE_SHARED)
      SET(COMMON_C_FLAGS             "-fPIC ${COMMON_C_FLAGS}")
    ENDIF()
    SET(CMAKE_C_FLAGS_DEBUG          "${COMMON_C_FLAGS}")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -ffunction-sections -fdata-sections ${COMMON_C_FLAGS}")
  ENDIF()
  IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    SET(COMMON_CXX_FLAGS               "-g -fno-omit-frame-pointer -std=c++11")
    IF(NOT DISABLE_SHARED)
      SET(COMMON_CXX_FLAGS             "-fPIC ${COMMON_CXX_FLAGS}")
    ENDIF()
    SET(CMAKE_CXX_FLAGS_DEBUG          "${COMMON_CXX_FLAGS}")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -ffunction-sections -fdata-sections ${COMMON_CXX_FLAGS}")
    # -std=c++11 must be set
    SET(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")
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
      SET(SUNPRO_FLAGS     "-xdebuginfo=no%decl")
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xbuiltin=%all")
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xlibmil")
      SET(SUNPRO_FLAGS     "${SUNPRO_FLAGS} -xatomic=studio")
      IF(NOT DISABLE_SHARED)
        SET(SUNPRO_FLAGS   "${SUNPRO_FLAGS} -KPIC")
      ENDIF()
      IF(CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
        SET(SUNPRO_FLAGS   "${SUNPRO_FLAGS} -nofstore")
      ENDIF()

      # -std=c++11 must be set
      SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")
      IF(NOT DISABLE_SHARED)
        SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -KPIC")
      ENDIF()

      SET(COMMON_C_FLAGS            "-g ${SUNPRO_FLAGS}")
      SET(COMMON_CXX_FLAGS          "-g0 -std=c++11 ${SUNPRO_FLAGS}")
      SET(CMAKE_C_FLAGS_DEBUG       "${COMMON_C_FLAGS}")
      SET(CMAKE_CXX_FLAGS_DEBUG     "${COMMON_CXX_FLAGS}")
      SET(CMAKE_C_FLAGS_RELWITHDEBINFO   "-xO3 ${COMMON_C_FLAGS}")
      SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-xO3 ${COMMON_CXX_FLAGS}")
    ENDIF()
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
