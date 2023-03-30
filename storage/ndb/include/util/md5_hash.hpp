/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.
    Use is subject to license terms.

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
