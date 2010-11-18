/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_INSTR_H
#define PFS_INSTR_H

/**
  @file storage/perfschema/pfs_instr.h
  Performance schema instruments (declarations).
*/

#include "pfs_lock.h"
#include "pfs_instr_class.h"
#include "pfs_events_waits.h"
#include "pfs_server.h"
#include "lf.h"

/**
  @addtogroup Performance_schema_buffers
  @{
*/

struct PFS_thread;

struct PFS_instr
{
  /** Internal lock. */
  pfs_lock m_lock;
  /** Instrument wait statistics chain. */
  PFS_single_stat_chain m_wait_stat;
};

/** Instrumented mutex implementation. @see PSI_mutex. */
struct PFS_mutex : public PFS_instr
{
  /** Mutex identity, typically a pthread_mutex_t. */
  const void *m_identity;
  /** Mutex class. */
  PFS_mutex_class *m_class;
  /**
    Mutex lock usage statistics chain.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_single_stat_chain m_lock_stat;
  /** Current owner. */
  PFS_thread *m_owner;
  /**
    Timestamp of the last lock.
    This statistic is not exposed in user visible tables yet.
  */
  ulonglong m_last_locked;
};

/** Instrumented rwlock implementation. @see PSI_rwlock. */
struct PFS_rwlock : public PFS_instr
{
  /** RWLock identity, typically a pthread_rwlock_t. */
  const void *m_identity;
  /** RWLock class. */
  PFS_rwlock_class *m_class;
  /**
    RWLock read lock usage statistics chain.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_single_stat_chain m_read_lock_stat;
  /**
    RWLock write lock usage statistics chain.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_single_stat_chain m_write_lock_stat;
  /** Current writer thread. */
  PFS_thread *m_writer;
  /** Current count of readers. */
  uint m_readers;
  /**
    Timestamp of the last write.
    This statistic is not exposed in user visible tables yet.
  */
  ulonglong m_last_written;
  /**
    Timestamp of the last read.
    This statistic is not exposed in user visible tables yet.
  */
  ulonglong m_last_read;
};

/** Instrumented cond implementation. @see PSI_cond. */
struct PFS_cond : public PFS_instr
{
  /** Condition identity, typically a pthread_cond_t. */
  const void *m_identity;
  /** Condition class. */
  PFS_cond_class *m_class;
  /** Condition instance usage statistics. */
  PFS_cond_stat m_cond_stat;
};

/** Instrumented File and FILE implementation. @see PSI_file. */
struct PFS_file : public PFS_instr
{
  /** File name. */
  char m_filename[FN_REFLEN];
  /** File name length in bytes. */
  uint m_filename_length;
  /** File class. */
  PFS_file_class *m_class;
  /** File usage statistics. */
  PFS_file_stat m_file_stat;
};

/** Instrumented table implementation. @see PSI_table. */
struct PFS_table : public PFS_instr
{
  /** Table share. */
  PFS_table_share *m_share;
  /** Table identity, typically a handler. */
  const void *m_identity;
};

/**
  @def LOCKER_STACK_SIZE
  Maximum number of nested waits.
*/
#define LOCKER_STACK_SIZE 3

/**
  @def PFS_MAX_ALLOC_RETRY
  Maximum number of times the code attempts to allocate an item
  from internal buffers, before giving up.
*/
#define PFS_MAX_ALLOC_RETRY 1000

#define PFS_MAX_SCAN_PASS 2

/**
  Helper to scan circular buffers.
  Given a buffer of size [0, max_size - 1],
  and a random starting point in the buffer,
  this helper returns up to two [first, last -1] intervals that:
  - fit into the [0, max_size - 1] range,
  - have a maximum combined length of at most PFS_MAX_ALLOC_RETRY.
*/
struct PFS_scan
{
public:
  void init(uint random, uint max_size);

  bool has_pass() const
  { return (m_pass < m_pass_max); }

  void next_pass()
  { m_pass++; }
  
  uint first() const
  { return m_first[m_pass]; }

  uint last() const
  { return m_last[m_pass]; }

private:
  uint m_pass;
  uint m_pass_max;
  uint m_first[PFS_MAX_SCAN_PASS];
  uint m_last[PFS_MAX_SCAN_PASS];
};


