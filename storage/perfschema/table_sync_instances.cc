/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_sync_instances.cc
  Table MUTEX_INSTANCES, RWLOCK_INSTANCES
  and COND_INSTANCES (implementation).
*/

#include "storage/perfschema/table_sync_instances.h"

#include <stddef.h>

#include "field.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "pfs_buffer_container.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_instr.h"

THR_LOCK table_mutex_instances::m_table_lock;

/* clang-format off */
static const TABLE_FIELD_TYPE mutex_field_types[]=
{
  {
    { C_STRING_WITH_LEN("NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_INSTANCE_BEGIN") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LOCKED_BY_THREAD_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_mutex_instances::m_field_def = {3, mutex_field_types};

PFS_engine_table_share table_mutex_instances::m_share = {
  {C_STRING_WITH_LEN("mutex_instances")},
  &pfs_readonly_acl,
  table_mutex_instances::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_mutex_instances::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

bool
PFS_index_mutex_instances_by_instance::match(PFS_mutex *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key.match(pfs))
    {
      return false;
    }
  }
  return true;
}

bool
PFS_index_mutex_instances_by_name::match(PFS_mutex *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key.match(pfs))
    {
      return false;
    }
  }
  return true;
}

bool
PFS_index_mutex_instances_by_thread_id::match(PFS_mutex *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key.match_owner(pfs))
    {
      return false;
    }
  }
  return true;
}

PFS_engine_table *
table_mutex_instances::create(void)
{
  return new table_mutex_instances();
}

ha_rows
table_mutex_instances::get_row_count(void)
{
  return global_mutex_container.get_row_count();
}

table_mutex_instances::table_mutex_instances()
  : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0)
{
}

