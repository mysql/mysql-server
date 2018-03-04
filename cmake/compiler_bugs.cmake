# Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(CheckCSourceRuns)
INCLUDE(CheckCXXSourceRuns)

FUNCTION(CHECK_C_COMPILER_BUG NO_BUG_VAR BAD_FLAGS WORKAROUND_FLAGS CODE)
  SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${BAD_FLAGS}")
  CHECK_C_SOURCE_RUNS("${CODE}" ${NO_BUG_VAR})

  IF(NOT ${NO_BUG_VAR})
    SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${WORKAROUND_FLAGS}")
    CHECK_C_SOURCE_RUNS("${CODE}" ${NO_BUG_VAR}_WORKAROUND)
    IF(NOT ${NO_BUG_VAR}_WORKAROUND AND NOT FORCE_UNSUPPORTED_COMPILER)
      MESSAGE(FATAL_ERROR
        "${NO_BUG_VAR} failed and workaround '${WORKAROUND_FLAGS}' not working!")
    ENDIF()
    IF(NOT ${NO_BUG_VAR}_WORKAROUND)
      MESSAGE(WARNING
        "${NO_BUG_VAR} failed and workaround '${WORKAROUND_FLAGS}' not working!")
    ELSE()
      MESSAGE(STATUS "Workaround for ${NO_BUG_VAR} added: ${WORKAROUND_FLAGS}")
      SET(COMMON_C_WORKAROUND_FLAGS
        "${COMMON_C_WORKAROUND_FLAGS} ${WORKAROUND_FLAGS}" PARENT_SCOPE)
    ENDIF()
  ENDIF()
ENDFUNCTION()

FUNCTION(CHECK_CXX_COMPILER_BUG NO_BUG_VAR BAD_FLAGS WORKAROUND_FLAGS CODE)
  SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${BAD_FLAGS}")
  CHECK_CXX_SOURCE_RUNS("${CODE}" ${NO_BUG_VAR})

  IF(NOT ${NO_BUG_VAR})
    SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${WORKAROUND_FLAGS}")
    CHECK_CXX_SOURCE_RUNS("${CODE}" ${NO_BUG_VAR}_WORKAROUND)
    IF(NOT ${NO_BUG_VAR}_WORKAROUND AND NOT FORCE_UNSUPPORTED_COMPILER)
      MESSAGE(FATAL_ERROR
        "${NO_BUG_VAR} failed and workaround '${WORKAROUND_FLAGS}' not working!")
    ENDIF()
    IF(NOT ${NO_BUG_VAR}_WORKAROUND)
      MESSAGE(WARNING
        "${NO_BUG_VAR} failed and workaround '${WORKAROUND_FLAGS}' not working!")
    ELSE()
      MESSAGE(STATUS "Workaround for ${NO_BUG_VAR} added: ${WORKAROUND_FLAGS}")
      SET(COMMON_CXX_WORKAROUND_FLAGS
        "${COMMON_CXX_WORKAROUND_FLAGS} ${WORKAROUND_FLAGS}" PARENT_SCOPE)
    ENDIF()
  ENDIF()
ENDFUNCTION()

# Below code is known to fail for GCC 5.3.0 on sparc solaris 11.
SET(code "
inline void g(unsigned size, unsigned x[], unsigned y[])
{
  unsigned i;
  for (i = 0; i < size; i++)
  {
    x[i] |= y[i];
  }
  for (i = 0; i < size; i++)
  {
    y[i] = 0;
  }
}

struct A
{
  long a; // Make struct A 8 byte aligned
  int b;  // Make x[] not 8 byte aligned
  unsigned x[6];
  unsigned y[6];
};

void f(struct A* a)
{
  g(6, a->x, a->y);
}

int
main()
{
  struct A a;
  f(&a);
  return 0;
}
")

IF(CMAKE_COMPILER_IS_GNUCC)
CHECK_C_COMPILER_BUG(HAVE_NOT_C_BUG_LOOP_VECTORIZE
  "-O3 -fPIC"
  "-fvect-cost-model=cheap -fno-tree-loop-distribute-patterns -fno-tree-loop-vectorize"
  "${code}")
ENDIF()

IF(CMAKE_COMPILER_IS_GNUCXX)
CHECK_CXX_COMPILER_BUG(HAVE_NOT_CXX_BUG_LOOP_VECTORIZE
  "-O3 -fPIC"
  "-fvect-cost-model=cheap -fno-tree-loop-distribute-patterns -fno-tree-loop-vectorize"
  "${code}")
ENDIF()

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
    return y == 0x12345678 ? 0 : 1;
  }
")

IF(CMAKE_COMPILER_IS_GNUCC)
  CHECK_C_COMPILER_BUG(HAVE_NOT_C_SHIFT_OR_OPTIMIZATION_BUG
                       "-O3 -fexpensive-optimizations"
                       "-fno-expensive-optimizations"
                       "${code}")
ENDIF()

IF(CMAKE_COMPILER_IS_GNUCXX)
  CHECK_CXX_COMPILER_BUG(HAVE_NOT_CXX_SHIFT_OR_OPTIMIZATION_BUG
                         "-O3 -fexpensive-optimizations"
                         "-fno-expensive-optimizations"
                         "${code}")
ENDIF()
