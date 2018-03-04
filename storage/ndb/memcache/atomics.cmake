# Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.
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

# Identify support for atomic operations
IF(NOT MSVC)

# HAVE_GCC_ATOMIC_BUILTINS is already checked by CMake, but if it's not defined
# we're going to retest using "-march=pentium"


  if(CMAKE_COMPILER_IS_GNUCC AND NOT (HAVE_GCC_ATOMIC_BUILTINS OR HAVE_GCC_SYNC_BUILTINS))
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

IF(HAVE_GCC_ATOMIC_BUILTINS OR HAVE_GCC_SYNC_BUILTINS OR HAVE_GCC_ATOMICS_WITH_ARCH_FLAG)
  MESSAGE(STATUS "Using gcc atomic builtins") 
ELSEIF(HAVE_DARWIN_ATOMICS) 
  MESSAGE(STATUS "Using Darwin OSAtomic") 
ELSEIF(HAVE_SOLARIS_ATOMICS)
  MESSAGE(STATUS "Using Solaris <atomic.h>")
ELSE()
  MESSAGE(STATUS "Skipping NDB/Memcache. No atomic functions available.")
  SET(NO_ATOMICS 1)
ENDIF()

