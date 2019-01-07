/* Copyright (c) 2008, 2019, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/perfschema/table_sync_instances.cc
  Table MUTEX_INSTANCES, RWLOCK_INSTANCES
  and COND_INSTANCES (implementation).
*/

#include "storage/perfschema/table_sync_instances.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"

THR_LOCK table_mutex_instances::m_table_lock;

Plugin_table table_mutex_instances::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "mutex_instances",
    /* Definition */
    "  NAME VARCHAR(128) not null,\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  LOCKED_BY_THREAD_ID BIGINT unsigned,\n"
    "  PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,\n"
    "  KEY (NAME) USING HASH,\n"
    "  KEY (LOCKED_BY_THREAD_ID) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_mutex_instances::m_share = {
    &pfs_readonly_acl,
    table_mutex_instances::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    table_mutex_instances::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_mutex_instances_by_instance::match(PFS_mutex *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_mutex_instances_by_name::match(PFS_mutex *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_mutex_instances_by_thread_id::match(PFS_mutex *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match_owner(pfs)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_mutex_instances::create(PFS_engine_table_share *) {
  return new table_mutex_instances();
}

ha_rows table_mutex_instances::get_row_count(void) {
  return global_mutex_container.get_row_count();
}

table_mutex_instances::table_mutex_instances()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

void table_mutex_instances::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_mutex_instances::rnd_next(void) {
  PFS_mutex *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_mutex_iterator it = global_mutex_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != NULL) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_mutex_instances::rnd_pos(const void *pos) {
  PFS_mutex *pfs;

  set_position(pos);

  pfs = global_mutex_container.get(m_pos.m_index);
  if (pfs != NULL) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_mutex_instances::index_init(uint idx, bool) {
  PFS_index_mutex_instances *result = NULL;

  switch (idx) {
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

int table_mutex_instances::index_next(void) {
  PFS_mutex *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_mutex_iterator it = global_mutex_container.iterate(m_pos.m_index);

  do {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != NULL) {
      if (m_opened_index->match(pfs)) {
        if (!make_row(pfs)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  } while (pfs != NULL);

  return HA_ERR_END_OF_FILE;
}

int table_mutex_instances::make_row(PFS_mutex *pfs) {
  pfs_optimistic_state lock;
  PFS_mutex_class *safe_class;

  /* Protect this reader against a mutex destroy */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_mutex_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_name = safe_class->m_name;
  m_row.m_name_length = safe_class->m_name_length;
  m_row.m_identity = pfs->m_identity;

  /* Protect this reader against a mutex unlock */
  PFS_thread *safe_owner = sanitize_thread(pfs->m_owner);
  if (safe_owner) {
    m_row.m_locked_by_thread_id = safe_owner->m_thread_internal_id;
    m_row.m_locked = true;
  } else {
    m_row.m_locked = false;
  }

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_mutex_instances::read_row_values(TABLE *table, unsigned char *buf,
                                           Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* NAME */
          set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
          break;
        case 1: /* OBJECT_INSTANCE */
          set_field_ulonglong(f, (intptr)m_row.m_identity);
          break;
        case 2: /* LOCKED_BY_THREAD_ID */
          if (m_row.m_locked) {
            set_field_ulonglong(f, m_row.m_locked_by_thread_id);
          } else {
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

Plugin_table table_rwlock_instances::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "rwlock_instances",
    /* Definition */
    "  NAME VARCHAR(128) not null,\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  WRITE_LOCKED_BY_THREAD_ID BIGINT unsigned,\n"
    "  READ_LOCKED_BY_COUNT INTEGER unsigned not null,\n"
    "  PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,\n"
    "  KEY (NAME) USING HASH,\n"
    "  KEY (WRITE_LOCKED_BY_THREAD_ID) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_rwlock_instances::m_share = {
    &pfs_readonly_acl,
    table_rwlock_instances::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    table_rwlock_instances::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_rwlock_instances_by_instance::match(PFS_rwlock *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_rwlock_instances_by_name::match(PFS_rwlock *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_rwlock_instances_by_thread_id::match(PFS_rwlock *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match_writer(pfs)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_rwlock_instances::create(PFS_engine_table_share *) {
  return new table_rwlock_instances();
}

ha_rows table_rwlock_instances::get_row_count(void) {
  return global_rwlock_container.get_row_count();
}

table_rwlock_instances::table_rwlock_instances()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

void table_rwlock_instances::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_rwlock_instances::rnd_next(void) {
  PFS_rwlock *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_rwlock_iterator it = global_rwlock_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != NULL) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_rwlock_instances::rnd_pos(const void *pos) {
  PFS_rwlock *pfs;

  set_position(pos);

  pfs = global_rwlock_container.get(m_pos.m_index);
  if (pfs != NULL) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_rwlock_instances::index_init(uint idx, bool) {
  PFS_index_rwlock_instances *result = NULL;

  switch (idx) {
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

int table_rwlock_instances::index_next(void) {
  PFS_rwlock *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_rwlock_iterator it = global_rwlock_container.iterate(m_pos.m_index);

  do {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != NULL) {
      if (m_opened_index->match(pfs)) {
        if (!make_row(pfs)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      m_pos.m_index++;
    }
  } while (pfs != NULL);

  return HA_ERR_END_OF_FILE;
}

int table_rwlock_instances::make_row(PFS_rwlock *pfs) {
  pfs_optimistic_state lock;
  PFS_rwlock_class *safe_class;

  /* Protect this reader against a rwlock destroy */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_rwlock_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_name = safe_class->m_name;
  m_row.m_name_length = safe_class->m_name_length;
  m_row.m_identity = pfs->m_identity;

  /* Protect this reader against a rwlock unlock in the writer */
  PFS_thread *safe_writer = sanitize_thread(pfs->m_writer);
  if (safe_writer) {
    m_row.m_write_locked_by_thread_id = safe_writer->m_thread_internal_id;
    m_row.m_readers = 0;
    m_row.m_write_locked = true;
  } else {
    m_row.m_readers = pfs->m_readers;
    m_row.m_write_locked = false;
  }

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_rwlock_instances::read_row_values(TABLE *table, unsigned char *buf,
                                            Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* NAME */
          set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
          break;
        case 1: /* OBJECT_INSTANCE */
          set_field_ulonglong(f, (intptr)m_row.m_identity);
          break;
        case 2: /* WRITE_LOCKED_BY_THREAD_ID */
          if (m_row.m_write_locked) {
            set_field_ulonglong(f, m_row.m_write_locked_by_thread_id);
          } else {
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

Plugin_table table_cond_instances::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "cond_instances",
    /* Definition */
    "  NAME VARCHAR(128) not null,\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,\n"
    "  KEY (NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_cond_instances::m_share = {
    &pfs_readonly_acl,
    table_cond_instances::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    table_cond_instances::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_cond_instances_by_instance::match(PFS_cond *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_cond_instances_by_name::match(PFS_cond *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_cond_instances::create(PFS_engine_table_share *) {
  return new table_cond_instances();
}

ha_rows table_cond_instances::get_row_count(void) {
  return global_cond_container.get_row_count();
}

table_cond_instances::table_cond_instances()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

void table_cond_instances::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_cond_instances::rnd_next(void) {
  PFS_cond *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_cond_iterator it = global_cond_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != NULL) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_cond_instances::rnd_pos(const void *pos) {
  PFS_cond *pfs;

  set_position(pos);

  pfs = global_cond_container.get(m_pos.m_index);
  if (pfs != NULL) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_cond_instances::index_init(uint idx, bool) {
  PFS_index_cond_instances *result = NULL;

  switch (idx) {
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

int table_cond_instances::index_next(void) {
  PFS_cond *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_cond_iterator it = global_cond_container.iterate(m_pos.m_index);

  do {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != NULL) {
      if (m_opened_index->match(pfs)) {
        if (!make_row(pfs)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      m_pos.m_index++;
    }
  } while (pfs != NULL);

  return HA_ERR_END_OF_FILE;
}

int table_cond_instances::make_row(PFS_cond *pfs) {
  pfs_optimistic_state lock;
  PFS_cond_class *safe_class;

  /* Protect this reader against a cond destroy */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_cond_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_name = safe_class->m_name;
  m_row.m_name_length = safe_class->m_name_length;
  m_row.m_identity = pfs->m_identity;

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_cond_instances::read_row_values(TABLE *table, unsigned char *,
                                          Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
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
