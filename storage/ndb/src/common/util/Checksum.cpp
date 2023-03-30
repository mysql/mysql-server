/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#if defined(__clang__)

#include <ndb_types.h>
#include <stddef.h>

#include <Checksum.hpp>


/**
 * computeXorChecksumAligned16: Compute checksum on 16-byte aligned memory.
 * Only needed when compiling with Clang:
 *
 * Knowing that the address is aligned, the compiler can generate code using
 * the more efficient quad-word-aligned SSE-instructions. It seems
 * like Clang doesn't take advantage of the assume_aligned hint, unless the
 * code is in a non-inlined function.
 *
 * We only use this aligned variant of XorChecksum for large memory blocks.
 */
Uint32 computeXorChecksumAligned16(const Uint32 *buf, const size_t words,
                                   const Uint32 sum)
{
  buf = static_cast<const Uint32*>(__builtin_assume_aligned(buf, 16));
  return xorChecksum(buf, words, sum);
}

#endif
