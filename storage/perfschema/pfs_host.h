/* Copyright (c) 2010, 2019, Oracle and/or its affiliates. All rights reserved.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

#ifndef PFS_HOST_H
#define PFS_HOST_H

/**
  @file storage/perfschema/pfs_host.h
  Performance schema host (declarations).
*/

#include <sys/types.h>
#include <atomic>

#include "lf.h"
#include "my_hostname.h" /* HOSTNAME_LENGTH */
#include "my_inttypes.h"
#include "mysql_com.h"
#include "storage/perfschema/pfs_con_slice.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_lock.h"

struct PFS_global_param;
struct PFS_memory_stat_delta;
struct PFS_memory_shared_stat;
struct PFS_thread;

/**
  @addtogroup performance_schema_buffers
  @{
*/

/** Hash key for a host. */
struct PFS_host_key {
  /**
    Hash search key.
    This has to be a string for @c LF_HASH,
    the format is @c "<hostname><0x00>"
  */
  char m_hash_key[HOSTNAME_LENGTH + 1];
  uint m_key_length;
};

/** Per host statistics. */
struct PFS_ALIGNED PFS_host : PFS_connection_slice {
 public:
  inline void init_refcount(void) { m_refcount.store(1); }

  inline int get_refcount(void) { return m_refcount.load(); }

  inline void inc_refcount(void) { ++m_refcount; }

  inline void dec_refcount(void) { --m_refcount; }

  void aggregate(bool alive);
  void aggregate_waits(void);
  void aggregate_stages(void);
  void aggregate_statements(void);
  void aggregate_transactions(void);
  void aggregate_errors(void);
  void aggregate_memory(bool alive);
  void aggregate_status(void);
  void aggregate_stats(void);
  void release(void);

  /** Reset all memory statistics. */
  void rebase_memory_stats();

  void carry_memory_stat_delta(PFS_memory_stat_delta *delta, uint index);

  void set_instr_class_memory_stats(PFS_memory_shared_stat *array) {
    m_has_memory_stats = false;
    m_instr_class_memory_stats = array;
  }

  const PFS_memory_shared_stat *read_instr_class_memory_stats() const {
    if (!m_has_memory_stats) {
      return NULL;
    }
    return m_instr_class_memory_stats;
  }

  PFS_memory_shared_stat *write_instr_class_memory_stats() {
    if (!m_has_memory_stats) {
      rebase_memory_stats();
      m_has_memory_stats = true;
    }
    return m_instr_class_memory_stats;
  }

  /* Internal lock. */
  pfs_lock m_lock;
  PFS_host_key m_key;
  const char *m_hostname;
  uint m_hostname_length;

  ulonglong m_disconnected_count;

 private:
  std::atomic<int> m_refcount;

  /**
    Per host memory aggregated statistics.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME.
    Immutable, safe to use without internal lock.
  */
  PFS_memory_shared_stat *m_instr_class_memory_stats;
};

int init_host(const PFS_global_param *param);
void cleanup_host(void);
int init_host_hash(const PFS_global_param *param);
void cleanup_host_hash(void);

PFS_host *find_or_create_host(PFS_thread *thread, const char *hostname,
                              uint hostname_length);

PFS_host *sanitize_host(PFS_host *unsafe);
void purge_all_host(void);

/* For show status. */

extern LF_HASH host_hash;

/** @} */
#endif
