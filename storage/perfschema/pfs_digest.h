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

#define PFS_MAX_DIGEST_STORAGE_SIZE 1024
#define PFS_SIZE_OF_A_TOKEN 2

extern bool flag_statements_digest;
extern unsigned int statements_digest_size;
extern unsigned int digest_index;
struct PFS_thread;

/**
  Structure to store a MD5 hash value (digest) for a statement.
*/
struct {
         unsigned char m_md5[16];
       }typedef PFS_digest_hash;

/**
  Structure to store token count/array for a statement
  on which digest is to be calculated.
*/
struct {
         PFS_digest_hash m_digest_hash;
         int m_byte_count;
         int m_last_id_index;
         char m_token_array[PFS_MAX_DIGEST_STORAGE_SIZE];

         void reset();
       } typedef PFS_digest_storage;

/** A statement digest stat record. */
struct PFS_statements_digest_stat
{
  /**
    Digest Storage.
  */
  PFS_digest_storage m_digest_storage;

  /**
    Statement stat.
  */
  PFS_statement_stat m_stat;

  /**
    First Seen/last seen.
  */
  ulonglong m_first_seen;
  ulonglong m_last_seen;
};

int init_digest(unsigned int digest_sizing);
void cleanup_digest();

int init_digest_hash(void);
void cleanup_digest_hash(void);
PFS_statements_digest_stat* find_or_create_digest(PFS_thread*,
                                                  PFS_digest_storage*);

void get_digest_text(char* digest_text,
                            char* token_array,
                            int byte_count);

void reset_esms_by_digest();

/* Exposing the data directly, for iterators. */
extern PFS_statements_digest_stat *statements_digest_stat_array;

/* Instrumentation callbacks for pfs.cc */

struct PSI_digest_locker* pfs_digest_start_v1(PSI_statement_locker *locker);
PSI_digest_locker* pfs_digest_add_token_v1(PSI_digest_locker *locker,
                                           uint token,
                                           char *yytext,
                                           int yylen);

/** 
  Function to read a single token from token array.
*/
inline void read_token(uint *dest, int *index, char *src)
{
  unsigned short sh;
  int remaining_bytes= PFS_MAX_DIGEST_STORAGE_SIZE - *index;
  DBUG_ASSERT(remaining_bytes >= 0);

  /* Make sure we have enough space to read a token. */
  if(remaining_bytes >= PFS_SIZE_OF_A_TOKEN)
  {
    sh= ((0x00ff & src[*index + 1])<<8) | (0x00ff & src[*index]);
    *dest= (uint)(sh);
    *index= *index + PFS_SIZE_OF_A_TOKEN;
  }
}

/**
  Function to store a single token in token array.
*/
inline void store_token(PFS_digest_storage* digest_storage, uint token)
{
  char* dest= digest_storage->m_token_array;
  int* index= &digest_storage->m_byte_count;
  unsigned short sh= (unsigned short)token;
  int remaining_bytes= PFS_MAX_DIGEST_STORAGE_SIZE - *index;
  DBUG_ASSERT(remaining_bytes >= 0);

  /* Make sure we have enough space to store a token. */
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
inline void read_identifier(char **dest, int *index, char *src)
{
  uint length;
  int remaining_bytes= PFS_MAX_DIGEST_STORAGE_SIZE - *index;
  DBUG_ASSERT(remaining_bytes >= 0);
  /*
    Read ID's length.
    Make sure that space, to read ID's length, is available.
  */
  if(remaining_bytes >= PFS_SIZE_OF_A_TOKEN)
  {
    read_token(&length, index, src);
    /*
      While storing ID length, it has already been stored
      in a way that ID doesn't go beyond the storage size,
      so no need to check length here.
    */
    strncpy(*dest, src + *index, length);
    *index= *index + length;
    *dest= *dest + length;
  }
}

/**
  Function to store an identifier in token array.
*/
inline void store_identifier(PFS_digest_storage* digest_storage,
                             uint id_length, char *id_name)
{
  char* dest= digest_storage->m_token_array;
  int* index= &digest_storage->m_byte_count;
  int remaining_bytes= PFS_MAX_DIGEST_STORAGE_SIZE - *index;
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

/**
  Function to read last two tokens from token array. If an identifier
  is found, do not look for token after that.
*/
inline void read_last_two_tokens(PFS_digest_storage* digest_storage,
                                 uint *t1, uint *t2)
{
  int last_token_index;
  int last_id_index= digest_storage->m_last_id_index;
  int byte_count= digest_storage->m_byte_count;

  if(last_id_index <= byte_count - PFS_SIZE_OF_A_TOKEN)
  {
    /* Take last token. */
    last_token_index= byte_count - PFS_SIZE_OF_A_TOKEN;
    DBUG_ASSERT(last_token_index >= 0);
    read_token(t1, &last_token_index, digest_storage->m_token_array);
  }
  if(last_id_index <= byte_count - 2*PFS_SIZE_OF_A_TOKEN)
  {
    /* Take 2nd token from last. */
    last_token_index= byte_count - 2*PFS_SIZE_OF_A_TOKEN;
    DBUG_ASSERT(last_token_index >= 0);
    read_token(t2, &last_token_index, digest_storage->m_token_array);
  }
}

#endif
