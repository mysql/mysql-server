# Copyright (c) 2010, 2024, Oracle and/or its affiliates.
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

INCLUDE(CheckSymbolExists)
INCLUDE(CheckCSourceRuns)
INCLUDE(CheckCSourceCompiles) 
INCLUDE(CheckCXXSourceCompiles)

IF(CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
  SET(SOLARIS_SPARC 1)
ELSE()
  MESSAGE(FATAL_ERROR "Solaris on Intel is not supported.")
ENDIF()

IF("${CMAKE_C_FLAGS}${CMAKE_CXX_FLAGS}" MATCHES "-m32")
  MESSAGE(FATAL_ERROR "32bit build not supported on Solaris.")
ENDIF()

IF(NOT "${CMAKE_C_FLAGS}" MATCHES  "-m64")
  STRING_APPEND(CMAKE_C_FLAGS      " -m64")
ENDIF()
IF(NOT "{CMAKE_CXX_FLAGS}" MATCHES "-m64")
  STRING_APPEND(CMAKE_CXX_FLAGS    " -m64")
ENDIF()

STRING_APPEND(CMAKE_C_LINK_FLAGS   " -m64")
STRING_APPEND(CMAKE_CXX_LINK_FLAGS " -m64")

IF(NOT FORCE_UNSUPPORTED_COMPILER)
  IF(MY_COMPILER_IS_CLANG)
    MESSAGE(WARNING "Clang is experimental!!")
  ELSEIF(MY_COMPILER_IS_GNU)
    # 9.2.0 generated code which dumped core in optimized mode.
    IF(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.2)
      MESSAGE(FATAL_ERROR "GCC 10.2 or newer is required")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "Unsupported compiler!")
  ENDIF()
ENDIF()

# Enable 64 bit file offsets
ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

# Enable general POSIX extensions. See standards(5) man page.
ADD_DEFINITIONS(-D__EXTENSIONS__)

# Solaris threads with POSIX semantics:
# http://docs.oracle.com/cd/E19455-01/806-5257/6je9h033k/index.html
ADD_DEFINITIONS(-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT -D_PTHREADS)

# CMake defined -lthread as thread flag. This crashes in dlopen 
# when trying to load plugins workaround with -lpthread
SET(CMAKE_THREAD_LIBS_INIT -lpthread CACHE INTERNAL "" FORCE)

# Solaris specific large page support
CHECK_SYMBOL_EXISTS(MHA_MAPSIZE_VA sys/mman.h  HAVE_SOLARIS_LARGE_PAGES)

SET(LINK_FLAG_NO_UNDEFINED "-Wl,--no-undefined")
SET(LINK_FLAG_Z_DEFS "-z,defs")
