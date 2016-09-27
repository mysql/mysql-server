/* Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_digest.h"

extern bool flag_statements_digest;
extern size_t digest_max;
extern ulong digest_lost;
struct PFS_thread;

/**
  Structure to store a MD5 hash value (digest) for a statement.
*/
struct PFS_digest_key
{
  unsigned char m_md5[MD5_HASH_SIZE];
  char m_schema_name[NAME_LEN];
  uint m_schema_name_length;
};

/** A statement digest stat record. */
struct PFS_ALIGNED PFS_statements_digest_stat
{
  /** Internal lock. */
  pfs_lock m_lock;

  /** Digest Schema + MD5 Hash. */
  PFS_digest_key m_digest_key;

  /** Digest Storage. */
  sql_digest_storage m_digest_storage;

  /** Statement stat. */
  PFS_statement_stat m_stat;

  /** First and last seen timestamps.*/
  ulonglong m_first_seen;
  ulonglong m_last_seen;

  /** Reset data for this record. */
  void reset_data(unsigned char* token_array, size_t length);
  /** Reset data and remove index for this record. */
  void reset_index(PFS_thread *thread);
};

int init_digest(const PFS_global_param *param);
void cleanup_digest();

int init_digest_hash(const PFS_global_param *param);
void cleanup_digest_hash(void);
PFS_statement_stat* find_or_create_digest(PFS_thread *thread,
                                          const sql_digest_storage *digest_storage,
                                          const char *schema_name,
                                          uint schema_name_length);

void reset_esms_by_digest();

/* Exposing the data directly, for iterators. */
extern PFS_statements_digest_stat *statements_digest_stat_array;

extern LF_HASH digest_hash;

#endif

