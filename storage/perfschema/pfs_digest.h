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
         int m_byte_count;
         int m_last_id_index;
         char m_token_array[PFS_MAX_DIGEST_STORAGE_SIZE];
         PFS_digest_hash m_digest_hash;
       } typedef PFS_digest_storage;

/** A statement digest stat record. */
struct PFS_statements_digest_stat
{
  /**
    Digest Storage.
  */
  PFS_digest_storage m_digest_storage;
  /**
    Digest hash/LF Hash search key.
  */
  PFS_digest_hash m_md5_hash;

  /**
    Statement stat.
  */
  PFS_statement_stat m_stat;
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
void pfs_digest_add_token_v1(PSI_digest_locker *locker,
                             uint token,
                             char *yytext,
                             int yylen);
void pfs_digest_end_v1(PSI_digest_locker *locker);

#endif
