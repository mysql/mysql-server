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

# HAVE_GCC_ATOMIC_BUILTINS is already checked by CMake somewhere else

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

IF(HAVE_GCC_ATOMIC_BUILTINS)
  MESSAGE(STATUS "Using gcc atomic builtins") 
ELSEIF(HAVE_DARWIN_ATOMICS) 
  MESSAGE(STATUS "Using Darwin OSAtomic") 
ELSEIF(HAVE_SOLARIS_ATOMICS)
  MESSAGE(STATUS "Using Solaris <atomic.h>")
ELSE()
  MESSAGE(FATAL_ERROR "No atomic functions available")
ENDIF()

