#ifndef MY_POINTER_ARITHMETIC_INCLUDED
#define MY_POINTER_ARITHMETIC_INCLUDED
/*
   Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <stdint.h>

/**
  @file include/my_pointer_arithmetic.h
  Some macros for dealing with pointer arithmetic, e.g., aligning
  of buffers to a given size.
*/

#define MY_ALIGN(A, L) (((A) + (L)-1) & ~((L)-1))
#define ALIGN_SIZE(A) MY_ALIGN((A), sizeof(double))

#ifdef __cplusplus

template<typename T>
bool is_aligned_to(T *t, int increment) {
  return reinterpret_cast<uintptr_t>(t) % increment == 0;
}

template<typename T>
bool is_aligned(T *t) {
  return is_aligned_to(t, alignof(T));
}

#endif // __cplusplus

#endif  // MY_POINTER_ARITHMETIC_INCLUDED
