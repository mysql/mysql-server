# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

# Identify support for atomic operations
IF(NOT MSVC)

# HAVE_GCC_ATOMIC_BUILTINS is already checked by CMake, but if it's not defined
# we're going to retest using "-march=pentium"


  if(CMAKE_COMPILER_IS_GNUCC AND NOT HAVE_GCC_ATOMIC_BUILTINS)
     set(OLD_FLAGS ${CMAKE_REQUIRED_FLAGS})
     set(CMAKE_REQUIRED_FLAGS "${OLD_FLAGS} -march=pentium")
     CHECK_C_SOURCE_RUNS(
         "int main() {
          volatile int foo= -10; 
          volatile int bar= 10;
          /* operation returns 0 and foo should be 0 */
          if (!__sync_fetch_and_add(&foo, bar) || foo)
            return -1;
          bar= __sync_lock_test_and_set(&foo, bar);
          /* Now bar is the return value 0 and foo is set to 10 */
          if (bar || foo != 10)
            return -1;
          __sync_val_compare_and_swap(&bar, foo, 15);
          /* CAS should have failed and bar is still 0 */
          if (bar)
            return -1;
          return 0;
        }"
        HAVE_GCC_ATOMICS_WITH_ARCH_FLAG
      )
      set(CMAKE_REQUIRED_FLAGS ${OLD_FLAGS})
  endif()


  CHECK_C_SOURCE_RUNS(
    "#include <libkern/OSAtomic.h>
     int main() {
       volatile int foo = 5; 
       OSAtomicAdd32(6, &foo);
       return (foo == 11) ? 0 : -1;
     }"
    HAVE_DARWIN_ATOMICS
  )

  CHECK_C_SOURCE_RUNS(
    "#include <atomic.h>
     int main() {
       volatile unsigned int foo = 5; 
       atomic_add_32(&foo, 6);
       return (foo == 11) ? 0 : -1;
     }"
    HAVE_SOLARIS_ATOMICS
  )
ENDIF()

IF(HAVE_GCC_ATOMICS_WITH_ARCH_FLAG)
  add_definitions(-march=pentium)
ENDIF()

IF(HAVE_GCC_ATOMIC_BUILTINS OR HAVE_GCC_ATOMICS_WITH_ARCH_FLAG)
  MESSAGE(STATUS "Using gcc atomic builtins") 
ELSEIF(HAVE_DARWIN_ATOMICS) 
  MESSAGE(STATUS "Using Darwin OSAtomic") 
ELSEIF(HAVE_SOLARIS_ATOMICS)
  MESSAGE(STATUS "Using Solaris <atomic.h>")
ELSE()
  MESSAGE(STATUS "Skipping NDB/Memcache. No atomic functions available.")
  SET(NO_ATOMICS 1)
ENDIF()

