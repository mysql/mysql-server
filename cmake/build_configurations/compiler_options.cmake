# Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
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
    SET(CMAKE_C_FLAGS_DEBUG          "${COMMON_C_FLAGS}")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_C_FLAGS}")
  ENDIF()
  IF(CMAKE_COMPILER_IS_GNUCXX)
    # GCC 6 has C++14 as default, set it explicitly to the old default.
    EXECUTE_PROCESS(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                    OUTPUT_VARIABLE GXX_VERSION)
    IF(GXX_VERSION VERSION_EQUAL 6.0 OR GXX_VERSION VERSION_GREATER 6.0)
      SET(COMMON_CXX_FLAGS             "${COMMON_CXX_FLAGS} -std=gnu++03")
    ENDIF()
    SET(COMMON_CXX_FLAGS               "-g -fabi-version=2 -fno-omit-frame-pointer -fno-strict-aliasing")
    # Disable inline optimizations for valgrind testing to avoid false positives
    IF(WITH_VALGRIND)
      SET(COMMON_CXX_FLAGS             "-fno-inline ${COMMON_CXX_FLAGS}")
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
    IF(CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL 6.0 OR
        CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
      IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
        SET(COMMON_CXX_FLAGS           "${COMMON_CXX_FLAGS} -std=gnu++03")
      ENDIF()
    ENDIF()
    SET(CMAKE_CXX_FLAGS_DEBUG          "${COMMON_CXX_FLAGS}")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 ${COMMON_CXX_FLAGS}")
  ENDIF()

  # Solaris flags
  IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    IF(CMAKE_SYSTEM_VERSION VERSION_GREATER "5.9")
      # Link mysqld with mtmalloc on Solaris 10 and later
      SET(WITH_MYSQLD_LDFLAGS "-lmtmalloc" CACHE STRING "")
    ENDIF()
    IF(CMAKE_C_COMPILER_ID MATCHES "SunPro")

      SET(SUNPRO_CXX_LIBRARY "stlport4" CACHE STRING
        "What C++ library to use. The server needs stlport4. It is possible to build the client libraries with -DWITHOUT_SERVER=1 -DSUNPRO_CXX_LIBRARY=Cstd")

      IF(CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
        SET(COMMON_C_FLAGS                   "-g -mt -fsimple=1 -ftrap=%none -nofstore -xbuiltin=%all -xlibmil -xlibmopt -xtarget=generic")
        SET(COMMON_CXX_FLAGS                 "-g0 -mt -fsimple=1 -ftrap=%none -nofstore -xbuiltin=%all -xlibmil -xlibmopt -xtarget=generic")
        # We have to specify "-xO1" for DEBUG flags here,
        # see http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=6879978
        SET(CMAKE_C_FLAGS_DEBUG              "-xO1 ${COMMON_C_FLAGS}")
        SET(CMAKE_CXX_FLAGS_DEBUG            "-xO1 ${COMMON_CXX_FLAGS}")
        IF(32BIT)
          SET(CMAKE_C_FLAGS_RELWITHDEBINFO   "-xO2 ${COMMON_C_FLAGS}")
          SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-xO2 ${COMMON_CXX_FLAGS}")
        ELSEIF(64BIT)
          SET(CMAKE_C_FLAGS_RELWITHDEBINFO   "-xO3 ${COMMON_C_FLAGS}")
          SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-xO3 ${COMMON_CXX_FLAGS}")
        ENDIF()
      ELSE() 
        # Assume !x86 is SPARC
        SET(COMMON_C_FLAGS                 "-g -xstrconst -mt")
        SET(COMMON_CXX_FLAGS               "-g0 -mt")
        IF(32BIT)
          SET(COMMON_C_FLAGS               "${COMMON_C_FLAGS} -xarch=sparc")
          SET(COMMON_CXX_FLAGS             "${COMMON_CXX_FLAGS} -xarch=sparc")
	ENDIF()
        SET(CMAKE_C_FLAGS_DEBUG            "${COMMON_C_FLAGS}")
        SET(CMAKE_CXX_FLAGS_DEBUG          "${COMMON_CXX_FLAGS}")
        SET(CMAKE_C_FLAGS_RELWITHDEBINFO   "-xO3 ${COMMON_C_FLAGS}")
        SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-xO3 ${COMMON_CXX_FLAGS}")
      ENDIF()
    ENDIF()
  ENDIF()
ENDIF()
