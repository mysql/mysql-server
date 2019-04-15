# Copyright (c) 2010, 2019, Oracle and/or its affiliates. All rights reserved.
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

SET(SOLARIS 1)
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
  SET(SOLARIS_SPARC 1)
ENDIF()

INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE("void *" SIZEOF_VOIDP)

# We require at least GCC 7.3 or SunStudio 12.2 (CC 5.14)
IF(NOT FORCE_UNSUPPORTED_COMPILER)
  IF(CMAKE_COMPILER_IS_GNUCC)
    EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                    OUTPUT_VARIABLE GCC_VERSION)
    IF(GCC_VERSION VERSION_LESS 7.3)
      MESSAGE(FATAL_ERROR "GCC 7.3 or newer is required!")
    ENDIF()
  ELSEIF(CMAKE_C_COMPILER_ID MATCHES "SunPro")
    # CC -V yields
    # CC: Studio 12.6 Sun C++ 5.15 SunOS_sparc Beta 2016/12/19
    # CC: Studio 12.5 Sun C++ 5.14 SunOS_sparc Dodona 2016/04/04
    # CC: Sun C++ 5.13 SunOS_sparc Beta 2014/03/11
    # CC: Sun C++ 5.11 SunOS_sparc 2010/08/13
    EXECUTE_PROCESS(
      COMMAND ${CMAKE_CXX_COMPILER} "-V"
      OUTPUT_VARIABLE stdout
      ERROR_VARIABLE  stderr
      RESULT_VARIABLE result
    )
    STRING(REGEX MATCH "CC: Sun C\\+\\+ 5\\.([0-9]+)" VERSION_STRING ${stderr})
    IF (NOT CMAKE_MATCH_1 OR CMAKE_MATCH_1 STREQUAL "")
      STRING(REGEX MATCH "CC: Studio 12\\.[56] Sun C\\+\\+ 5\\.([0-9]+)"
        VERSION_STRING ${stderr})
    ENDIF()
    SET(CC_MINOR_VERSION ${CMAKE_MATCH_1})
    IF(${CC_MINOR_VERSION} LESS 14)
      MESSAGE(FATAL_ERROR "Oracle Studio 12.5 or newer is required!")
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
    SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -m${ISAINFO_B}")
    SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -m${ISAINFO_B}")
  ENDIF()
ELSE()
  IF("${CMAKE_C_FLAGS}" MATCHES "-m32")
    SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -m32")
    SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -m32")
  ELSE()
    SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -m64")
    SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -m64")
  ENDIF()
ENDIF()

# On  Solaris, use of intrinsics will screw the lib search logic
# Force using -lm, so rint etc are found.
SET(LIBM m)

# CMake defined -lthread as thread flag. This crashes in dlopen 
# when trying to load plugins workaround with -lpthread
SET(CMAKE_THREAD_LIBS_INIT -lpthread CACHE INTERNAL "" FORCE)

# Solaris specific large page support
CHECK_SYMBOL_EXISTS(MHA_MAPSIZE_VA sys/mman.h  HAVE_SOLARIS_LARGE_PAGES)

