#ifndef DIGEST_INCLUDED
#define DIGEST_INCLUDED
/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file digest.h
  Constants and functionality that facilitate working with digests.
*/

/**
  Write SHA-256 hash value in a string to be used
  as DIGEST for the statement.
*/
#define DIGEST_HASH_TO_STRING(_hash, _str)    \
  sprintf(_str,                               \
          "%02x%02x%02x%02x%02x%02x%02x%02x"  \
          "%02x%02x%02x%02x%02x%02x%02x%02x"  \
          "%02x%02x%02x%02x%02x%02x%02x%02x"  \
          "%02x%02x%02x%02x%02x%02x%02x%02x", \
          _hash[0],                           \
          _hash[1],                           \
          _hash[2],                           \
          _hash[3],                           \
          _hash[4],                           \
          _hash[5],                           \
          _hash[6],                           \
          _hash[7],                           \
          _hash[8],                           \
          _hash[9],                           \
          _hash[10],                          \
          _hash[11],                          \
          _hash[12],                          \
          _hash[13],                          \
          _hash[14],                          \
          _hash[15],                          \
          _hash[16],                          \
          _hash[17],                          \
          _hash[18],                          \
          _hash[19],                          \
          _hash[20],                          \
          _hash[21],                          \
          _hash[22],                          \
          _hash[23],                          \
          _hash[24],                          \
          _hash[25],                          \
          _hash[26],                          \
          _hash[27],                          \
          _hash[28],                          \
          _hash[29],                          \
          _hash[30],                          \
          _hash[31])

/// SHA-256 = 32 bytes of binary = 64 printable characters.
#define DIGEST_HASH_TO_STRING_LENGTH 64


#endif // DIGEST_INCLUDED
