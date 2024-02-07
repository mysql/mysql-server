/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

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

#ifndef MD5_HASH_H
#define MD5_HASH_H

#include <ndb_types.h>

/**
 * Calculate a md5-hash of keyBuf into result. If 'no_of_bytes' in keybuf
 * is not an 32 bit aligned word, the hash is calculated as if keybuf
 * were zero-padded to the upper word aligned length.
 * Note that there is no alignment requirement on the keybuf itself.
 */
void md5_hash(Uint32 result[4], const char *keybuf, Uint32 no_of_bytes);

// Convenient variants as keys are often stored in an Uint32 array
inline void md5_hash(Uint32 result[4], const Uint32 *keybuf,
                     Uint32 no_of_words) {
  md5_hash(result, reinterpret_cast<const char *>(keybuf), no_of_words * 4);
}

inline Uint32 md5_hash(const Uint32 *keybuf, Uint32 no_of_words) {
  Uint32 result[4];
  md5_hash(result, reinterpret_cast<const char *>(keybuf), no_of_words * 4);
  return result[0];
}

#endif
