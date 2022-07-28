/* Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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

#ifndef PFS_DIGEST_H
#define PFS_DIGEST_H

/**
  @file storage/perfschema/pfs_digest.h
  Statement Digest data structures (declarations).
*/

#include <sys/types.h>
#include <atomic>

#include "lf.h"
#include "my_inttypes.h"
#include "mysql_com.h"
#include "sql/sql_digest.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_histogram.h"
#include "storage/perfschema/pfs_lock.h"
#include "storage/perfschema/pfs_name.h"
#include "storage/perfschema/pfs_stat.h"

extern bool flag_statements_digest;
extern size_t digest_max;
extern ulong digest_lost;
struct PFS_thread;

/**
  Structure to store a hash value (digest) for a statement.
*/
struct PFS_digest_key {
  PFS_schema_name m_schema_name;
  unsigned char m_hash[DIGEST_HASH_SIZE];
};

/** A statement digest stat record. */
struct PFS_ALIGNED PFS_statements_digest_stat {
  /** Internal lock. */
  pfs_lock m_lock;

  /** Digest Schema + Digest Hash. */
  PFS_digest_key m_digest_key;

  /** Digest Storage. */
  sql_digest_storage m_digest_storage;

  /** Statement stat. */
  PFS_statement_stat m_stat;

  /** Query sample SQL text. */
  char *m_query_sample;
  /** Length of @c m_query_sample. */
  size_t m_query_sample_length;
  /** True if @c m_query_sample was truncated. */
  bool m_query_sample_truncated;
  /** Statement character set number. */
  uint m_query_sample_cs_number;
  /** Query sample seen timestamp.*/
  ulonglong m_query_sample_seen;
  /** Query sample timer wait.*/
  std::atomic<std::uint64_t> m_query_sample_timer_wait;
  /** Query sample reference count. */
  std::atomic<std::uint32_t> m_query_sample_refs;

  /** First and last seen timestamps.*/
  ulonglong m_first_seen;
  ulonglong m_last_seen;

  // FIXME : allocate in separate buffer
  PFS_histogram m_histogram;

  /** Reset data for this record. */
  void reset_data(unsigned char *token_array, size_t token_array_length,
                  char *query_sample_array);
  /** Reset data and remove index for this record. */
  void reset_index(PFS_thread *thread);

  /** Get the age in micro seconds of the last query sample. */
  ulonglong get_sample_age() {
    ulonglong age = m_last_seen - m_query_sample_seen;
    return age;
  }

  /** Set the query sample wait time. */
  void set_sample_timer_wait(ulonglong wait_time) {
    m_query_sample_timer_wait.store(wait_time);
  }

  /** Get the query sample wait time. */
  ulonglong get_sample_timer_wait() { return m_query_sample_timer_wait.load(); }

  /** Increment the query sample reference count. */
  uint inc_sample_ref() {
    /* Return value prior to increment. */
    return (uint)m_query_sample_refs.fetch_add(1);
  }

  /** Decrement the query sample reference count. */
  uint dec_sample_ref() {
    /* Return value prior to decrement. */
    return (uint)m_query_sample_refs.fetch_sub(1);
  }
};

int init_digest(const PFS_global_param *param);
void cleanup_digest();

int init_digest_hash(const PFS_global_param *param);
void cleanup_digest_hash(void);
PFS_statements_digest_stat *find_or_create_digest(
    PFS_thread *thread, const sql_digest_storage *digest_storage,
    const char *schema_name, uint schema_name_length);

void reset_esms_by_digest();
void reset_histogram_by_digest();

/* Exposing the data directly, for iterators. */
extern PFS_statements_digest_stat *statements_digest_stat_array;

extern LF_HASH digest_hash;

#endif
