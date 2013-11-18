/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_DIGEST_STREAM_H
#define SQL_DIGEST_STREAM_H

#include "sql_digest.h"

/**
  State data storage for @c digest_start, @c digest_add_token.
  This structure provide temporary storage to a digest locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
*/
struct sql_digest_state
{
  int m_last_id_index;
  sql_digest_storage m_digest_storage;

  inline void reset()
  {
    m_last_id_index= 0;
    m_digest_storage.reset();
  }
};
typedef struct sql_digest_state sql_digest_state;

#define PFS_SIZE_OF_A_TOKEN 2

/* Fixed, per MD5 hash. */
#define PFS_MD5_SIZE 16

/**
  Read a single token from token array.
*/
inline int read_token(const sql_digest_storage *digest_storage,
                      int index, uint *tok)
{
  int safe_byte_count= digest_storage->m_byte_count;

  if (index + PFS_SIZE_OF_A_TOKEN <= safe_byte_count &&
      safe_byte_count <= MAX_DIGEST_STORAGE_SIZE)
  {
    const unsigned char *src= & digest_storage->m_token_array[index];
    *tok= src[0] | (src[1] << 8);
    return index + PFS_SIZE_OF_A_TOKEN;
  }

  /* The input byte stream is exhausted. */
  *tok= 0;
  return MAX_DIGEST_STORAGE_SIZE + 1;
}

/**
  Store a single token in token array.
*/
inline void store_token(sql_digest_storage* digest_storage, uint token)
{
  DBUG_ASSERT(digest_storage->m_byte_count >= 0);
  DBUG_ASSERT(digest_storage->m_byte_count <= MAX_DIGEST_STORAGE_SIZE);

  if (digest_storage->m_byte_count + PFS_SIZE_OF_A_TOKEN <= MAX_DIGEST_STORAGE_SIZE)
  {
    unsigned char* dest= & digest_storage->m_token_array[digest_storage->m_byte_count];
    dest[0]= token & 0xff;
    dest[1]= (token >> 8) & 0xff;
    digest_storage->m_byte_count+= PFS_SIZE_OF_A_TOKEN;
  }
  else
  {
    digest_storage->m_full= true;
  }
}

/**
  Read an identifier from token array.
*/
inline int read_identifier(const sql_digest_storage* digest_storage,
                           int index, char ** id_string, int *id_length)
{
  int new_index;
  DBUG_ASSERT(index <= digest_storage->m_byte_count);
  DBUG_ASSERT(digest_storage->m_byte_count <= MAX_DIGEST_STORAGE_SIZE);

  /*
    token + length + string are written in an atomic way,
    so we do always expect a length + string here
  */
  const unsigned char *src= & digest_storage->m_token_array[index];
  uint length= src[0] | (src[1] << 8);
  *id_string= (char *) (src + 2);
  *id_length= length;

  new_index= index + PFS_SIZE_OF_A_TOKEN + length;
  DBUG_ASSERT(new_index <= digest_storage->m_byte_count);
  return new_index;
}

/**
  Store an identifier in token array.
*/
inline void store_token_identifier(sql_digest_storage* digest_storage,
                                   uint token,
                                   uint id_length, const char *id_name)
{
  DBUG_ASSERT(digest_storage->m_byte_count >= 0);
  DBUG_ASSERT(digest_storage->m_byte_count <= MAX_DIGEST_STORAGE_SIZE);

  uint bytes_needed= 2 * PFS_SIZE_OF_A_TOKEN + id_length;
  if (digest_storage->m_byte_count + bytes_needed <= MAX_DIGEST_STORAGE_SIZE)
  {
    unsigned char* dest= & digest_storage->m_token_array[digest_storage->m_byte_count];
    /* Write the token */
    dest[0]= token & 0xff;
    dest[1]= (token >> 8) & 0xff;
    /* Write the string length */
    dest[2]= id_length & 0xff;
    dest[3]= (id_length >> 8) & 0xff;
    /* Write the string data */
    if (id_length > 0)
      memcpy((char *)(dest + 4), id_name, id_length);
    digest_storage->m_byte_count+= bytes_needed;
  }
  else
  {
    digest_storage->m_full= true;
  }
}

#endif

