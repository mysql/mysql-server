# Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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

# Enable 64 bit file offsets
SET(_FILE_OFFSET_BITS 64)

# Legacy option, without it  my_pthread is having problems
ADD_DEFINITIONS(-DHAVE_RWLOCK_T)

# On  Solaris, use of intrinsics will screw the lib search logic
# Force using -lm, so rint etc are found.
SET(LIBM m)

# CMake defined -lthread as thread flag. This crashes in dlopen 
# when trying to load plugins workaround with -lpthread
SET(CMAKE_THREADS_LIBS_INIT -lpthread CACHE INTERNAL "" FORCE)

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