/** Instrumented thread implementation. @see PSI_thread. */
struct PFS_thread
{
  /** Internal lock. */
  pfs_lock m_lock;
  /** Pins for filename_hash. */
  LF_PINS *m_filename_hash_pins;
  /** Pins for table_share_hash. */
  LF_PINS *m_table_share_hash_pins;
  /** Event ID counter */
  ulonglong m_event_id;
  /** Thread instrumentation flag. */
  bool m_enabled;
  /** Internal thread identifier, unique. */
  ulong m_thread_internal_id;
  /** External (SHOW PROCESSLIST) thread identifier, not unique. */
  ulong m_thread_id;
  /** Thread class. */
  PFS_thread_class *m_class;
  /** Size of @c m_wait_locker_stack. */
  uint m_wait_locker_count;
  /**
    Stack of wait lockers.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_WAITS_CURRENT.
    For most locks, only 1 wait locker is used at a given time.
    For composite locks, several records are needed:
    - 1 for a 'logical' wait (for example on the GLOBAL READ LOCK state)
    - 1 for a 'physical' wait (for example on COND_refresh)
  */
  PFS_wait_locker m_wait_locker_stack[LOCKER_STACK_SIZE];
  /** True if the circular buffer @c m_waits_history is full. */
  bool m_waits_history_full;
  /** Current index in the circular buffer @c m_waits_history. */
  uint m_waits_history_index;
  /**
    Waits history circular buffer.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_WAITS_HISTORY.
  */
  PFS_events_waits *m_waits_history;
  /**
    Per thread waits aggregated statistics.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME.
  */
  PFS_single_stat_chain *m_instr_class_wait_stats;
};

PFS_thread *sanitize_thread(PFS_thread *unsafe);
const char *sanitize_file_name(const char *unsafe);

PFS_single_stat_chain*
find_per_thread_mutex_class_wait_stat(PFS_thread *thread,
                                      PFS_mutex_class *klass);

PFS_single_stat_chain*
find_per_thread_rwlock_class_wait_stat(PFS_thread *thread,
                                       PFS_rwlock_class *klass);

PFS_single_stat_chain*
find_per_thread_cond_class_wait_stat(PFS_thread *thread,
                                     PFS_cond_class *klass);

PFS_single_stat_chain*
find_per_thread_file_class_wait_stat(PFS_thread *thread,
                                     PFS_file_class *klass);

int init_instruments(const PFS_global_param *param);
void cleanup_instruments();
int init_file_hash();
void cleanup_file_hash();
PFS_mutex* create_mutex(PFS_mutex_class *mutex_class, const void *identity);
void destroy_mutex(PFS_mutex *pfs);
PFS_rwlock* create_rwlock(PFS_rwlock_class *klass, const void *identity);
void destroy_rwlock(PFS_rwlock *pfs);
PFS_cond* create_cond(PFS_cond_class *klass, const void *identity);
void destroy_cond(PFS_cond *pfs);

PFS_thread* create_thread(PFS_thread_class *klass, const void *identity,
                          ulong thread_id);

void destroy_thread(PFS_thread *pfs);

PFS_file* find_or_create_file(PFS_thread *thread, PFS_file_class *klass,
                              const char *filename, uint len);

void release_file(PFS_file *pfs);
void destroy_file(PFS_thread *thread, PFS_file *pfs);
PFS_table* create_table(PFS_table_share *share, const void *identity);
void destroy_table(PFS_table *pfs);

/* For iterators and show status. */

extern ulong mutex_max;
extern ulong mutex_lost;
extern ulong rwlock_max;
extern ulong rwlock_lost;
extern ulong cond_max;
extern ulong cond_lost;
extern ulong thread_max;
extern ulong thread_lost;
extern ulong file_max;
extern ulong file_lost;
extern long file_handle_max;
extern ulong file_handle_lost;
extern ulong table_max;
extern ulong table_lost;
extern ulong events_waits_history_per_thread;
extern ulong instr_class_per_thread;
extern ulong locker_lost;

/* Exposing the data directly, for iterators. */

extern PFS_mutex *mutex_array;
extern PFS_rwlock *rwlock_array;
extern PFS_cond *cond_array;
extern PFS_thread *thread_array;
extern PFS_file *file_array;
extern PFS_file **file_handle_array;
extern PFS_table *table_array;

void reset_events_waits_by_instance();
void reset_per_thread_wait_stat();
void reset_file_instance_io();

/** @} */
#endif

