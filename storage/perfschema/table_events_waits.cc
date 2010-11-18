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

/**
  @file storage/perfschema/table_events_waits.cc
  Table EVENTS_WAITS_xxx (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "table_events_waits.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_events_waits.h"

THR_LOCK table_events_waits_current::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("THREAD_ID") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("EVENT_NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SOURCE") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMER_START") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMER_END") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SPINS") },
    { C_STRING_WITH_LEN("int(10)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_SCHEMA") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_NAME") },
    { C_STRING_WITH_LEN("varchar(512)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_TYPE") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_INSTANCE_BEGIN") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("NESTING_EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OPERATION") },
    { C_STRING_WITH_LEN("varchar(16)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("NUMBER_OF_BYTES") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("FLAGS") },
    { C_STRING_WITH_LEN("int(10)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_events_waits_current::m_field_def=
{ 16, field_types };

PFS_engine_table_share
table_events_waits_current::m_share=
{
  { C_STRING_WITH_LEN("events_waits_current") },
  &pfs_truncatable_acl,
  &table_events_waits_current::create,
  NULL, /* write_row */
  &table_events_waits_current::delete_all_rows,
  1000, /* records */
  sizeof(pos_events_waits_current), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

THR_LOCK table_events_waits_history::m_table_lock;

PFS_engine_table_share
table_events_waits_history::m_share=
{
  { C_STRING_WITH_LEN("events_waits_history") },
  &pfs_truncatable_acl,
  &table_events_waits_history::create,
  NULL, /* write_row */
  &table_events_waits_history::delete_all_rows,
  1000, /* records */
  sizeof(pos_events_waits_history), /* ref length */
  &m_table_lock,
  &table_events_waits_current::m_field_def,
  false /* checked */
};

THR_LOCK table_events_waits_history_long::m_table_lock;

PFS_engine_table_share
table_events_waits_history_long::m_share=
{
  { C_STRING_WITH_LEN("events_waits_history_long") },
  &pfs_truncatable_acl,
  &table_events_waits_history_long::create,
  NULL, /* write_row */
  &table_events_waits_history_long::delete_all_rows,
  10000, /* records */
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &table_events_waits_current::m_field_def,
  false /* checked */
};

table_events_waits_common::table_events_waits_common
(const PFS_engine_table_share *share, void *pos)
  : PFS_engine_table(share, pos),
  m_row_exists(false)
{}

void table_events_waits_common::clear_object_columns()
{
  m_row.m_object_type= NULL;
  m_row.m_object_type_length= 0;
  m_row.m_object_schema_length= 0;
  m_row.m_object_name_length= 0;
}

