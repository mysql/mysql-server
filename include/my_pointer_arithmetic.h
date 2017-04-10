/*
   Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MY_POINTER_ARITHMETIC_INCLUDED
#define MY_POINTER_ARITHMETIC_INCLUDED

/**
  @file include/my_pointer_arithmetic.h
  Some macros for dealing with pointer arithmetic, e.g., aligning
  of buffers to a given size.
*/

#include "my_inttypes.h"

#define MY_ALIGN(A,L)   (((A) + (L) - 1) & ~((L) - 1))
#define ALIGN_SIZE(A)   MY_ALIGN((A),sizeof(double))
/* Size to make adressable obj. */
#define ADD_TO_PTR(ptr,size,type) (type) ((uchar*) (ptr)+size)
#define PTR_BYTE_DIFF(A,B) (my_ptrdiff_t) ((uchar*) (A) - (uchar*) (B))

#endif  // MY_POINTER_ARITHMETIC_INCLUDED
