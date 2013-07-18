/* Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_DIGEST_H
#define PFS_DIGEST_H

/**
  @file storage/perfschema/pfs_digest.h
  Statement Digest data structures (declarations).
*/

#include "pfs_column_types.h"
#include "lf.h"
#include "pfs_stat.h"

#define PFS_SIZE_OF_A_TOKEN 2

extern bool flag_statements_digest;
extern ulong digest_max;
extern ulong digest_lost;
struct PFS_thread;

/* Fixed, per MD5 hash. */
#define PFS_MD5_SIZE 16

/**
  Structure to store a MD5 hash value (digest) for a statement.
*/
struct PFS_digest_key
{
  unsigned char m_md5[PFS_MD5_SIZE];
  char m_schema_name[NAME_LEN];
  uint m_schema_name_length;
};

/** A statement digest stat record. */
struct PFS_ALIGNED PFS_statements_digest_stat
{
  /** Digest Schema + MD5 Hash. */
  PFS_digest_key m_digest_key;

  /** Digest Storage. */
  PSI_digest_storage m_digest_storage;

  /** Statement stat. */
  PFS_statement_stat m_stat;

  /** First and last seen timestamps.*/
  ulonglong m_first_seen;
  ulonglong m_last_seen;

  /** Reset data for this record. */
  void reset_data();
  /** Reset data and remove index for this record. */
  void reset_index(PFS_thread *thread);
};

int init_digest(const PFS_global_param *param);
void cleanup_digest();

int init_digest_hash(void);
void cleanup_digest_hash(void);
PFS_statement_stat* find_or_create_digest(PFS_thread *thread,
                                          PSI_digest_storage *digest_storage,
                                          const char *schema_name,
                                          uint schema_name_length);

void get_digest_text(char *digest_text, PSI_digest_storage *digest_storage);

void reset_esms_by_digest();

/* Exposing the data directly, for iterators. */
extern PFS_statements_digest_stat *statements_digest_stat_array;

/* Instrumentation callbacks for pfs.cc */

struct PSI_digest_locker *pfs_digest_start_v1(PSI_statement_locker *locker);
PSI_digest_locker *pfs_digest_add_token_v1(PSI_digest_locker *locker,
                                           uint token,
                                           OPAQUE_LEX_YYSTYPE *yylval);

static inline void digest_reset(PSI_digest_storage *digest)
{
  digest->m_full= false;
  digest->m_byte_count= 0;
  digest->m_charset_number= 0;
}

static inline void digest_copy(PSI_digest_storage *to, const PSI_digest_storage *from)
{
  if (from->m_byte_count > 0)
  {
    to->m_full= from->m_full;
    to->m_byte_count= from->m_byte_count;
    to->m_charset_number= from->m_charset_number;
    DBUG_ASSERT(to->m_byte_count <= PSI_MAX_DIGEST_STORAGE_SIZE);
    memcpy(to->m_token_array, from->m_token_array, to->m_byte_count);
  }
  else
  {
    DBUG_ASSERT(from->m_byte_count == 0);
    to->m_full= false;
    to->m_byte_count= 0;
    to->m_charset_number= 0;
  }
}

/** 
  Read a single token from token array.
*/
inline int read_token(PSI_digest_storage *digest_storage,
                      int index, uint *tok)
{
  int safe_byte_count= digest_storage->m_byte_count;

  if (index + PFS_SIZE_OF_A_TOKEN <= safe_byte_count &&
      safe_byte_count <= PSI_MAX_DIGEST_STORAGE_SIZE)
  {
    unsigned char *src= & digest_storage->m_token_array[index];
    *tok= src[0] | (src[1] << 8);
    return index + PFS_SIZE_OF_A_TOKEN;
  }

  /* The input byte stream is exhausted. */
  *tok= 0;
  return PSI_MAX_DIGEST_STORAGE_SIZE + 1;
}

/**
  Store a single token in token array.
*/
inline void store_token(PSI_digest_storage* digest_storage, uint token)
{
  DBUG_ASSERT(digest_storage->m_byte_count >= 0);
  DBUG_ASSERT(digest_storage->m_byte_count <= PSI_MAX_DIGEST_STORAGE_SIZE);

  if (digest_storage->m_byte_count + PFS_SIZE_OF_A_TOKEN <= PSI_MAX_DIGEST_STORAGE_SIZE)
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
inline int read_identifier(PSI_digest_storage* digest_storage,
                           int index, char ** id_string, int *id_length)
{
  int new_index;
  DBUG_ASSERT(index <= digest_storage->m_byte_count);
  DBUG_ASSERT(digest_storage->m_byte_count <= PSI_MAX_DIGEST_STORAGE_SIZE);

  /*
    token + length + string are written in an atomic way,
    so we do always expect a length + string here
  */
  unsigned char *src= & digest_storage->m_token_array[index];
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
inline void store_token_identifier(PSI_digest_storage* digest_storage,
                                   uint token,
                                   uint id_length, const char *id_name)
{
  DBUG_ASSERT(digest_storage->m_byte_count >= 0);
  DBUG_ASSERT(digest_storage->m_byte_count <= PSI_MAX_DIGEST_STORAGE_SIZE);

  uint bytes_needed= 2 * PFS_SIZE_OF_A_TOKEN + id_length;
  if (digest_storage->m_byte_count + bytes_needed <= PSI_MAX_DIGEST_STORAGE_SIZE)
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

extern LF_HASH digest_hash;

#endif
