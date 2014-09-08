/* Copyright (c) 2008, 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_DIGEST_H
#define SQL_DIGEST_H

#define MAX_DIGEST_STORAGE_SIZE 1024

#include <string.h>

/**
  Structure to store token count/array for a statement
  on which digest is to be calculated.
*/
struct sql_digest_storage
{
  bool m_full;
  int m_byte_count;
  /** Character set number. */
  uint m_charset_number;
  /**
    Token array.
    Token array is an array of bytes to store tokens received during parsing.
    Following is the way token array is formed.
    ... &lt;non-id-token&gt; &lt;non-id-token&gt; &lt;id-token&gt; &lt;id_len&gt; &lt;id_text&gt; ...
    For Example:
    SELECT * FROM T1;
    &lt;SELECT_TOKEN&gt; &lt;*&gt; &lt;FROM_TOKEN&gt; &lt;ID_TOKEN&gt; &lt;2&gt; &lt;T1&gt;
  */
  unsigned char m_token_array[MAX_DIGEST_STORAGE_SIZE];

  inline void reset()
  {
    m_full= false;
    m_byte_count= 0;
    m_charset_number= 0;
  }

  inline bool is_empty()
  {
    return (m_byte_count == 0);
  }

  inline void copy(const sql_digest_storage *from)
  {
    /*
      Keep in mind this is a dirty copy of something that may change,
      as the thread producing the digest is executing concurrently,
      without any lock enforced.
    */
    int byte_count_copy= from->m_byte_count;

    if ((byte_count_copy > 0) && (byte_count_copy <= MAX_DIGEST_STORAGE_SIZE))
    {
      m_full= from->m_full;
      m_byte_count= byte_count_copy;
      m_charset_number= from->m_charset_number;
      memcpy(m_token_array, from->m_token_array, m_byte_count);
    }
    else
    {
      m_full= false;
      m_byte_count= 0;
      m_charset_number= 0;
    }
  }
};
typedef struct sql_digest_storage sql_digest_storage;

/**
  Compute a digest hash.
  @param digest_storage The digest
  @param [out] md5 The computed digest hash. This parameter is a buffer of size @c MD5_HASH_SIZE.
*/
void compute_digest_md5(const sql_digest_storage *digest_storage, unsigned char *md5);

/**
  Compute a digest text.
  A 'digest text' is a textual representation of a query,
  where:
  - comments are removed,
  - non significant spaces are removed,
  - literal values are replaced with a special '?' marker,
  - lists of values are collapsed using a shorter notation
  @param digest_storage The digest
  @param [out] digest_text
  @param digest_text_length Size of @c digest_text.
  @param [out] truncated true if the text representation was truncated
*/
void compute_digest_text(const sql_digest_storage *digest_storage,
                         char *digest_text, size_t digest_text_length,
                         bool *truncated);

#endif

