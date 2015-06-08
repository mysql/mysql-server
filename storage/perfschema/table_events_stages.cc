/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "my_thread.h"
#include "table_events_stages.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_events_stages.h"
#include "pfs_timer.h"
#include "pfs_buffer_container.h"
#include "field.h"

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
    { C_STRING_WITH_LEN("WORK_COMPLETED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("WORK_ESTIMATED") },
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
    { C_STRING_WITH_LEN("enum(\'TRANSACTION\',\'STATEMENT\',\'STAGE\',\'WAIT\'") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_events_stages_current::m_field_def=
{12 , field_types };

PFS_engine_table_share
table_events_stages_current::m_share=
{
  { C_STRING_WITH_LEN("events_stages_current") },
  &pfs_truncatable_acl,
  table_events_stages_current::create,
  NULL, /* write_row */
  table_events_stages_current::delete_all_rows,
  table_events_stages_current::get_row_count,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

THR_LOCK table_events_stages_history::m_table_lock;

PFS_engine_table_share
table_events_stages_history::m_share=
{
  { C_STRING_WITH_LEN("events_stages_history") },
  &pfs_truncatable_acl,
  table_events_stages_history::create,
  NULL, /* write_row */
  table_events_stages_history::delete_all_rows,
  table_events_stages_history::get_row_count,
  sizeof(pos_events_stages_history), /* ref length */
  &m_table_lock,
  &table_events_stages_current::m_field_def,
  false, /* checked */
  false  /* perpetual */
};

THR_LOCK table_events_stages_history_long::m_table_lock;

PFS_engine_table_share
table_events_stages_history_long::m_share=
{
  { C_STRING_WITH_LEN("events_stages_history_long") },
  &pfs_truncatable_acl,
  table_events_stages_history_long::create,
  NULL, /* write_row */
  table_events_stages_history_long::delete_all_rows,
  table_events_stages_history_long::get_row_count,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &table_events_stages_current::m_field_def,
  false, /* checked */
  false  /* perpetual */
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
  ulonglong timer_end;

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

  if (m_row.m_end_event_id == 0)
  {
    timer_end= get_timer_raw_value(stage_timer);
  }
  else
  {
    timer_end= stage->m_timer_end;
  }

  m_normalizer->to_pico(stage->m_timer_start, timer_end,
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

  if (klass->is_progress())
  {
    m_row.m_progress= true;
    m_row.m_work_completed= stage->m_progress.m_work_completed;
    m_row.m_work_estimated= stage->m_progress.m_work_estimated;
  }
  else
  {
    m_row.m_progress= false;
  }

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
  DBUG_ASSERT(table->s->null_bytes == 2);
  buf[0]= 0;
  buf[1]= 0;

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
      case 8: /* WORK_COMPLETED */
        if (m_row.m_progress)
          set_field_ulonglong(f, m_row.m_work_completed);
        else
          f->set_null();
        break;
      case 9: /* WORK_ESTIMATED */
        if (m_row.m_progress)
          set_field_ulonglong(f, m_row.m_work_estimated);
        else
          f->set_null();
        break;
      case 10: /* NESTING_EVENT_ID */
        if (m_row.m_nesting_event_id != 0)
          set_field_ulonglong(f, m_row.m_nesting_event_id);
        else
          f->set_null();
        break;
      case 11: /* NESTING_EVENT_TYPE */
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

  m_pos.set_at(&m_next_pos);
  PFS_thread_iterator it= global_thread_container.iterate(m_pos.m_index);
  pfs_thread= it.scan_next(& m_pos.m_index);
  if (pfs_thread != NULL)
  {
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

  pfs_thread= global_thread_container.get(m_pos.m_index);
  if (pfs_thread != NULL)
  {
    stage= &pfs_thread->m_stage_current;
    make_row(stage);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

int table_events_stages_current::delete_all_rows(void)
{
  reset_events_stages_current();
  return 0;
}

ha_rows
table_events_stages_current::get_row_count(void)
{
  return global_thread_container.get_row_count();
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
  bool has_more_thread= true;

  if (events_stages_history_per_thread == 0)
    return HA_ERR_END_OF_FILE;

  for (m_pos.set_at(&m_next_pos);
       has_more_thread;
       m_pos.next_thread())
  {
    pfs_thread= global_thread_container.get(m_pos.m_index_1, & has_more_thread);
    if (pfs_thread != NULL)
    {
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
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_stages_history::rnd_pos(const void *pos)
{
  PFS_thread *pfs_thread;
  PFS_events_stages *stage;

  DBUG_ASSERT(events_stages_history_per_thread != 0);
  set_position(pos);

  DBUG_ASSERT(m_pos.m_index_2 < events_stages_history_per_thread);

  pfs_thread= global_thread_container.get(m_pos.m_index_1);
  if (pfs_thread != NULL)
  {
    if ( ! pfs_thread->m_stages_history_full &&
        (m_pos.m_index_2 >= pfs_thread->m_stages_history_index))
      return HA_ERR_RECORD_DELETED;

    stage= &pfs_thread->m_stages_history[m_pos.m_index_2];

    if (stage->m_class != NULL)
    {
      make_row(stage);
      return 0;
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_events_stages_history::delete_all_rows(void)
{
  reset_events_stages_history();
  return 0;
}

ha_rows
table_events_stages_history::get_row_count(void)
{
  return events_stages_history_per_thread * global_thread_container.get_row_count();
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
    limit= events_stages_history_long_index.m_u32 % events_stages_history_long_size;

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
    limit= events_stages_history_long_index.m_u32 % events_stages_history_long_size;

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

ha_rows
table_events_stages_history_long::get_row_count(void)
{
  return events_stages_history_long_size;
}

