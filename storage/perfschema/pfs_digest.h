/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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
extern unsigned int statements_digest_size;
extern unsigned int digest_index;
extern ulong digest_max;
struct PFS_thread;

/* Fixed, per MD5 hash. */
#define PFS_MD5_SIZE 16

/**
  Structure to store a MD5 hash value (digest) for a statement.
*/
struct PFS_digest_hash
{
  unsigned char m_md5[PFS_MD5_SIZE];
};

/** A statement digest stat record. */
struct PFS_statements_digest_stat
{
  /**
    Digest MD5 Hash.
  */
  PFS_digest_hash m_digest_hash;

  /**
    Digest Storage.
  */
  PSI_digest_storage m_digest_storage;

  /**
    Statement stat.
  */
  PFS_statement_stat m_stat;

  /**
    First Seen/last seen.
  */
  ulonglong m_first_seen;
  ulonglong m_last_seen;

  void reset();
};

int init_digest(const PFS_global_param *param);
void cleanup_digest();

int init_digest_hash(void);
void cleanup_digest_hash(void);
PFS_statements_digest_stat* find_or_create_digest(PFS_thread*,
                                                  PSI_digest_storage*);

void get_digest_text(char* digest_text, PSI_digest_storage*);

void reset_esms_by_digest();

/* Exposing the data directly, for iterators. */
extern PFS_statements_digest_stat *statements_digest_stat_array;

/* Instrumentation callbacks for pfs.cc */

struct PSI_digest_locker* pfs_digest_start_v1(PSI_statement_locker *locker);
PSI_digest_locker* pfs_digest_add_token_v1(PSI_digest_locker *locker,
                                           uint token,
                                           OPAQUE_LEX_YYSTYPE *yylval);

static inline void digest_reset(PSI_digest_storage *digest)
{
  digest->m_full= false;
  digest->m_byte_count= 0;
}

static inline void digest_copy(PSI_digest_storage *to, const PSI_digest_storage *from)
{
  to->m_full= from->m_full;
  to->m_byte_count= from->m_byte_count;
  DBUG_ASSERT(to->m_byte_count <= PSI_MAX_DIGEST_STORAGE_SIZE);
  memcpy(to->m_token_array, from->m_token_array, to->m_byte_count);
}

/** 
  Function to read a single token from token array.
*/
inline void read_token(uint *tok, int *index, char *src)
{
  unsigned short sh;
  int remaining_bytes= PSI_MAX_DIGEST_STORAGE_SIZE - *index;
  DBUG_ASSERT(remaining_bytes >= 0);

  /* Make sure we have enough space to read a token from.
   */
  if(remaining_bytes >= PFS_SIZE_OF_A_TOKEN)
  {
    sh= ((0x00ff & src[*index + 1])<<8) | (0x00ff & src[*index]);
    *tok= (uint)(sh);
    *index= *index + PFS_SIZE_OF_A_TOKEN;
  }
}

/**
  Function to store a single token in token array.
*/
inline void store_token(PSI_digest_storage* digest_storage, uint token)
{
  char* dest= digest_storage->m_token_array;
  int* index= &digest_storage->m_byte_count;
  unsigned short sh= (unsigned short)token;
  int remaining_bytes= PSI_MAX_DIGEST_STORAGE_SIZE - *index;
  DBUG_ASSERT(remaining_bytes >= 0);

  /* Make sure we have enough space to write a token to. */
  if(remaining_bytes >= PFS_SIZE_OF_A_TOKEN)
  {
    dest[*index]= (sh) & 0xff;
    *index= *index + 1;
    dest[*index]= (sh>>8) & 0xff;
    *index= *index + 1;
  }
}

/**
  Function to read an identifier from token array.
*/
inline void read_identifier(char **dest, int *index, char *src,
                            uint available_bytes_to_write, uint offset)
{
  uint length;
  int remaining_bytes= PSI_MAX_DIGEST_STORAGE_SIZE - *index;
  DBUG_ASSERT(remaining_bytes >= 0);
  /*
    Read ID's length.
    Make sure that space to read ID's length from, is available.
  */
  if(remaining_bytes >= PFS_SIZE_OF_A_TOKEN &&
     available_bytes_to_write > offset)
  {
    read_token(&length, index, src);
    /*
      Make sure not to overflow digest_text buffer while writing
      identifier name.
      +/-offset is to make sure extra space for ''' and ' '.
    */
    length= available_bytes_to_write >= length+offset ?
                                        length :
                                        available_bytes_to_write-offset;
    strncpy(*dest, src + *index, length);
    *index= *index + length;
    *dest= *dest + length;
  }
}

/**
  Function to store an identifier in token array.
*/
inline void store_identifier(PSI_digest_storage* digest_storage,
                             uint id_length, char *id_name)
{
  char* dest= digest_storage->m_token_array;
  int* index= &digest_storage->m_byte_count;
  int remaining_bytes= PSI_MAX_DIGEST_STORAGE_SIZE - *index;
  DBUG_ASSERT(remaining_bytes >= 0);

  /*
    Store ID's length.
    Make sure that space, to store ID's length, is available.
  */
  if(remaining_bytes >= PFS_SIZE_OF_A_TOKEN)
  {
    /*
       Make sure to store ID length/ID as per the space available.
    */
    remaining_bytes-= PFS_SIZE_OF_A_TOKEN;
    id_length= id_length>(uint)remaining_bytes?(uint)remaining_bytes:id_length;
    store_token(digest_storage, id_length);
    strncpy(dest + *index, id_name, id_length);
    *index= *index + id_length;
  }
}

#endif
