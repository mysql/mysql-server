# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(CheckSymbolExists)
INCLUDE(CheckCSourceRuns)
INCLUDE(CheckCSourceCompiles)
INCLUDE(CheckCXXSourceCompiles)

# Solaris threads with POSIX semantics:
# http://docs.oracle.com/cd/E19455-01/806-5257/6je9h033k/index.html
ADD_DEFINITIONS(-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT)

# set the pthread library
SET(CMAKE_THREADS_LIBS_INIT -lpthread CACHE INTERNAL "" FORCE)

# This is used for the version_compile_machine variable.
IF(CMAKE_SIZEOF_VOID_P MATCHES 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
  SET(MYSQL_MACHINE_TYPE "x86_64")
ENDIF()

# add -m32 or -m64 to CMAKE_C_FLAGS, CMAKE_CXX_FLAGS and
# if needed
IF (NOT "${CMAKE_C_FLAGS}${CMAKE_CXX_FLAGS}" MATCHES "-m32|-m64")
  EXECUTE_PROCESS(COMMAND isainfo -b
    OUTPUT_VARIABLE ISAINFO_B
    RESULT_VARIABLE ISAINFO_B_RES
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  IF(ISAINFO_B_RES)
    MESSAGE(STATUS "Failed to run isainfo -b to determine arch bits: "
      "${ISAINFO_B_RES}. Falling back to compiler's default.")
  ELSE()
    MESSAGE("Adding -m${ISAINFO_B}")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m${ISAINFO_B}")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m${ISAINFO_B}")
  ENDIF()
ENDIF()

# Add the arch flags to the linking flags as well
IF (NOT "${CMAKE_SHARED_LIBRARY_C_FLAGS}" MATCHES "-m32|-m64")
  IF ("${CMAKE_C_FLAGS}${CMAKE_CXX_FLAGS}" MATCHES "-m32")
    SET(CMAKE_SHARED_LIBRARY_C_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS} -m32")
    SET(CMAKE_SHARED_LIBRARY_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CXX_FLAGS} -m32")
    MESSAGE("Adding -m32 to linkage library}")
  ELSEIF("${CMAKE_C_FLAGS}${CMAKE_CXX_FLAGS}" MATCHES "-m64")
    SET(CMAKE_SHARED_LIBRARY_C_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS} -m64")
    SET(CMAKE_SHARED_LIBRARY_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CXX_FLAGS} -m64")
    MESSAGE("Adding -m64 to linkage library")
  ELSE()
    MESSAGE(STATUS "Architecture flag not set to CMAKE_SHARED_LIBRARY_C_FLAGS."
            "Falling back to compiler's default.")
  ENDIF()
ENDIF()
