/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_DIGEST_H
#define SQL_DIGEST_H

#include <string.h>
#include <sys/types.h>

#include "my_inttypes.h"  // IWYU pragma: keep

class String;

#define MAX_DIGEST_STORAGE_SIZE (1024 * 1024)

/**
  Write SHA-256 hash value in a string to be used
  as DIGEST for the statement.
*/
#define DIGEST_HASH_TO_STRING(_hash, _str)                                  \
  sprintf(_str,                                                             \
          "%02x%02x%02x%02x%02x%02x%02x%02x"                                \
          "%02x%02x%02x%02x%02x%02x%02x%02x"                                \
          "%02x%02x%02x%02x%02x%02x%02x%02x"                                \
          "%02x%02x%02x%02x%02x%02x%02x%02x",                               \
          _hash[0], _hash[1], _hash[2], _hash[3], _hash[4], _hash[5],       \
          _hash[6], _hash[7], _hash[8], _hash[9], _hash[10], _hash[11],     \
          _hash[12], _hash[13], _hash[14], _hash[15], _hash[16], _hash[17], \
          _hash[18], _hash[19], _hash[20], _hash[21], _hash[22], _hash[23], \
          _hash[24], _hash[25], _hash[26], _hash[27], _hash[28], _hash[29], \
          _hash[30], _hash[31])

/// SHA-256 = 32 bytes of binary = 64 printable characters.
#define DIGEST_HASH_TO_STRING_LENGTH 64

/*
  Various hashes considered for digests.

  MD5:
  - 128 bits
  - used up to MySQL 5.7
  - abandoned in MySQL 8.0, non FIPS compliant.

  SHA1:
  - 160 bits
  - non FIPS compliant in strict mode
  - not used

  SHA2-224
  - 224 bits
  - non FIPS compliant in strict mode
  - not used

  SHA2-256
  - 256 bits
  - FIPS compliant
  - Used starting with MySQL 8.0

  SHA2-384
  - 384 bits

  SHA2-512
  - 512 bits
*/

/**
  DIGEST hash size, in bytes.
  256 bits, for SHA256.
*/
#define DIGEST_HASH_SIZE 32

ulong get_max_digest_length();

/**
  Structure to store token count/array for a statement
  on which digest is to be calculated.
*/
struct sql_digest_storage {
  bool m_full;
  size_t m_byte_count;
  unsigned char m_hash[DIGEST_HASH_SIZE];
  /** Character set number. */
  uint m_charset_number;
  /**
    Token array.
    Token array is an array of bytes to store tokens received during parsing.
    Following is the way token array is formed.
    ... &lt;non-id-token&gt; &lt;non-id-token&gt; &lt;id-token&gt;
    &lt;id_len&gt; &lt;id_text&gt; ... For Example: SELECT * FROM T1;
    &lt;SELECT_TOKEN&gt; &lt;*&gt; &lt;FROM_TOKEN&gt; &lt;ID_TOKEN&gt; &lt;2&gt;
    &lt;T1&gt;

    @note Only the first @c m_byte_count bytes are initialized,
      out of @c m_token_array_length.
  */
  unsigned char *m_token_array;
  /* Length of the token array to be considered for DIGEST_TEXT calculation. */
  size_t m_token_array_length;

  sql_digest_storage() { reset(nullptr, 0); }

  inline void reset(unsigned char *token_array, size_t length) {
    m_token_array = token_array;
    m_token_array_length = length;
    reset();
  }

  inline void reset() {
    m_full = false;
    m_byte_count = 0;
    m_charset_number = 0;
    memset(m_hash, 0, DIGEST_HASH_SIZE);
  }

  inline bool is_empty() { return (m_byte_count == 0); }

  inline void copy(const sql_digest_storage *from) {
    /*
      Keep in mind this is a dirty copy of something that may change,
      as the thread producing the digest is executing concurrently,
      without any lock enforced.
    */
    size_t byte_count_copy = m_token_array_length < from->m_byte_count
                                 ? m_token_array_length
                                 : from->m_byte_count;

    if (byte_count_copy > 0) {
      m_full = from->m_full;
      m_byte_count = byte_count_copy;
      m_charset_number = from->m_charset_number;
      memcpy(m_token_array, from->m_token_array, m_byte_count);
      memcpy(m_hash, from->m_hash, DIGEST_HASH_SIZE);
    } else {
      m_full = false;
      m_byte_count = 0;
      m_charset_number = 0;
    }
  }
};
typedef struct sql_digest_storage sql_digest_storage;

/**
  Compute a digest hash.
  @param digest_storage The digest
  @param [out] hash The computed digest hash. This parameter is a buffer of size
  @c DIGEST_HASH_SIZE.
*/
void compute_digest_hash(const sql_digest_storage *digest_storage,
                         unsigned char *hash);

/**
  Compute a digest text.
  A 'digest text' is a textual representation of a query,
  where:
  - comments are removed,
  - non significant spaces are removed,
  - literal values are replaced with a special '?' marker,
  - lists of values are collapsed using a shorter notation
  @param digest_storage The digest
  @param [out] digest_text The digest text
*/
void compute_digest_text(const sql_digest_storage *digest_storage,
                         String *digest_text);

#endif