/**
  Build a row.
  @param thread_own_wait            True if the memory for the wait
    is owned by pfs_thread
  @param pfs_thread                 the thread the cursor is reading
  @param wait                       the wait the cursor is reading
*/
void table_events_waits_common::make_row(bool thread_own_wait,
                                         PFS_thread *pfs_thread,
                                         volatile PFS_events_waits *wait)
{
  pfs_lock lock;
  PFS_thread *safe_thread;
  PFS_instr_class *safe_class;
  const char *base;
  const char *safe_source_file;
  const char *safe_table_schema_name;
  const char *safe_table_object_name;
  const char *safe_file_name;

  m_row_exists= false;
  safe_thread= sanitize_thread(pfs_thread);
  if (unlikely(safe_thread == NULL))
    return;

  /* Protect this reader against a thread termination */
  if (thread_own_wait)
    safe_thread->m_lock.begin_optimistic_lock(&lock);

  /*
    Design choice:
    We could have used a pfs_lock in PFS_events_waits here,
    to protect the reader from concurrent event generation,
    but this leads to too many pfs_lock atomic operations
    each time an event is recorded:
    - 1 dirty() + 1 allocated() per event start, for EVENTS_WAITS_CURRENT
    - 1 dirty() + 1 allocated() per event end, for EVENTS_WAITS_CURRENT
    - 1 dirty() + 1 allocated() per copy to EVENTS_WAITS_HISTORY
    - 1 dirty() + 1 allocated() per copy to EVENTS_WAITS_HISTORY_LONG
    or 8 atomics per recorded event.
    The problem is that we record a *lot* of events ...

    This code is prepared to accept *dirty* records,
    and sanitizes all the data before returning a row.
  */

  m_row.m_thread_internal_id= safe_thread->m_thread_internal_id;
  m_row.m_event_id= wait->m_event_id;
  m_row.m_timer_state= wait->m_timer_state;
  m_row.m_timer_start= wait->m_timer_start;
  m_row.m_timer_end= wait->m_timer_end;
  m_row.m_object_instance_addr= (intptr) wait->m_object_instance_addr;

  /*
    PFS_events_waits::m_class needs to be sanitized,
    for race conditions when this code:
    - reads a new value in m_wait_class,
    - reads an old value in m_class.
  */
  switch (wait->m_wait_class)
  {
  case WAIT_CLASS_MUTEX:
    clear_object_columns();
    safe_class= sanitize_mutex_class((PFS_mutex_class*) wait->m_class);
    break;
  case WAIT_CLASS_RWLOCK:
    clear_object_columns();
    safe_class= sanitize_rwlock_class((PFS_rwlock_class*) wait->m_class);
    break;
  case WAIT_CLASS_COND:
    clear_object_columns();
    safe_class= sanitize_cond_class((PFS_cond_class*) wait->m_class);
    break;
  case WAIT_CLASS_TABLE:
    m_row.m_object_type= "TABLE";
    m_row.m_object_type_length= 5;
    m_row.m_object_schema_length= wait->m_schema_name_length;
    safe_table_schema_name= sanitize_table_schema_name(wait->m_schema_name);
    if (unlikely((m_row.m_object_schema_length == 0) ||
                 (m_row.m_object_schema_length > sizeof(m_row.m_object_schema)) ||
                 (safe_table_schema_name == NULL)))
      return;
    memcpy(m_row.m_object_schema, safe_table_schema_name, m_row.m_object_schema_length);
    m_row.m_object_name_length= wait->m_object_name_length;
    safe_table_object_name= sanitize_table_object_name(wait->m_object_name);
    if (unlikely((m_row.m_object_name_length == 0) ||
                 (m_row.m_object_name_length > sizeof(m_row.m_object_name)) ||
                 (safe_table_object_name == NULL)))
      return;
    memcpy(m_row.m_object_name, safe_table_object_name, m_row.m_object_name_length);
    safe_class= &global_table_class;
    break;
  case WAIT_CLASS_FILE:
    m_row.m_object_type= "FILE";
    m_row.m_object_type_length= 4;
    m_row.m_object_schema_length= 0;
    m_row.m_object_name_length= wait->m_object_name_length;
    safe_file_name= sanitize_file_name(wait->m_object_name);
    if (unlikely((m_row.m_object_name_length == 0) ||
                 (m_row.m_object_name_length > sizeof(m_row.m_object_name)) ||
                 (safe_file_name == NULL)))
      return;
    memcpy(m_row.m_object_name, safe_file_name, m_row.m_object_name_length);
    safe_class= sanitize_file_class((PFS_file_class*) wait->m_class);
    break;
  case NO_WAIT_CLASS:
  default:
    return;
  }
  if (unlikely(safe_class == NULL))
    return;
  m_row.m_name= safe_class->m_name;
  m_row.m_name_length= safe_class->m_name_length;

  /*
    We are assuming this pointer is sane,
    since it comes from __FILE__.
  */
  safe_source_file= wait->m_source_file;
  if (unlikely(safe_source_file == NULL))
    return;

  base= base_name(wait->m_source_file);
  m_row.m_source_length= my_snprintf(m_row.m_source, sizeof(m_row.m_source),
                                     "%s:%d", base, wait->m_source_line);
  if (m_row.m_source_length > sizeof(m_row.m_source))
    m_row.m_source_length= sizeof(m_row.m_source);
  m_row.m_operation= wait->m_operation;
  m_row.m_number_of_bytes= wait->m_number_of_bytes;
  m_row.m_flags= 0;

  if (thread_own_wait)
  {
    if (safe_thread->m_lock.end_optimistic_lock(&lock))
      m_row_exists= true;
  }
  else
  {
    /*
      For EVENTS_WAITS_HISTORY_LONG (thread_own_wait is false),
      the wait record is always valid, because it is not stored
      in memory owned by pfs_thread.
      Even when the thread terminated, the record is mostly readable,
      so this record is displayed.
    */
    m_row_exists= true;
  }
}

