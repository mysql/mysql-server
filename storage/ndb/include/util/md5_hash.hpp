/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef MD5_HASH_H
#define MD5_HASH_H

#include <ndb_types.h>

// External declaration of hash function 
void md5_hash(Uint32 result[4], const Uint64* keybuf, Uint32 no_of_32_words);

inline
Uint32
md5_hash(const Uint64* keybuf, Uint32 no_of_32_words)
{
  Uint32 result[4];
  md5_hash(result, keybuf, no_of_32_words);
  return result[0];
}

#endif
