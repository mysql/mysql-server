/*
   Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_BITMAP_H
#define NDB_BITMAP_H

#include <string>

#include "my_bitmap.h"


/**
  Initialize bitmap using provided buffer.
  @param bitmap     The MY_BITMAP to initialize
  @param buf        Buffer to hold the bits for the bitmap
  @param num_bits   Max number of bits to store in the bitmap

  NOTE! Since no memory need to be allocated the 'bitmap_init' funtion
  never fails.

  NOTE! Size of provided buffer automatically defferred by
  usage of template and thus it's possible to check that
  bitmap is not initialized larger than what the buffer can hold.

*/

template<size_t sz>
static inline
void ndb_bitmap_init(MY_BITMAP& bitmap,
                     my_bitmap_map (&buf)[sz],
                     uint num_bits)
{
  assert(num_bits > 0);
  assert(bitmap_buffer_size(num_bits) <= (sz * sizeof(my_bitmap_map)));

  // Function never fails when called with a "buf" provided
  (void)bitmap_init(&bitmap, buf, num_bits, false);
}

/**
 * @brief Return bitmap as hex formatted string
 * @param bitmap The bitmap to format
 * @return string representation of the bitmap
 */
std::string ndb_bitmap_to_hex_string(const MY_BITMAP* bitmap);

#endif