/**
  Operations names map, as displayed in the 'OPERATION' column.
  Indexed by enum_operation_type - 1.
  Note: enum_operation_type contains a more precise definition,
  since more details are needed internally by the instrumentation.
  Different similar operations (CLOSE vs STREAMCLOSE) are displayed
  with the same name 'close'.
*/
static const LEX_STRING operation_names_map[]=
{
  /* Mutex operations */
  { C_STRING_WITH_LEN("lock") },
  { C_STRING_WITH_LEN("try_lock") },

  /* RWLock operations */
  { C_STRING_WITH_LEN("read_lock") },
  { C_STRING_WITH_LEN("write_lock") },
  { C_STRING_WITH_LEN("try_read_lock") },
  { C_STRING_WITH_LEN("try_write_lock") },

  /* Condition operations */
  { C_STRING_WITH_LEN("wait") },
  { C_STRING_WITH_LEN("timed_wait") },

  /* File operations */
  { C_STRING_WITH_LEN("create") },
  { C_STRING_WITH_LEN("create") }, /* create tmp */
  { C_STRING_WITH_LEN("open") },
  { C_STRING_WITH_LEN("open") }, /* stream open */
  { C_STRING_WITH_LEN("close") },
  { C_STRING_WITH_LEN("close") }, /* stream close */
  { C_STRING_WITH_LEN("read") },
  { C_STRING_WITH_LEN("write") },
  { C_STRING_WITH_LEN("seek") },
  { C_STRING_WITH_LEN("tell") },
  { C_STRING_WITH_LEN("flush") },
  { C_STRING_WITH_LEN("stat") },
  { C_STRING_WITH_LEN("stat") }, /* fstat */
  { C_STRING_WITH_LEN("chsize") },
  { C_STRING_WITH_LEN("delete") },
  { C_STRING_WITH_LEN("rename") },
  { C_STRING_WITH_LEN("sync") }
};


int table_events_waits_common::read_row_values(TABLE *table,
                                               unsigned char *buf,
                                               Field **fields,
                                               bool read_all)
{
  Field *f;
  const LEX_STRING *operation;

  compile_time_assert(COUNT_OPERATION_TYPE ==
                      array_elements(operation_names_map));

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 2);
  buf[0]= 0;
  buf[1]= 0;

  /*
    Some columns are unreliable, because they are joined with other buffers,
    which could have changed and been reused for something else.
    These columns are:
    - THREAD_ID (m_thread joins with PFS_thread),
    - SCHEMA_NAME (m_schema_name joins with PFS_table_share)
    - OBJECT_NAME (m_object_name joins with PFS_table_share)
  */
  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* THREAD_ID */
        set_field_ulong(f, m_row.m_thread_internal_id);
        break;
      case 1: /* EVENT_ID */
        set_field_ulonglong(f, m_row.m_event_id);
        break;
      case 2: /* EVENT_NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 3: /* SOURCE */
        set_field_varchar_utf8(f, m_row.m_source, m_row.m_source_length);
        break;
      case 4: /* TIMER_START */
        if ((m_row.m_timer_state == TIMER_STATE_STARTED) ||
            (m_row.m_timer_state == TIMER_STATE_TIMED))
          set_field_ulonglong(f, m_row.m_timer_start);
        else
          f->set_null();
        break;
      case 5: /* TIMER_END */
        if (m_row.m_timer_state == TIMER_STATE_TIMED)
          set_field_ulonglong(f, m_row.m_timer_end);
        else
          f->set_null();
        break;
      case 6: /* TIMER_WAIT */
        if (m_row.m_timer_state == TIMER_STATE_TIMED)
          set_field_ulonglong(f, m_row.m_timer_end - m_row.m_timer_start);
        else
          f->set_null();
        break;
      case 7: /* SPINS */
        f->set_null();
        break;
      case 8: /* OBJECT_SCHEMA */
        if (m_row.m_object_schema_length > 0)
        {
          set_field_varchar_utf8(f, m_row.m_object_schema,
                                 m_row.m_object_schema_length);
        }
        else
          f->set_null();
        break;
      case 9: /* OBJECT_NAME */
        if (m_row.m_object_name_length > 0)
        {
          set_field_varchar_utf8(f, m_row.m_object_name,
                                 m_row.m_object_name_length);
        }
        else
          f->set_null();
        break;
      case 10: /* OBJECT_TYPE */
        if (m_row.m_object_type)
        {
          set_field_varchar_utf8(f, m_row.m_object_type,
                                 m_row.m_object_type_length);
        }
        else
          f->set_null();
        break;
      case 11: /* OBJECT_INSTANCE */
        set_field_ulonglong(f, m_row.m_object_instance_addr);
        break;
      case 12: /* NESTING_EVENT_ID */
        f->set_null();
        break;
      case 13: /* OPERATION */
        operation= &operation_names_map[(int) m_row.m_operation - 1];
        set_field_varchar_utf8(f, operation->str, operation->length);
        break;
      case 14: /* NUMBER_OF_BYTES */
        if ((m_row.m_operation == OPERATION_TYPE_FILEREAD) ||
            (m_row.m_operation == OPERATION_TYPE_FILEWRITE) ||
            (m_row.m_operation == OPERATION_TYPE_FILECHSIZE))
          set_field_ulonglong(f, m_row.m_number_of_bytes);
        else
          f->set_null();
        break;
      case 15: /* FLAGS */
        set_field_ulong(f, m_row.m_flags);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}

