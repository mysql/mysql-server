/* Copyright (C) 2008-2009 Sun Microsystems, Inc

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
  @file storage/perfschema/pfs_events_waits.cc
  Events waits data structures (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_global.h"
#include "pfs_instr.h"
#include "pfs_events_waits.h"
#include "pfs_atomic.h"
#include "m_string.h"

ulong events_waits_history_long_size= 0;
/** Consumer flag for table EVENTS_WAITS_CURRENT. */
bool flag_events_waits_current= true;
/** Consumer flag for table EVENTS_WAITS_HISTORY. */
bool flag_events_waits_history= true;
/** Consumer flag for table EVENTS_WAITS_HISTORY_LONG. */
bool flag_events_waits_history_long= true;
/** Consumer flag for table EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME. */
bool flag_events_waits_summary_by_thread_by_event_name= true;
/** Consumer flag for table EVENTS_WAITS_SUMMARY_BY_EVENT_NAME. */
bool flag_events_waits_summary_by_event_name= true;
/** Consumer flag for table EVENTS_WAITS_SUMMARY_BY_INSTANCE. */
bool flag_events_waits_summary_by_instance= true;
bool flag_events_locks_summary_by_thread_by_event_name= true;
bool flag_events_locks_summary_by_event_name= true;
bool flag_events_locks_summary_by_instance= true;
/** Consumer flag for table FILE_SUMMARY_BY_EVENT_NAME. */
bool flag_file_summary_by_event_name= true;
/** Consumer flag for table FILE_SUMMARY_BY_INSTANCE. */
bool flag_file_summary_by_instance= true;

/** True if EVENTS_WAITS_HISTORY_LONG circular buffer is full. */
bool events_waits_history_long_full= false;
/** Index in EVENTS_WAITS_HISTORY_LONG circular buffer. */
volatile uint32 events_waits_history_long_index= 0;
/** EVENTS_WAITS_HISTORY_LONG circular buffer. */
PFS_events_waits *events_waits_history_long_array= NULL;

/**
  Initialize table EVENTS_WAITS_HISTORY_LONG.
  @param events_waits_history_long_sizing       table sizing
*/
int init_events_waits_history_long(uint events_waits_history_long_sizing)
{
  events_waits_history_long_size= events_waits_history_long_sizing;
  events_waits_history_long_full= false;
  PFS_atomic::store_u32(&events_waits_history_long_index, 0);

  if (events_waits_history_long_size == 0)
    return 0;

  events_waits_history_long_array=
    PFS_MALLOC_ARRAY(events_waits_history_long_size, PFS_events_waits,
                     MYF(MY_ZEROFILL));

  return (events_waits_history_long_array ? 0 : 1);
}

/** Cleanup table EVENTS_WAITS_HISTORY_LONG. */
void cleanup_events_waits_history_long(void)
{
  pfs_free(events_waits_history_long_array);
  events_waits_history_long_array= NULL;
}

static void copy_events_waits(PFS_events_waits *dest,
                              const PFS_events_waits *source)
{
  /* m_wait_class must be the first member of PFS_events_waits. */
  compile_time_assert(offsetof(PFS_events_waits, m_wait_class) == 0);

  char* dest_body= (reinterpret_cast<char*> (dest)) + sizeof(events_waits_class);
  const char* source_body= (reinterpret_cast<const char*> (source))
    + sizeof(events_waits_class);

  /* See comments in table_events_waits_common::make_row(). */

  /* Signal readers they are about to read garbage ... */
  dest->m_wait_class= NO_WAIT_CLASS;
  /* ... that this can generate. */
  memcpy_fixed(dest_body,
               source_body,
               sizeof(PFS_events_waits) - sizeof(events_waits_class));
  /* Signal readers the record is now clean again. */
  dest->m_wait_class= source->m_wait_class;
}

/**
  Insert a wait record in table EVENTS_WAITS_HISTORY.
  @param thread             thread that executed the wait
  @param wait               record to insert
*/
void insert_events_waits_history(PFS_thread *thread, PFS_events_waits *wait)
{
  uint index= thread->m_waits_history_index;

  /*
    A concurrent thread executing TRUNCATE TABLE EVENTS_WAITS_CURRENT
    could alter the data that this thread is inserting,
    causing a potential race condition.
    We are not testing for this and insert a possibly empty record,
    to make this thread (the writer) faster.
    This is ok, the truncated data will have
    wait->m_wait_class == NO_WAIT_CLASS,
    which readers of m_waits_history will filter out.
  */
  copy_events_waits(&thread->m_waits_history[index], wait);

  index++;
  if (index >= events_waits_history_per_thread)
  {
    index= 0;
    thread->m_waits_history_full= true;
  }
  thread->m_waits_history_index= index;
}

/**
  Insert a wait record in table EVENTS_WAITS_HISTORY_LONG.
  @param wait               record to insert
*/
void insert_events_waits_history_long(PFS_events_waits *wait)
{
  uint index= PFS_atomic::add_u32(&events_waits_history_long_index, 1);

  index= index % events_waits_history_long_size;
  if (index == 0)
    events_waits_history_long_full= true;

  /* See related comment in insert_events_waits_history. */
  copy_events_waits(&events_waits_history_long_array[index], wait);
}

/** Reset table EVENTS_WAITS_CURRENT data. */
void reset_events_waits_current(void)
{
  PFS_thread *pfs_thread= thread_array;
  PFS_thread *pfs_thread_last= thread_array + thread_max;

  for ( ; pfs_thread < pfs_thread_last; pfs_thread++)
  {
    PFS_wait_locker *locker= pfs_thread->m_wait_locker_stack;
    PFS_wait_locker *locker_last= locker + LOCKER_STACK_SIZE;

    for ( ; locker < locker_last; locker++)
      locker->m_waits_current.m_wait_class= NO_WAIT_CLASS;
  }
}

/** Reset table EVENTS_WAITS_HISTORY data. */
void reset_events_waits_history(void)
{
  PFS_thread *pfs_thread= thread_array;
  PFS_thread *pfs_thread_last= thread_array + thread_max;

  for ( ; pfs_thread < pfs_thread_last; pfs_thread++)
  {
    PFS_events_waits *wait= pfs_thread->m_waits_history;
    PFS_events_waits *wait_last= wait + events_waits_history_per_thread;

    pfs_thread->m_waits_history_index= 0;
    pfs_thread->m_waits_history_full= false;
    for ( ; wait < wait_last; wait++)
      wait->m_wait_class= NO_WAIT_CLASS;
  }
}

/** Reset table EVENTS_WAITS_HISTORY_LONG data. */
void reset_events_waits_history_long(void)
{
  PFS_atomic::store_u32(&events_waits_history_long_index, 0);
  events_waits_history_long_full= false;

  PFS_events_waits *wait= events_waits_history_long_array;
  PFS_events_waits *wait_last= wait + events_waits_history_long_size;
  for ( ; wait < wait_last; wait++)
    wait->m_wait_class= NO_WAIT_CLASS;
}

