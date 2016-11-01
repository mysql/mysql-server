# Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.
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
INCLUDE(CheckFunctionExists)
INCLUDE(CheckLibraryExists)

# Solaris threads with POSIX semantics:
# http://docs.oracle.com/cd/E19455-01/806-5257/6je9h033k/index.html
ADD_DEFINITIONS(-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT)

# set the pthread library
SET(CMAKE_THREADS_LIBS_INIT -lpthread CACHE INTERNAL "" FORCE)

# This is used for the version_compile_machine variable.
IF(CMAKE_SIZEOF_VOID_P MATCHES 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
  SET(MYSQL_MACHINE_TYPE "x86_64")
ENDIF()

# Solaris 10 needs librt linked in because of the sched_yield symbol
# check if it is necessary to define it at all
IF(DEFINED MY_SEARCH_LIBS)
  # if built embedded in the MySQL Server
  MY_SEARCH_LIBS(sched_yield rt SCHED_YIELD_RETVAL)
  IF (DEFINED SCHED_YIELD_RETVAL AND ${SCHED_YIELD_RETVAL} MATCHES "rt")
    SET (LINK_WITH_LIBRT TRUE)
  ENDIF()
ELSE()
  #  Check libc for sched_yield
  CHECK_FUNCTION_EXISTS(sched_yield FOUND_SCHED_YIELD_IN_LIBC)
  IF (FOUND_SCHED_YIELD_IN_LIBC)
    SET (LINK_WITH_LIBRT FALSE)
  ELSE()
    CHECK_LIBRARY_EXISTS(rt sched_yield "" FOUND_SCHED_YIELD_IN_RT)
    IF(FOUND_SCHED_YIELD_IN_RT)
      SET (LINK_WITH_LIBRT TRUE)
    ENDIF()
  ENDIF()

  IF (NOT DEFINED LINK_WITH_LIBRT)
    MESSAGE(FATAL_ERROR "Unable to find symbol sched_yield.")
  ENDIF()
ENDIF()

IF (LINK_WITH_LIBRT)
  SET (XCOM_REQUIRED_LIBS ${XCOM_REQUIRED_LIBS} "rt")
ENDIF()