PFS_engine_table* table_events_waits_current::create(void)
{
  return new table_events_waits_current();
}

table_events_waits_current::table_events_waits_current()
  : table_events_waits_common(&m_share, &m_pos),
  m_pos(), m_next_pos()
{}

void table_events_waits_current::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_events_waits_current::rnd_next(void)
{
  PFS_thread *pfs_thread;
  PFS_events_waits *wait;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index_1 < thread_max;
       m_pos.next_thread())
  {
    pfs_thread= &thread_array[m_pos.m_index_1];

    if (! pfs_thread->m_lock.is_populated())
    {
      /* This thread does not exist */
      continue;
    }

    /*
      We do not show nested events for now,
      this will be revised with TABLE io
    */
#define ONLY_SHOW_ONE_WAIT

#ifdef ONLY_SHOW_ONE_WAIT
    if (m_pos.m_index_2 >= 1)
      continue;
#else
    if (m_pos.m_index_2 >= pfs_thread->m_wait_locker_count)
      continue;
#endif

    wait= &pfs_thread->m_wait_locker_stack[m_pos.m_index_2].m_waits_current;

    if (wait->m_wait_class == NO_WAIT_CLASS)
    {
      /*
        This locker does not exist.
        There can not be more lockers in the stack, skip to the next thread
      */
      continue;
    }

    make_row(true, pfs_thread, wait);
    /* Next iteration, look for the next locker in this thread */
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_waits_current::rnd_pos(const void *pos)
{
  PFS_thread *pfs_thread;
  PFS_events_waits *wait;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < thread_max);
  pfs_thread= &thread_array[m_pos.m_index_1];

  if (! pfs_thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

#ifdef ONLY_SHOW_CURRENT_WAITS
  if (m_pos.m_index_2 >= pfs_thread->m_wait_locker_count)
    return HA_ERR_RECORD_DELETED;
#endif

  DBUG_ASSERT(m_pos.m_index_2 < LOCKER_STACK_SIZE);

  wait= &pfs_thread->m_wait_locker_stack[m_pos.m_index_2].m_waits_current;

  if (wait->m_wait_class == NO_WAIT_CLASS)
    return HA_ERR_RECORD_DELETED;

  make_row(true, pfs_thread, wait);
  return 0;
}

int table_events_waits_current::delete_all_rows(void)
{
  reset_events_waits_current();
  return 0;
}

PFS_engine_table* table_events_waits_history::create(void)
{
  return new table_events_waits_history();
}

table_events_waits_history::table_events_waits_history()
  : table_events_waits_common(&m_share, &m_pos),
  m_pos(), m_next_pos()
{}

void table_events_waits_history::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_events_waits_history::rnd_next(void)
{
  PFS_thread *pfs_thread;
  PFS_events_waits *wait;

  if (events_waits_history_per_thread == 0)
    return HA_ERR_END_OF_FILE;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index_1 < thread_max;
       m_pos.next_thread())
  {
    pfs_thread= &thread_array[m_pos.m_index_1];

    if (! pfs_thread->m_lock.is_populated())
    {
      /* This thread does not exist */
      continue;
    }

    if (m_pos.m_index_2 >= events_waits_history_per_thread)
    {
      /* This thread does not have more (full) history */
      continue;
    }

    if ( ! pfs_thread->m_waits_history_full &&
        (m_pos.m_index_2 >= pfs_thread->m_waits_history_index))
    {
      /* This thread does not have more (not full) history */
      continue;
    }

    if (pfs_thread->m_waits_history[m_pos.m_index_2].m_wait_class
        == NO_WAIT_CLASS)
    {
      /*
        This locker does not exist.
        There can not be more lockers in the stack, skip to the next thread
      */
      continue;
    }

    wait= &pfs_thread->m_waits_history[m_pos.m_index_2];

    make_row(true, pfs_thread, wait);
    /* Next iteration, look for the next history in this thread */
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_waits_history::rnd_pos(const void *pos)
{
  PFS_thread *pfs_thread;
  PFS_events_waits *wait;

  DBUG_ASSERT(events_waits_history_per_thread != 0);
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < thread_max);
  pfs_thread= &thread_array[m_pos.m_index_1];

  if (! pfs_thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  DBUG_ASSERT(m_pos.m_index_2 < events_waits_history_per_thread);

  if ( ! pfs_thread->m_waits_history_full &&
      (m_pos.m_index_2 >= pfs_thread->m_waits_history_index))
    return HA_ERR_RECORD_DELETED;

  if (pfs_thread->m_waits_history[m_pos.m_index_2].m_wait_class
      == NO_WAIT_CLASS)
    return HA_ERR_RECORD_DELETED;

  wait= &pfs_thread->m_waits_history[m_pos.m_index_2];

  make_row(true, pfs_thread, wait);
  return 0;
}

int table_events_waits_history::delete_all_rows(void)
{
  reset_events_waits_history();
  return 0;
}

PFS_engine_table* table_events_waits_history_long::create(void)
{
  return new table_events_waits_history_long();
}

table_events_waits_history_long::table_events_waits_history_long()
  : table_events_waits_common(&m_share, &m_pos),
  m_pos(0), m_next_pos(0)
{}

void table_events_waits_history_long::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_events_waits_history_long::rnd_next(void)
{
  PFS_events_waits *wait;
  uint limit;

  if (events_waits_history_long_size == 0)
    return HA_ERR_END_OF_FILE;

  if (events_waits_history_long_full)
    limit= events_waits_history_long_size;
  else
    limit= events_waits_history_long_index % events_waits_history_long_size;

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < limit; m_pos.next())
  {
    wait= &events_waits_history_long_array[m_pos.m_index];

    if (wait->m_wait_class != NO_WAIT_CLASS)
    {
      make_row(false, wait->m_thread, wait);
      /* Next iteration, look for the next entry */
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_waits_history_long::rnd_pos(const void *pos)
{
  PFS_events_waits *wait;
  uint limit;

  if (events_waits_history_long_size == 0)
    return HA_ERR_RECORD_DELETED;

  set_position(pos);

  if (events_waits_history_long_full)
    limit= events_waits_history_long_size;
  else
    limit= events_waits_history_long_index % events_waits_history_long_size;

  if (m_pos.m_index >= limit)
    return HA_ERR_RECORD_DELETED;

  wait= &events_waits_history_long_array[m_pos.m_index];

  if (wait->m_wait_class == NO_WAIT_CLASS)
    return HA_ERR_RECORD_DELETED;

  make_row(false, wait->m_thread, wait);
  return 0;
}

int table_events_waits_history_long::delete_all_rows(void)
{
  reset_events_waits_history_long();
  return 0;
}

