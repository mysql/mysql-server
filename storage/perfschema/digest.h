#ifndef DIGEST_INCLUDED
#define DIGEST_INCLUDED
/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/digest.h
  Constants and functionality that facilitate working with digests.
*/

/**
  Write SHA-256 hash value in a string to be used
  as DIGEST for the statement.
*/
#define DIGEST_HASH_TO_STRING(_hash, _str)                                    \
  (void)sprintf(_str,                                                         \
                "%02x%02x%02x%02x%02x%02x%02x%02x"                            \
                "%02x%02x%02x%02x%02x%02x%02x%02x"                            \
                "%02x%02x%02x%02x%02x%02x%02x%02x"                            \
                "%02x%02x%02x%02x%02x%02x%02x%02x",                           \
                _hash[0], _hash[1], _hash[2], _hash[3], _hash[4], _hash[5],   \
                _hash[6], _hash[7], _hash[8], _hash[9], _hash[10], _hash[11], \
                _hash[12], _hash[13], _hash[14], _hash[15], _hash[16],        \
                _hash[17], _hash[18], _hash[19], _hash[20], _hash[21],        \
                _hash[22], _hash[23], _hash[24], _hash[25], _hash[26],        \
                _hash[27], _hash[28], _hash[29], _hash[30], _hash[31])

/// SHA-256 = 32 bytes of binary = 64 printable characters.
#define DIGEST_HASH_TO_STRING_LENGTH 64

#endif  // DIGEST_INCLUDED
