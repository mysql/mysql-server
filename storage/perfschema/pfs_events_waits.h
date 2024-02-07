/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_EVENTS_WAITS_H
#define PFS_EVENTS_WAITS_H

/**
  @file storage/perfschema/pfs_events_waits.h
  Events waits data structures (declarations).
*/

#include <sys/types.h>
#include <atomic>

#include "my_inttypes.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_events.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_lock.h"

struct PFS_mutex;
struct PFS_rwlock;
struct PFS_cond;
struct PFS_table;
struct PFS_file;
struct PFS_thread;
struct PFS_socket;
struct PFS_instr_class;
struct PFS_table_share;
struct PFS_account;
struct PFS_user;
struct PFS_host;
struct PFS_metadata_lock;

/** Class of a wait event. */
enum events_waits_class {
  NO_WAIT_CLASS = 0,
  WAIT_CLASS_MUTEX,
  WAIT_CLASS_RWLOCK,
  WAIT_CLASS_COND,
  WAIT_CLASS_TABLE,
  WAIT_CLASS_FILE,
  WAIT_CLASS_SOCKET,
  WAIT_CLASS_IDLE,
  WAIT_CLASS_METADATA
};

/** A wait event record. */
struct PFS_events_waits : public PFS_events {
  /**
    The type of wait.
    Readers:
    - the consumer threads.
    Writers:
    - the producer threads, in the instrumentation.
    Out of bound Writers:
    - TRUNCATE EVENTS_WAITS_CURRENT
    - TRUNCATE EVENTS_WAITS_HISTORY
    - TRUNCATE EVENTS_WAITS_HISTORY_LONG
  */
  events_waits_class m_wait_class;
  /** Object type */
  enum_object_type m_object_type;
  /** Table share, for table operations only. */
  PFS_table_share *m_weak_table_share;
  /** File, for file operations only. */
  PFS_file *m_weak_file;
  /** Socket, for socket operations only. */
  PFS_socket *m_weak_socket;
  /** Metadata lock, for MDL operations only. */
  PFS_metadata_lock *m_weak_metadata_lock;
  /** For weak pointers, target object version. */
  uint32 m_weak_version;
  /** Address in memory of the object instance waited on. */
  const void *m_object_instance_addr;
  /** Operation performed. */
  enum_operation_type m_operation;
  /**
    Number of bytes/rows read/written.
    This member is populated for FILE READ/WRITE operations, with a number of
    bytes.
    This member is populated for TABLE I/O operations, with a number of rows.
  */
  size_t m_number_of_bytes;
  /**
    Index used.
    This member is populated for TABLE I/O operations only.
  */
  uint m_index;
  /** Flags */
  ulong m_flags;
};

/** TIMED bit in the state flags bitfield. */
#define STATE_FLAG_TIMED (1 << 0)
/** THREAD bit in the state flags bitfield. */
#define STATE_FLAG_THREAD (1 << 1)
/** EVENT bit in the state flags bitfield. */
#define STATE_FLAG_EVENT (1 << 2)
/** DIGEST bit in the state flags bitfield. */
#define STATE_FLAG_DIGEST (1 << 3)
/** CPU bit in the state flags bitfield. */
#define STATE_FLAG_CPU (1 << 4)
/** BASE bit in the state flags bitfield. */
#define STATE_FLAG_BASE (1 << 5)

void insert_events_waits_history(PFS_thread *thread, PFS_events_waits *wait);

void insert_events_waits_history_long(PFS_events_waits *wait);

extern bool flag_events_waits_current;
extern bool flag_events_waits_history;
extern bool flag_events_waits_history_long;
extern bool flag_global_instrumentation;
extern bool flag_thread_instrumentation;

extern bool events_waits_history_long_full;
extern PFS_cacheline_atomic_uint32 events_waits_history_long_index;
extern PFS_events_waits *events_waits_history_long_array;
extern ulong events_waits_history_long_size;

int init_events_waits_history_long(uint events_waits_history_long_sizing);
void cleanup_events_waits_history_long();

void reset_events_waits_current();
void reset_events_waits_history();
void reset_events_waits_history_long();
void reset_events_waits_by_thread();
void reset_events_waits_by_account();
void reset_events_waits_by_user();
void reset_events_waits_by_host();
void reset_events_waits_global();
void aggregate_account_waits(PFS_account *account);
void aggregate_user_waits(PFS_user *user);
void aggregate_host_waits(PFS_host *host);

void reset_table_waits_by_table();
void reset_table_io_waits_by_table();
void reset_table_lock_waits_by_table();
void reset_table_waits_by_table_handle();
void reset_table_io_waits_by_table_handle();
void reset_table_lock_waits_by_table_handle();

#endif
