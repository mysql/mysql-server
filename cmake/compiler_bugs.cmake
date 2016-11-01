# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(CheckCSourceRuns)
INCLUDE(CheckCXXSourceRuns)

SET(SAVE_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
SET(CMAKE_REQUIRED_FLAGS
  "${CMAKE_REQUIRED_FLAGS} -O3 -fexpensive-optimizations"
)

# Below code is known to fail for GCC 5.3.0 on sparc solaris 11.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67781
SET(code "
  struct X
  {
    int i;
    unsigned short s;
  };

  unsigned __attribute__((noinline)) f(struct X x)
  {
    return x.s | (x.i << 16);
  }

  int
  main()
  {
    struct X x;
    x.i = 0x00001234;
    x.s = 0x5678;
    unsigned y = f(x);
    /* Succeed (return 0) if compiler have bug */
    return y == 0x12345678 ? 1 : 0;
  }
")

IF(CMAKE_COMPILER_IS_GNUCC)
  CHECK_C_SOURCE_RUNS("${code}" HAVE_C_SHIFT_OR_OPTIMIZATION_BUG)
ENDIF()

IF(CMAKE_COMPILER_IS_GNUCXX)
  CHECK_CXX_SOURCE_RUNS("${code}" HAVE_CXX_SHIFT_OR_OPTIMIZATION_BUG)
ENDIF()

SET(CMAKE_REQUIRED_FLAGS "${SAVE_CMAKE_REQUIRED_FLAGS}")
