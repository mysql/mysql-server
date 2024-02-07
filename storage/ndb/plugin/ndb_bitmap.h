/*
   Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_BITMAP_H
#define NDB_BITMAP_H

#include <string>

#include "my_bitmap.h"

/**
  Bitmap buffer providing space for the given number of bits.

  NOTE! To be used when the max number of bits is known at compile time and is
  reasonably small to justify avoiding the need to dynamically allocate memory
  for the bitmap.
 */
template <size_t bits>
class Ndb_bitmap_buf {
  static_assert(bits > 0, "Number of bits must be greater than zero");

  // Assume that my_bitmap_map is a 32 bit variable (since that's what the
  // my_bitmap implementation assumes)
  static_assert(sizeof(my_bitmap_map) == 4, "Unxpected my_bitmap_map type");

  // Buffer space for number of bits, rounded up. Uninitialized.
  my_bitmap_map m_buf[(bits + 31) / 32];

 public:
  Ndb_bitmap_buf() {}

  static constexpr size_t size_in_bytes() { return sizeof(m_buf); }

  my_bitmap_map *buf() { return m_buf; }
};

/**
  Initialize bitmap using provided buffer.
  @param bitmap     The MY_BITMAP to initialize
  @param buf        Buffer to hold the bits for the bitmap
  @param num_bits   Max number of bits to store in the bitmap

  NOTE! Since no memory need to be allocated the 'bitmap_init' function
  never fails.

  NOTE! Size of provided buffer automatically deferred by
  usage of template and thus it's possible to check that
  bitmap is not initialized larger than what the buffer can hold.

*/

template <size_t sz>
static inline void ndb_bitmap_init(MY_BITMAP *bitmap, Ndb_bitmap_buf<sz> &buf,
                                   uint num_bits) {
  assert(num_bits > 0);
  assert(bitmap_buffer_size(num_bits) <= buf.size_in_bytes());

  // Function never fails when called with a "buf" provided
  (void)bitmap_init(bitmap, buf.buf(), num_bits);
}

/**
 * @brief Return bitmap as hex formatted string
 * @param bitmap The bitmap to format
 * @return string representation of the bitmap
 */
std::string ndb_bitmap_to_hex_string(const MY_BITMAP *bitmap);

#endif
