/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/** @file storage/temptable/include/temptable/misc.h
TempTable miscellaneous helper utilities declarations. */

#ifndef TEMPTABLE_MISC_H
#define TEMPTABLE_MISC_H

#include <cstddef> /* size_t */

namespace temptable {

/** Check if a given buffer is inside another buffer.
@return true if inside */
inline bool buf_is_inside_another(
    /** [in] First buffer, that should be inside the other. */
    const unsigned char* small,
    /** [in] First buffer length in bytes. */
    size_t small_length,
    /** [in] Second buffer, that should contain the other. */
    const unsigned char* big,
    /** [in] Second buffer length in bytes. */
    size_t big_length) {
  const unsigned char* small_after_last = small + small_length;
  const unsigned char* big_after_last = big + big_length;

  return small >= big && small_after_last <= big_after_last;
}

} /* namespace temptable */

#endif /* TEMPTABLE_MISC_H */
