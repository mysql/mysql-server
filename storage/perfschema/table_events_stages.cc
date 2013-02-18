/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_events_stages.cc
  Table EVENTS_STAGES_xxx (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "table_events_stages.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_events_stages.h"
#include "pfs_timer.h"

THR_LOCK table_events_stages_current::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("THREAD_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("END_EVENT_ID") },
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
    { C_STRING_WITH_LEN("NESTING_EVENT_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("NESTING_EVENT_TYPE") },
    { C_STRING_WITH_LEN("enum(\'STATEMENT\',\'STAGE\',\'WAIT\'") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_events_stages_current::m_field_def=
{10 , field_types };

PFS_engine_table_share
table_events_stages_current::m_share=
{
  { C_STRING_WITH_LEN("events_stages_current") },
  &pfs_truncatable_acl,
  &table_events_stages_current::create,
  NULL, /* write_row */
  &table_events_stages_current::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

THR_LOCK table_events_stages_history::m_table_lock;

PFS_engine_table_share
table_events_stages_history::m_share=
{
  { C_STRING_WITH_LEN("events_stages_history") },
  &pfs_truncatable_acl,
  &table_events_stages_history::create,
  NULL, /* write_row */
  &table_events_stages_history::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(pos_events_stages_history), /* ref length */
  &m_table_lock,
  &table_events_stages_current::m_field_def,
  false /* checked */
};

THR_LOCK table_events_stages_history_long::m_table_lock;

PFS_engine_table_share
table_events_stages_history_long::m_share=
{
  { C_STRING_WITH_LEN("events_stages_history_long") },
  &pfs_truncatable_acl,
  &table_events_stages_history_long::create,
  NULL, /* write_row */
  &table_events_stages_history_long::delete_all_rows,
  NULL, /* get_row_count */
  10000, /* records */
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &table_events_stages_current::m_field_def,
  false /* checked */
};

table_events_stages_common::table_events_stages_common
(const PFS_engine_table_share *share, void *pos)
  : PFS_engine_table(share, pos),
  m_row_exists(false)
{}

/**
  Build a row.
  @param stage                      the stage the cursor is reading
*/
void table_events_stages_common::make_row(PFS_events_stages *stage)
{
  const char *base;
  const char *safe_source_file;

  m_row_exists= false;

  PFS_stage_class *unsafe= (PFS_stage_class*) stage->m_class;
  PFS_stage_class *klass= sanitize_stage_class(unsafe);
  if (unlikely(klass == NULL))
    return;

  m_row.m_thread_internal_id= stage->m_thread_internal_id;
  m_row.m_event_id= stage->m_event_id;
  m_row.m_end_event_id= stage->m_end_event_id;
  m_row.m_nesting_event_id= stage->m_nesting_event_id;
  m_row.m_nesting_event_type= stage->m_nesting_event_type;

  m_normalizer->to_pico(stage->m_timer_start, stage->m_timer_end,
                      & m_row.m_timer_start, & m_row.m_timer_end, & m_row.m_timer_wait);

  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;

  safe_source_file= stage->m_source_file;
  if (unlikely(safe_source_file == NULL))
    return;

  base= base_name(safe_source_file);
  m_row.m_source_length= my_snprintf(m_row.m_source, sizeof(m_row.m_source),
                                     "%s:%d", base, stage->m_source_line);
  if (m_row.m_source_length > sizeof(m_row.m_source))
    m_row.m_source_length= sizeof(m_row.m_source);

  m_row_exists= true;
  return;
}

int table_events_stages_common::read_row_values(TABLE *table,
                                               unsigned char *buf,
                                               Field **fields,
                                               bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* THREAD_ID */
        set_field_ulonglong(f, m_row.m_thread_internal_id);
        break;
      case 1: /* EVENT_ID */
        set_field_ulonglong(f, m_row.m_event_id);
        break;
      case 2: /* END_EVENT_ID */
        if (m_row.m_end_event_id > 0)
          set_field_ulonglong(f, m_row.m_end_event_id - 1);
        else
          f->set_null();
        break;
      case 3: /* EVENT_NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 4: /* SOURCE */
        set_field_varchar_utf8(f, m_row.m_source, m_row.m_source_length);
        break;
      case 5: /* TIMER_START */
        if (m_row.m_timer_start != 0)
          set_field_ulonglong(f, m_row.m_timer_start);
        else
          f->set_null();
        break;
      case 6: /* TIMER_END */
        if (m_row.m_timer_end != 0)
          set_field_ulonglong(f, m_row.m_timer_end);
        else
          f->set_null();
        break;
      case 7: /* TIMER_WAIT */
        if (m_row.m_timer_wait != 0)
          set_field_ulonglong(f, m_row.m_timer_wait);
        else
          f->set_null();
        break;
      case 8: /* NESTING_EVENT_ID */
        if (m_row.m_nesting_event_id != 0)
          set_field_ulonglong(f, m_row.m_nesting_event_id);
        else
          f->set_null();
        break;
      case 9: /* NESTING_EVENT_TYPE */
        if (m_row.m_nesting_event_id != 0)
          set_field_enum(f, m_row.m_nesting_event_type);
        else
          f->set_null();
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}

PFS_engine_table* table_events_stages_current::create(void)
{
  return new table_events_stages_current();
}

table_events_stages_current::table_events_stages_current()
  : table_events_stages_common(&m_share, &m_pos),
  m_pos(0), m_next_pos(0)
{}

void table_events_stages_current::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_events_stages_current::rnd_init(bool scan)
{
  m_normalizer= time_normalizer::get(stage_timer);
  return 0;
}

int table_events_stages_current::rnd_next(void)
{
  PFS_thread *pfs_thread;
  PFS_events_stages *stage;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < thread_max;
       m_pos.next())
  {
    pfs_thread= &thread_array[m_pos.m_index];

    if (! pfs_thread->m_lock.is_populated())
    {
      /* This thread does not exist */
      continue;
    }

    stage= &pfs_thread->m_stage_current;

    make_row(stage);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_stages_current::rnd_pos(const void *pos)
{
  PFS_thread *pfs_thread;
  PFS_events_stages *stage;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < thread_max);
  pfs_thread= &thread_array[m_pos.m_index];

  if (! pfs_thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  stage= &pfs_thread->m_stage_current;
  make_row(stage);
  return 0;
}

int table_events_stages_current::delete_all_rows(void)
{
  reset_events_stages_current();
  return 0;
}

PFS_engine_table* table_events_stages_history::create(void)
{
  return new table_events_stages_history();
}

table_events_stages_history::table_events_stages_history()
  : table_events_stages_common(&m_share, &m_pos),
  m_pos(), m_next_pos()
{}

void table_events_stages_history::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_events_stages_history::rnd_init(bool scan)
{
  m_normalizer= time_normalizer::get(stage_timer);
  return 0;
}

int table_events_stages_history::rnd_next(void)
{
  PFS_thread *pfs_thread;
  PFS_events_stages *stage;

  if (events_stages_history_per_thread == 0)
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

    if (m_pos.m_index_2 >= events_stages_history_per_thread)
    {
      /* This thread does not have more (full) history */
      continue;
    }

    if ( ! pfs_thread->m_stages_history_full &&
        (m_pos.m_index_2 >= pfs_thread->m_stages_history_index))
    {
      /* This thread does not have more (not full) history */
      continue;
    }

    stage= &pfs_thread->m_stages_history[m_pos.m_index_2];

    if (stage->m_class != NULL)
    {
      make_row(stage);
      /* Next iteration, look for the next history in this thread */
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_stages_history::rnd_pos(const void *pos)
{
  PFS_thread *pfs_thread;
  PFS_events_stages *stage;

  DBUG_ASSERT(events_stages_history_per_thread != 0);
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < thread_max);
  pfs_thread= &thread_array[m_pos.m_index_1];

  if (! pfs_thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  DBUG_ASSERT(m_pos.m_index_2 < events_stages_history_per_thread);

  if ( ! pfs_thread->m_stages_history_full &&
      (m_pos.m_index_2 >= pfs_thread->m_stages_history_index))
    return HA_ERR_RECORD_DELETED;

  stage= &pfs_thread->m_stages_history[m_pos.m_index_2];

  if (stage->m_class == NULL)
    return HA_ERR_RECORD_DELETED;

  make_row(stage);
  return 0;
}

int table_events_stages_history::delete_all_rows(void)
{
  reset_events_stages_history();
  return 0;
}

PFS_engine_table* table_events_stages_history_long::create(void)
{
  return new table_events_stages_history_long();
}

table_events_stages_history_long::table_events_stages_history_long()
  : table_events_stages_common(&m_share, &m_pos),
  m_pos(0), m_next_pos(0)
{}

void table_events_stages_history_long::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_events_stages_history_long::rnd_init(bool scan)
{
  m_normalizer= time_normalizer::get(stage_timer);
  return 0;
}

int table_events_stages_history_long::rnd_next(void)
{
  PFS_events_stages *stage;
  uint limit;

  if (events_stages_history_long_size == 0)
    return HA_ERR_END_OF_FILE;

  if (events_stages_history_long_full)
    limit= events_stages_history_long_size;
  else
    limit= events_stages_history_long_index % events_stages_history_long_size;

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < limit; m_pos.next())
  {
    stage= &events_stages_history_long_array[m_pos.m_index];

    if (stage->m_class != NULL)
    {
      make_row(stage);
      /* Next iteration, look for the next entry */
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_stages_history_long::rnd_pos(const void *pos)
{
  PFS_events_stages *stage;
  uint limit;

  if (events_stages_history_long_size == 0)
    return HA_ERR_RECORD_DELETED;

  set_position(pos);

  if (events_stages_history_long_full)
    limit= events_stages_history_long_size;
  else
    limit= events_stages_history_long_index % events_stages_history_long_size;

  if (m_pos.m_index > limit)
    return HA_ERR_RECORD_DELETED;

  stage= &events_stages_history_long_array[m_pos.m_index];

  if (stage->m_class == NULL)
    return HA_ERR_RECORD_DELETED;

  make_row(stage);
  return 0;
}

int table_events_stages_history_long::delete_all_rows(void)
{
  reset_events_stages_history_long();
  return 0;
}