void
table_mutex_instances::reset_position(void)
{
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int
table_mutex_instances::rnd_next(void)
{
  PFS_mutex *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_mutex_iterator it = global_mutex_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != NULL)
  {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int
table_mutex_instances::rnd_pos(const void *pos)
{
  PFS_mutex *pfs;

  set_position(pos);

  pfs = global_mutex_container.get(m_pos.m_index);
  if (pfs != NULL)
  {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_mutex_instances::index_init(uint idx, bool)
{
  PFS_index_mutex_instances *result = NULL;

  switch (idx)
  {
  case 0:
    result = PFS_NEW(PFS_index_mutex_instances_by_instance);
    break;
  case 1:
    result = PFS_NEW(PFS_index_mutex_instances_by_name);
    break;
  case 2:
    result = PFS_NEW(PFS_index_mutex_instances_by_thread_id);
    break;
  default:
    DBUG_ASSERT(false);
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_mutex_instances::index_next(void)
{
  PFS_mutex *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_mutex_iterator it = global_mutex_container.iterate(m_pos.m_index);

  do
  {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != NULL)
    {
      if (m_opened_index->match(pfs))
      {
        if (!make_row(pfs))
        {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  } while (pfs != NULL);

  return HA_ERR_END_OF_FILE;
}

int
table_mutex_instances::make_row(PFS_mutex *pfs)
{
  pfs_optimistic_state lock;
  PFS_mutex_class *safe_class;

  /* Protect this reader against a mutex destroy */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_mutex_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
  {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_name = safe_class->m_name;
  m_row.m_name_length = safe_class->m_name_length;
  m_row.m_identity = pfs->m_identity;

  /* Protect this reader against a mutex unlock */
  PFS_thread *safe_owner = sanitize_thread(pfs->m_owner);
  if (safe_owner)
  {
    m_row.m_locked_by_thread_id = safe_owner->m_thread_internal_id;
    m_row.m_locked = true;
  }
  else
  {
    m_row.m_locked = false;
  }

  if (!pfs->m_lock.end_optimistic_lock(&lock))
  {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int
table_mutex_instances::read_row_values(TABLE *table,
                                       unsigned char *buf,
                                       Field **fields,
                                       bool read_all)
{
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 1: /* OBJECT_INSTANCE */
        set_field_ulonglong(f, (intptr)m_row.m_identity);
        break;
      case 2: /* LOCKED_BY_THREAD_ID */
        if (m_row.m_locked)
        {
          set_field_ulonglong(f, m_row.m_locked_by_thread_id);
        }
        else
        {
          f->set_null();
        }
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

THR_LOCK table_rwlock_instances::m_table_lock;

static const TABLE_FIELD_TYPE rwlock_field_types[] = {
  {{C_STRING_WITH_LEN("NAME")}, {C_STRING_WITH_LEN("varchar(128)")}, {NULL, 0}},
  {{C_STRING_WITH_LEN("OBJECT_INSTANCE_BEGIN")},
   {C_STRING_WITH_LEN("bigint(20)")},
   {NULL, 0}},
  {{C_STRING_WITH_LEN("WRITE_LOCKED_BY_THREAD_ID")},
   {C_STRING_WITH_LEN("bigint(20)")},
   {NULL, 0}},
  {{C_STRING_WITH_LEN("READ_LOCKED_BY_COUNT")},
   {C_STRING_WITH_LEN("int(10)")},
   {NULL, 0}}};

TABLE_FIELD_DEF
table_rwlock_instances::m_field_def = {4, rwlock_field_types};

PFS_engine_table_share table_rwlock_instances::m_share = {
  {C_STRING_WITH_LEN("rwlock_instances")},
  &pfs_readonly_acl,
  table_rwlock_instances::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_rwlock_instances::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

bool
PFS_index_rwlock_instances_by_instance::match(PFS_rwlock *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key.match(pfs))
    {
      return false;
    }
  }
  return true;
}

bool
PFS_index_rwlock_instances_by_name::match(PFS_rwlock *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key.match(pfs))
    {
      return false;
    }
  }
  return true;
}

bool
PFS_index_rwlock_instances_by_thread_id::match(PFS_rwlock *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key.match_writer(pfs))
    {
      return false;
    }
  }
  return true;
}

PFS_engine_table *
table_rwlock_instances::create(void)
{
  return new table_rwlock_instances();
}

ha_rows
table_rwlock_instances::get_row_count(void)
{
  return global_rwlock_container.get_row_count();
}

table_rwlock_instances::table_rwlock_instances()
  : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0)
{
}

void
table_rwlock_instances::reset_position(void)
{
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int
table_rwlock_instances::rnd_next(void)
{
  PFS_rwlock *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_rwlock_iterator it = global_rwlock_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != NULL)
  {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int
table_rwlock_instances::rnd_pos(const void *pos)
{
  PFS_rwlock *pfs;

  set_position(pos);

  pfs = global_rwlock_container.get(m_pos.m_index);
  if (pfs != NULL)
  {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_rwlock_instances::index_init(uint idx, bool)
{
  PFS_index_rwlock_instances *result = NULL;

  switch (idx)
  {
  case 0:
    result = PFS_NEW(PFS_index_rwlock_instances_by_instance);
    break;
  case 1:
    result = PFS_NEW(PFS_index_rwlock_instances_by_name);
    break;
  case 2:
    result = PFS_NEW(PFS_index_rwlock_instances_by_thread_id);
    break;
  default:
    DBUG_ASSERT(false);
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_rwlock_instances::index_next(void)
{
  PFS_rwlock *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_rwlock_iterator it = global_rwlock_container.iterate(m_pos.m_index);

  do
  {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != NULL)
    {
      if (m_opened_index->match(pfs))
      {
        if (!make_row(pfs))
        {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      m_pos.m_index++;
    }
  } while (pfs != NULL);

  return HA_ERR_END_OF_FILE;
}

int
table_rwlock_instances::make_row(PFS_rwlock *pfs)
{
  pfs_optimistic_state lock;
  PFS_rwlock_class *safe_class;

  /* Protect this reader against a rwlock destroy */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_rwlock_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
  {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_name = safe_class->m_name;
  m_row.m_name_length = safe_class->m_name_length;
  m_row.m_identity = pfs->m_identity;

  /* Protect this reader against a rwlock unlock in the writer */
  PFS_thread *safe_writer = sanitize_thread(pfs->m_writer);
  if (safe_writer)
  {
    m_row.m_write_locked_by_thread_id = safe_writer->m_thread_internal_id;
    m_row.m_readers = 0;
    m_row.m_write_locked = true;
  }
  else
  {
    m_row.m_readers = pfs->m_readers;
    m_row.m_write_locked = false;
  }

  if (!pfs->m_lock.end_optimistic_lock(&lock))
  {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int
table_rwlock_instances::read_row_values(TABLE *table,
                                        unsigned char *buf,
                                        Field **fields,
                                        bool read_all)
{
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 1: /* OBJECT_INSTANCE */
        set_field_ulonglong(f, (intptr)m_row.m_identity);
        break;
      case 2: /* WRITE_LOCKED_BY_THREAD_ID */
        if (m_row.m_write_locked)
        {
          set_field_ulonglong(f, m_row.m_write_locked_by_thread_id);
        }
        else
        {
          f->set_null();
        }
        break;
      case 3: /* READ_LOCKED_BY_COUNT */
        set_field_ulong(f, m_row.m_readers);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

THR_LOCK table_cond_instances::m_table_lock;

static const TABLE_FIELD_TYPE cond_field_types[] = {
  {{C_STRING_WITH_LEN("NAME")}, {C_STRING_WITH_LEN("varchar(128)")}, {NULL, 0}},
  {{C_STRING_WITH_LEN("OBJECT_INSTANCE_BEGIN")},
   {C_STRING_WITH_LEN("bigint(20)")},
   {NULL, 0}}};

TABLE_FIELD_DEF
table_cond_instances::m_field_def = {2, cond_field_types};

PFS_engine_table_share table_cond_instances::m_share = {
  {C_STRING_WITH_LEN("cond_instances")},
  &pfs_readonly_acl,
  table_cond_instances::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_cond_instances::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

bool
PFS_index_cond_instances_by_instance::match(PFS_cond *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key.match(pfs))
    {
      return false;
    }
  }
  return true;
}

bool
PFS_index_cond_instances_by_name::match(PFS_cond *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key.match(pfs))
    {
      return false;
    }
  }
  return true;
}

PFS_engine_table *
table_cond_instances::create(void)
{
  return new table_cond_instances();
}

ha_rows
table_cond_instances::get_row_count(void)
{
  return global_cond_container.get_row_count();
}

table_cond_instances::table_cond_instances()
  : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0)
{
}

void
table_cond_instances::reset_position(void)
{
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int
table_cond_instances::rnd_next(void)
{
  PFS_cond *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_cond_iterator it = global_cond_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != NULL)
  {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int
table_cond_instances::rnd_pos(const void *pos)
{
  PFS_cond *pfs;

  set_position(pos);

  pfs = global_cond_container.get(m_pos.m_index);
  if (pfs != NULL)
  {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_cond_instances::index_init(uint idx, bool)
{
  PFS_index_cond_instances *result = NULL;

  switch (idx)
  {
  case 0:
    result = PFS_NEW(PFS_index_cond_instances_by_instance);
    break;
  case 1:
    result = PFS_NEW(PFS_index_cond_instances_by_name);
    break;
  default:
    DBUG_ASSERT(false);
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_cond_instances::index_next(void)
{
  PFS_cond *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_cond_iterator it = global_cond_container.iterate(m_pos.m_index);

  do
  {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != NULL)
    {
      if (m_opened_index->match(pfs))
      {
        if (!make_row(pfs))
        {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      m_pos.m_index++;
    }
  } while (pfs != NULL);

  return HA_ERR_END_OF_FILE;
}

int
table_cond_instances::make_row(PFS_cond *pfs)
{
  pfs_optimistic_state lock;
  PFS_cond_class *safe_class;

  /* Protect this reader against a cond destroy */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_cond_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
  {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_name = safe_class->m_name;
  m_row.m_name_length = safe_class->m_name_length;
  m_row.m_identity = pfs->m_identity;

  if (!pfs->m_lock.end_optimistic_lock(&lock))
  {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int
table_cond_instances::read_row_values(TABLE *table,
                                      unsigned char *,
                                      Field **fields,
                                      bool read_all)
{
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 1: /* OBJECT_INSTANCE */
        set_field_ulonglong(f, (intptr)m_row.m_identity);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
