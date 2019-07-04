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
  @file storage/perfschema/table_events_waits_summary.cc
  Table EVENTS_WAITS_SUMMARY_BY_xxx (implementation).
*/

#include "storage/perfschema/table_events_waits_summary.h"

#include <stddef.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_events_waits_summary_by_instance::m_table_lock;

Plugin_table table_events_waits_summary_by_instance::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_waits_summary_by_instance",
    /* Definition */
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  COUNT_STAR BIGINT unsigned not null,\n"
    "  SUM_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MIN_TIMER_WAIT BIGINT unsigned not null,\n"
    "  AVG_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MAX_TIMER_WAIT BIGINT unsigned not null,\n"
    "  PRIMARY KEY (OBJECT_INSTANCE_BEGIN),\n"
    "  KEY (EVENT_NAME)\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_events_waits_summary_by_instance::m_share = {
    &pfs_truncatable_acl,
    table_events_waits_summary_by_instance::create,
    NULL, /* write_row */
    table_events_waits_summary_by_instance::delete_all_rows,
    table_all_instr::get_row_count,
    sizeof(pos_all_instr),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_events_waits_summary_by_instance::match(PFS_mutex *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_events_waits_summary_by_instance::match(PFS_rwlock *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_events_waits_summary_by_instance::match(PFS_cond *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_events_waits_summary_by_instance::match(PFS_file *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_events_waits_summary_by_instance::match(PFS_socket *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_events_waits_summary_by_event_name::match(PFS_mutex *pfs) {
  if (m_fields >= 1) {
    PFS_mutex_class *safe_class;
    safe_class = sanitize_mutex_class(pfs->m_class);
    if (unlikely(safe_class == NULL)) {
      return false;
    }
    return m_key.match(safe_class);
  }
  return true;
}

bool PFS_index_events_waits_summary_by_event_name::match(PFS_rwlock *pfs) {
  if (m_fields >= 1) {
    PFS_rwlock_class *safe_class;
    safe_class = sanitize_rwlock_class(pfs->m_class);
    if (unlikely(safe_class == NULL)) {
      return false;
    }
    return m_key.match(safe_class);
  }
  return true;
}

bool PFS_index_events_waits_summary_by_event_name::match(PFS_cond *pfs) {
  if (m_fields >= 1) {
    PFS_cond_class *safe_class;
    safe_class = sanitize_cond_class(pfs->m_class);
    if (unlikely(safe_class == NULL)) {
      return false;
    }
    return m_key.match(safe_class);
  }
  return true;
}

bool PFS_index_events_waits_summary_by_event_name::match(PFS_file *pfs) {
  if (m_fields >= 1) {
    PFS_file_class *safe_class;
    safe_class = sanitize_file_class(pfs->m_class);
    if (unlikely(safe_class == NULL)) {
      return false;
    }
    return m_key.match(safe_class);
  }
  return true;
}

bool PFS_index_events_waits_summary_by_event_name::match(PFS_socket *pfs) {
  if (m_fields >= 1) {
    PFS_socket_class *safe_class;
    safe_class = sanitize_socket_class(pfs->m_class);
    if (unlikely(safe_class == NULL)) {
      return false;
    }
    return m_key.match(safe_class);
  }
  return true;
}

bool PFS_index_events_waits_summary_by_event_name::match_view(uint view) {
  if (m_fields >= 1) {
    return m_key.match_view(view);
  }
  return true;
}

PFS_engine_table *table_events_waits_summary_by_instance::create(
    PFS_engine_table_share *) {
  return new table_events_waits_summary_by_instance();
}

int table_events_waits_summary_by_instance::delete_all_rows(void) {
  reset_events_waits_by_instance();
  return 0;
}

table_events_waits_summary_by_instance::table_events_waits_summary_by_instance()
    : table_all_instr(&m_share) {
  m_normalizer = time_normalizer::get_wait();
}

int table_events_waits_summary_by_instance::index_init(uint idx, bool) {
  PFS_index_all_instr *result = NULL;
  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_events_waits_summary_by_instance);
      break;
    case 1:
      result = PFS_NEW(PFS_index_events_waits_summary_by_event_name);
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_events_waits_summary_by_instance::make_instr_row(
    PFS_instr *pfs, PFS_instr_class *klass, const void *object_instance_begin,
    PFS_single_stat *pfs_stat) {
  pfs_optimistic_state lock;

  /*
    Protect this reader against a mutex/rwlock/cond destroy,
    file delete, table drop.
  */
  pfs->m_lock.begin_optimistic_lock(&lock);

  m_row.m_name = klass->m_name;
  m_row.m_name_length = klass->m_name_length;
  m_row.m_object_instance_addr = (intptr)object_instance_begin;

  m_row.m_stat.set(m_normalizer, pfs_stat);

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

/**
  Build a row, for mutex statistics in a thread.
  @param pfs              the mutex this cursor is reading
*/
int table_events_waits_summary_by_instance::make_mutex_row(PFS_mutex *pfs) {
  PFS_mutex_class *safe_class;
  safe_class = sanitize_mutex_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  return make_instr_row(pfs, safe_class, pfs->m_identity,
                        &pfs->m_mutex_stat.m_wait_stat);
}

/**
  Build a row, for rwlock statistics in a thread.
  @param pfs              the rwlock this cursor is reading
*/
int table_events_waits_summary_by_instance::make_rwlock_row(PFS_rwlock *pfs) {
  PFS_rwlock_class *safe_class;
  safe_class = sanitize_rwlock_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  return make_instr_row(pfs, safe_class, pfs->m_identity,
                        &pfs->m_rwlock_stat.m_wait_stat);
}

/**
  Build a row, for condition statistics in a thread.
  @param pfs              the condition this cursor is reading
*/
int table_events_waits_summary_by_instance::make_cond_row(PFS_cond *pfs) {
  PFS_cond_class *safe_class;
  safe_class = sanitize_cond_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  return make_instr_row(pfs, safe_class, pfs->m_identity,
                        &pfs->m_cond_stat.m_wait_stat);
}

/**
  Build a row, for file statistics in a thread.
  @param pfs              the file this cursor is reading
*/
int table_events_waits_summary_by_instance::make_file_row(PFS_file *pfs) {
  PFS_file_class *safe_class;
  safe_class = sanitize_file_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  PFS_single_stat sum;
  pfs->m_file_stat.m_io_stat.sum_waits(&sum);
  /*
    Files don't have a in memory structure associated to it,
    so we use the address of the PFS_file buffer as object_instance_begin
  */
  return make_instr_row(pfs, safe_class, pfs, &sum);
}

/**
  Build a row, for socket statistics in a thread.
  @param pfs              the socket this cursor is reading
*/
int table_events_waits_summary_by_instance::make_socket_row(PFS_socket *pfs) {
  PFS_socket_class *safe_class;
  safe_class = sanitize_socket_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  /*
    Consolidate wait times and byte counts for individual operations. This is
    done by the consumer in order to reduce overhead on the socket instrument.
  */
  PFS_byte_stat pfs_stat;
  pfs->m_socket_stat.m_io_stat.sum(&pfs_stat);

  /*
    Sockets don't have an associated in-memory structure, so use the address of
    the PFS_socket buffer as object_instance_begin.
  */
  return make_instr_row(pfs, safe_class, pfs, &pfs_stat);
}

int table_events_waits_summary_by_instance::read_row_values(TABLE *table,
                                                            unsigned char *,
                                                            Field **fields,
                                                            bool read_all) {
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
          set_field_ulonglong(f, m_row.m_object_instance_addr);
          break;
        case 2: /* COUNT */
          set_field_ulonglong(f, m_row.m_stat.m_count);
          break;
        case 3: /* SUM */
          set_field_ulonglong(f, m_row.m_stat.m_sum);
          break;
        case 4: /* MIN */
          set_field_ulonglong(f, m_row.m_stat.m_min);
          break;
        case 5: /* AVG */
          set_field_ulonglong(f, m_row.m_stat.m_avg);
          break;
        case 6: /* MAX */
          set_field_ulonglong(f, m_row.m_stat.m_max);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