# Solaris atomics
CHECK_C_SOURCE_RUNS(
 "
 #include  <atomic.h>
  int main()
  {
    int foo = -10; int bar = 10;
    int64_t foo64 = -10; int64_t bar64 = 10;
    if (atomic_add_int_nv((uint_t *)&foo, bar) || foo)
      return -1;
    bar = atomic_swap_uint((uint_t *)&foo, (uint_t)bar);
    if (bar || foo != 10)
     return -1;
    bar = atomic_cas_uint((uint_t *)&bar, (uint_t)foo, 15);
    if (bar)
      return -1;
    if (atomic_add_64_nv((volatile uint64_t *)&foo64, bar64) || foo64)
      return -1;
    bar64 = atomic_swap_64((volatile uint64_t *)&foo64, (uint64_t)bar64);
    if (bar64 || foo64 != 10)
      return -1;
    bar64 = atomic_cas_64((volatile uint64_t *)&bar64, (uint_t)foo64, 15);
    if (bar64)
      return -1;
    atomic_or_64((volatile uint64_t *)&bar64, 0);
    return 0;
  }
"  HAVE_SOLARIS_ATOMIC)

CHECK_CXX_SOURCE_COMPILES("
    #undef inline
    #if !defined(_REENTRANT)
    #define _REENTRANT
    #endif
    #include <pthread.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    int main()
    {

       struct hostent *foo =
       gethostbyaddr_r((const char *) 0,
          0, 0, (struct hostent *) 0, (char *) NULL,  0, (int *)0);
       return 0;
    }
  "
  HAVE_SOLARIS_STYLE_GETHOST)

# Check is special processor flag needs to be set on older GCC
#that defaults to v8 sparc . Code here is taken from my_rdtsc.c 
IF(CMAKE_COMPILER_IS_GNUCC AND CMAKE_SIZEOF_VOID_P EQUAL 4
  AND CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
  SET(SOURCE
  "
  int main()
  {
     long high\;
     long low\;
    __asm __volatile__ (\"rd %%tick,%1\; srlx %1,32,%0\" : \"=r\" ( high), \"=r\" (low))\;
    return 0\;
  } ")
  CHECK_C_SOURCE_COMPILES(${SOURCE}  HAVE_SPARC32_TICK)
  IF(NOT HAVE_SPARC32_TICK)
    SET(CMAKE_REQUIRED_FLAGS "-mcpu=v9")
    CHECK_C_SOURCE_COMPILES(${SOURCE}  HAVE_SPARC32_TICK_WITH_V9)
    SET(CMAKE_REQUIRED_FLAGS)
    IF(HAVE_SPARC32_TICK_WITH_V9)
      SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=v9")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=v9")
    ENDIF()
  ENDIF()
ENDIF()

# This is used for the version_compile_machine variable.
IF(SIZEOF_VOIDP MATCHES 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
  SET(MYSQL_MACHINE_TYPE "x86_64")
ENDIF()


MACRO(DIRNAME IN OUT)
  GET_FILENAME_COMPONENT(${OUT} ${IN} PATH)
ENDMACRO()

# We assume that developer studio runtime libraries are installed.
IF(CMAKE_SYSTEM_NAME MATCHES "SunOS" AND
   CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
  DIRNAME(${CMAKE_CXX_COMPILER} CXX_PATH)

  SET(LIBRARY_SUFFIX "lib/compilers/CC-gcc/lib")
  IF(SIZEOF_VOIDP EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
    SET(LIBRARY_SUFFIX "${LIBRARY_SUFFIX}/sparcv9")
  ENDIF()
  IF(SIZEOF_VOIDP EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
    SET(LIBRARY_SUFFIX "${LIBRARY_SUFFIX}/amd64")
  ENDIF()
  FIND_LIBRARY(STL_LIBRARY_NAME
    NAMES "stdc++"
    PATHS ${CXX_PATH}/../${LIBRARY_SUFFIX}
    NO_DEFAULT_PATH
  )
  MESSAGE(STATUS "STL_LIBRARY_NAME ${STL_LIBRARY_NAME}")
  IF(STL_LIBRARY_NAME)
    DIRNAME(${STL_LIBRARY_NAME} STL_LIBRARY_PATH)
    SET(LRFLAGS " -L${STL_LIBRARY_PATH} -R${STL_LIBRARY_PATH}")
    SET(QUOTED_CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")

    STRING_APPEND(CMAKE_C_LINK_FLAGS          "${LRFLAGS}")
    STRING_APPEND(CMAKE_CXX_LINK_FLAGS        "${LRFLAGS}")
    STRING_APPEND(CMAKE_MODULE_LINKER_FLAGS   "${LRFLAGS}")
    STRING_APPEND(CMAKE_SHARED_LINKER_FLAGS   "${LRFLAGS}")
    STRING_APPEND(QUOTED_CMAKE_CXX_LINK_FLAGS "${LRFLAGS}")
  ENDIF()

  STRING_APPEND(CMAKE_C_LINK_FLAGS          " -lc")
  STRING_APPEND(CMAKE_CXX_LINK_FLAGS        " -lstdc++ -lgcc_s -lCrunG3 -lc")
  STRING_APPEND(CMAKE_MODULE_LINKER_FLAGS   " -lstdc++ -lgcc_s -lCrunG3 -lc")
  STRING_APPEND(CMAKE_SHARED_LINKER_FLAGS   " -lstdc++ -lgcc_s -lCrunG3 -lc")
  STRING_APPEND(QUOTED_CMAKE_CXX_LINK_FLAGS " -lstdc++ -lgcc_s -lCrunG3 -lc ")
ENDIF()
