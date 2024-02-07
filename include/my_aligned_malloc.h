/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef __MY_ALIGNED_MALLOC_H__
#define __MY_ALIGNED_MALLOC_H__

#include <cstddef>

/**
 Function allocates size bytes and returns a pointer to the allocated memory.
 Size and alignment parameters depend on platform on which the function is
 executed. Please check posix_memalign, memalign and _aligned_malloc functions
 for details. To conform with all platforms size should be multiple of aligment
 and aligment should be power of two.

 We can use C++17 aligned new/aligned delete on non-windows platforms once the
 minimum supported version of tcmalloc becomes >= 2.6.2. Right now TC malloc
 crashes.

 @param[in] size Multiple of alignment.
 @param[in] alignment Memory aligment, which must be power of two.

 @return Pointer to allocated memory.

 @see my_aligned_free
*/
void *my_aligned_malloc(size_t size, size_t alignment);

/**
 Free allocated memory using my_aligned_malloc function.

 @param[in] ptr Pointer to allocated memory using my_aligned_malloc function.
*/
void my_aligned_free(void *ptr);

#endif /* __MY_ALIGNED_MALLOC_H__ */
