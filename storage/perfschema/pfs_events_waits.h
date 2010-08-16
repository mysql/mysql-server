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

#ifndef PFS_EVENTS_WAITS_H
#define PFS_EVENTS_WAITS_H

/**
  @file storage/perfschema/pfs_events_waits.h
  Events waits data structures (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_lock.h"

struct PFS_mutex;
struct PFS_rwlock;
struct PFS_cond;
struct PFS_table;
struct PFS_file;
struct PFS_thread;
struct PFS_instr_class;

/** Class of a wait event. */
enum events_waits_class
{
  NO_WAIT_CLASS= 0,
  WAIT_CLASS_MUTEX,
  WAIT_CLASS_RWLOCK,
  WAIT_CLASS_COND,
  WAIT_CLASS_TABLE,
  WAIT_CLASS_FILE
};

/** State of a timer. */
enum timer_state
{
  /**
    Not timed.
    In this state, TIMER_START, TIMER_END and TIMER_WAIT are NULL.
  */
  TIMER_STATE_UNTIMED,
  /**
    About to start.
    In this state, TIMER_START, TIMER_END and TIMER_WAIT are NULL.
  */
  TIMER_STATE_STARTING,
  /**
    Started, but not yet ended.
    In this state, TIMER_START has a value, TIMER_END and TIMER_WAIT are NULL.
  */
  TIMER_STATE_STARTED,
  /**
    Ended.
    In this state, TIMER_START, TIMER_END and TIMER_WAIT have a value.
  */
  TIMER_STATE_TIMED
};

/** Target object a wait event is waiting on. */
union events_waits_target
{
  /** Mutex waited on. */
  PFS_mutex *m_mutex;
  /** RWLock waited on. */
  PFS_rwlock *m_rwlock;
  /** Condition waited on. */
  PFS_cond *m_cond;
  /** Table waited on. */
  PFS_table *m_table;
  /** File waited on. */
  PFS_file *m_file;
};

/** A wait event record. */
struct PFS_events_waits
{
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
  /** Executing thread. */
  PFS_thread *m_thread;
  /** Instrument metadata. */
  PFS_instr_class *m_class;
  /** Timer state. */
  enum timer_state m_timer_state;
  /** Event id. */
  ulonglong m_event_id;
  /**
    Timer start.
    This member is populated only if m_timed is true.
  */
  ulonglong m_timer_start;
  /**
    Timer end.
    This member is populated only if m_timed is true.
  */
  ulonglong m_timer_end;
  /** Schema name. */
  const char *m_schema_name;
  /** Length in bytes of @c m_schema_name. */
  uint m_schema_name_length;
  /** Object name. */
  const char *m_object_name;
  /** Length in bytes of @c m_object_name. */
  uint m_object_name_length;
  /** Address in memory of the object instance waited on. */
  const void *m_object_instance_addr;
  /** Location of the instrumentation in the source code (file name). */
  const char *m_source_file;
  /** Location of the instrumentation in the source code (line number). */
  uint m_source_line;
  /** Operation performed. */
  enum_operation_type m_operation;
  /**
    Number of bytes read/written.
    This member is populated for file READ/WRITE operations only.
  */
  size_t m_number_of_bytes;
};

/**
  A wait locker.
  A locker is a transient helper structure used by the instrumentation
  during the recording of a wait.
*/
struct PFS_wait_locker
{
  /** The timer used to measure the wait. */
  enum_timer_name m_timer_name;
  /** The object waited on. */
  events_waits_target m_target;
  /** The wait data recorded. */
  PFS_events_waits m_waits_current;
};

void insert_events_waits_history(PFS_thread *thread, PFS_events_waits *wait);

void insert_events_waits_history_long(PFS_events_waits *wait);

extern bool flag_events_waits_current;
extern bool flag_events_waits_history;
extern bool flag_events_waits_history_long;
extern bool flag_events_waits_summary_by_thread_by_event_name;
extern bool flag_events_waits_summary_by_event_name;
extern bool flag_events_waits_summary_by_instance;
extern bool flag_events_locks_summary_by_thread_by_name;
extern bool flag_events_locks_summary_by_event_name;
extern bool flag_events_locks_summary_by_instance;
extern bool flag_file_summary_by_event_name;
extern bool flag_file_summary_by_instance;
extern bool events_waits_history_long_full;
extern volatile uint32 events_waits_history_long_index;
extern PFS_events_waits *events_waits_history_long_array;
extern ulong events_waits_history_long_size;

int init_events_waits_history_long(uint events_waits_history_long_sizing);
void cleanup_events_waits_history_long();

void reset_events_waits_current();
void reset_events_waits_history();
void reset_events_waits_history_long();

#endif

