# Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
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

# We require SunStudio 12u2 (CC 5.11)
IF(NOT FORCE_UNSUPPORTED_COMPILER)
  IF(CMAKE_C_COMPILER_ID MATCHES "SunPro")
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
    IF(${CC_MINOR_VERSION} LESS 11)
      MESSAGE(FATAL_ERROR "SunStudio 12u2 or newer is required!")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "Unsupported compiler!")
  ENDIF()
ENDIF()

# Enable 64 bit file offsets
SET(_FILE_OFFSET_BITS 64)
ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

# Legacy option, without it  my_pthread is having problems
ADD_DEFINITIONS(-DHAVE_RWLOCK_T)

# On  Solaris, use of intrinsics will screw the lib search logic
# Force using -lm, so rint etc are found.
SET(LIBM m)

# CMake defined -lthread as thread flag. This crashes in dlopen 
# when trying to load plugins workaround with -lpthread
SET(CMAKE_THREAD_LIBS_INIT -lpthread CACHE INTERNAL "" FORCE)

# Solaris specific large page support
CHECK_SYMBOL_EXISTS(MHA_MAPSIZE_VA sys/mman.h  HAVE_DECL_MHA_MAPSIZE_VA)
IF(HAVE_DECL_MHA_MAPSIZE_VA)
 SET(HAVE_SOLARIS_LARGE_PAGES 1)
 SET(HAVE_LARGE_PAGE_OPTION 1)
ENDIF()


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
IF(CMAKE_SIZEOF_VOID_P MATCHES 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
  SET(MYSQL_MACHINE_TYPE "x86_64")
ENDIF()
