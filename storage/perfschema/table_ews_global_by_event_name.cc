/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/table_ews_global_by_event_name.cc
  Table EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (implementation).
*/

#include "storage/perfschema/table_ews_global_by_event_name.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_timer.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_ews_global_by_event_name::m_table_lock;

Plugin_table table_ews_global_by_event_name::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_waits_summary_global_by_event_name",
    /* Definition */
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  COUNT_STAR BIGINT unsigned not null,\n"
    "  SUM_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MIN_TIMER_WAIT BIGINT unsigned not null,\n"
    "  AVG_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MAX_TIMER_WAIT BIGINT unsigned not null,\n"
    "  PRIMARY KEY (EVENT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_ews_global_by_event_name::m_share = {
    &pfs_truncatable_acl,
    table_ews_global_by_event_name::create,
    nullptr, /* write_row */
    table_ews_global_by_event_name::delete_all_rows,
    table_ews_global_by_event_name::get_row_count,
    sizeof(pos_ews_global_by_event_name),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_ews_global_by_event_name::match_view(uint view) {
  if (m_fields >= 1) {
    return m_key.match_view(view);
  }
  return true;
}

bool PFS_index_ews_global_by_event_name::match(PFS_instr_class *instr_class) {
  if (m_fields >= 1) {
    if (!m_key.match(instr_class)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_ews_global_by_event_name::create(
    PFS_engine_table_share *) {
  return new table_ews_global_by_event_name();
}

int table_ews_global_by_event_name::delete_all_rows() {
  reset_events_waits_by_instance();
  reset_table_waits_by_table_handle();
  reset_table_waits_by_table();
  reset_events_waits_by_class();
  return 0;
}

ha_rows table_ews_global_by_event_name::get_row_count() {
  return wait_class_max;
}

table_ews_global_by_event_name::table_ews_global_by_event_name()
    : PFS_engine_table(&m_share, &m_pos), m_opened_index(nullptr) {
  // For all cases except IDLE
  m_normalizer = time_normalizer::get_wait();
}

void table_ews_global_by_event_name::reset_position() {
  m_pos.reset();
  m_next_pos.reset();
}

int table_ews_global_by_event_name::rnd_next() {
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;
  PFS_socket_class *socket_class;
  PFS_instr_class *instr_class;

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_view(); m_pos.next_view()) {
    switch (m_pos.m_index_1) {
      case pos_ews_global_by_event_name::VIEW_MUTEX:
        mutex_class = find_mutex_class(m_pos.m_index_2);
        if (mutex_class) {
          m_next_pos.set_after(&m_pos);
          return make_mutex_row(mutex_class);
        }
        break;
      case pos_ews_global_by_event_name::VIEW_RWLOCK:
        rwlock_class = find_rwlock_class(m_pos.m_index_2);
        if (rwlock_class) {
          m_next_pos.set_after(&m_pos);
          return make_rwlock_row(rwlock_class);
        }
        break;
      case pos_ews_global_by_event_name::VIEW_COND:
        cond_class = find_cond_class(m_pos.m_index_2);
        if (cond_class) {
          m_next_pos.set_after(&m_pos);
          return make_cond_row(cond_class);
        }
        break;
      case pos_ews_global_by_event_name::VIEW_FILE:
        file_class = find_file_class(m_pos.m_index_2);
        if (file_class) {
          m_next_pos.set_after(&m_pos);
          return make_file_row(file_class);
        }
        break;
      case pos_ews_global_by_event_name::VIEW_TABLE:
        if (m_pos.m_index_2 == 1) {
          m_next_pos.set_after(&m_pos);
          return make_table_io_row(&global_table_io_class);
        }
        if (m_pos.m_index_2 == 2) {
          m_next_pos.set_after(&m_pos);
          return make_table_lock_row(&global_table_lock_class);
        }
        break;
      case pos_ews_global_by_event_name::VIEW_SOCKET:
        socket_class = find_socket_class(m_pos.m_index_2);
        if (socket_class) {
          m_next_pos.set_after(&m_pos);
          return make_socket_row(socket_class);
        }
        break;
      case pos_ews_global_by_event_name::VIEW_IDLE:
        instr_class = find_idle_class(m_pos.m_index_2);
        if (instr_class) {
          m_next_pos.set_after(&m_pos);
          return make_idle_row(instr_class);
        }
        break;
      case pos_ews_global_by_event_name::VIEW_METADATA:
        instr_class = find_metadata_class(m_pos.m_index_2);
        if (instr_class) {
          m_next_pos.set_after(&m_pos);
          return make_metadata_row(instr_class);
        }
        break;
      default:
        break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_ews_global_by_event_name::rnd_pos(const void *pos) {
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;
  PFS_socket_class *socket_class;
  PFS_instr_class *instr_class;

  set_position(pos);

  switch (m_pos.m_index_1) {
    case pos_ews_global_by_event_name::VIEW_MUTEX:
      mutex_class = find_mutex_class(m_pos.m_index_2);
      if (mutex_class) {
        return make_mutex_row(mutex_class);
      }
      break;
    case pos_ews_global_by_event_name::VIEW_RWLOCK:
      rwlock_class = find_rwlock_class(m_pos.m_index_2);
      if (rwlock_class) {
        return make_rwlock_row(rwlock_class);
      }
      break;
    case pos_ews_global_by_event_name::VIEW_COND:
      cond_class = find_cond_class(m_pos.m_index_2);
      if (cond_class) {
        return make_cond_row(cond_class);
      }
      break;
    case pos_ews_global_by_event_name::VIEW_FILE:
      file_class = find_file_class(m_pos.m_index_2);
      if (file_class) {
        return make_file_row(file_class);
      }
      break;
    case pos_ews_global_by_event_name::VIEW_TABLE:
      assert(m_pos.m_index_2 >= 1);
      assert(m_pos.m_index_2 <= 2);
      if (m_pos.m_index_2 == 1) {
        return make_table_io_row(&global_table_io_class);
      }
      return make_table_lock_row(&global_table_lock_class);
    case pos_ews_global_by_event_name::VIEW_SOCKET:
      socket_class = find_socket_class(m_pos.m_index_2);
      if (socket_class) {
        return make_socket_row(socket_class);
      }
      break;
    case pos_ews_global_by_event_name::VIEW_IDLE:
      instr_class = find_idle_class(m_pos.m_index_2);
      if (instr_class) {
        return make_idle_row(instr_class);
      }
      break;
    case pos_ews_global_by_event_name::VIEW_METADATA:
      instr_class = find_metadata_class(m_pos.m_index_2);
      if (instr_class) {
        return make_metadata_row(instr_class);
      }
      break;
    default:
      assert(false);
      break;
  }

  return HA_ERR_RECORD_DELETED;
}

int table_ews_global_by_event_name::index_init(uint idx [[maybe_unused]],
                                               bool) {
  assert(idx == 0);
  auto *result = PFS_NEW(PFS_index_ews_global_by_event_name);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_ews_global_by_event_name::index_next() {
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;
  PFS_instr_class *table_class;
  PFS_socket_class *socket_class;
  PFS_instr_class *instr_class;

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_view(); m_pos.next_view()) {
    if (!m_opened_index->match_view(m_pos.m_index_1)) {
      continue;
    }

    switch (m_pos.m_index_1) {
      case pos_ews_global_by_event_name::VIEW_MUTEX:
        do {
          mutex_class = find_mutex_class(m_pos.m_index_2);
          if (mutex_class) {
            if (m_opened_index->match(mutex_class)) {
              m_next_pos.set_after(&m_pos);
              return make_mutex_row(mutex_class);
            }
            m_pos.set_after(&m_pos);
          }
        } while (mutex_class != nullptr);
        break;
      case pos_ews_global_by_event_name::VIEW_RWLOCK:
        do {
          rwlock_class = find_rwlock_class(m_pos.m_index_2);
          if (rwlock_class) {
            if (m_opened_index->match(rwlock_class)) {
              m_next_pos.set_after(&m_pos);
              return make_rwlock_row(rwlock_class);
            }
            m_pos.set_after(&m_pos);
          }
        } while (rwlock_class != nullptr);

        break;
      case pos_ews_global_by_event_name::VIEW_COND:
        do {
          cond_class = find_cond_class(m_pos.m_index_2);
          if (cond_class) {
            if (m_opened_index->match(cond_class)) {
              m_next_pos.set_after(&m_pos);
              return make_cond_row(cond_class);
            }
            m_pos.set_after(&m_pos);
          }
        } while (cond_class != nullptr);
        break;
      case pos_ews_global_by_event_name::VIEW_FILE:
        do {
          file_class = find_file_class(m_pos.m_index_2);
          if (file_class) {
            if (m_opened_index->match(file_class)) {
              m_next_pos.set_after(&m_pos);
              return make_file_row(file_class);
            }
            m_pos.set_after(&m_pos);
          }
        } while (file_class != nullptr);
        break;
      case pos_ews_global_by_event_name::VIEW_TABLE:
        do {
          table_class = find_table_class(m_pos.m_index_2);
          if (table_class) {
            if (m_opened_index->match(table_class)) {
              m_next_pos.set_after(&m_pos);
              if (m_pos.m_index_2 == 1) {
                return make_table_io_row(table_class);
              }
              return make_table_lock_row(table_class);
            }
            m_pos.set_after(&m_pos);
          }
        } while (table_class != nullptr);
        break;
      case pos_ews_global_by_event_name::VIEW_SOCKET:
        do {
          socket_class = find_socket_class(m_pos.m_index_2);
          if (socket_class) {
            if (m_opened_index->match(socket_class)) {
              m_next_pos.set_after(&m_pos);
              return make_socket_row(socket_class);
            }
            m_pos.set_after(&m_pos);
          }
        } while (socket_class != nullptr);
        break;
      case pos_ews_global_by_event_name::VIEW_IDLE:
        do {
          instr_class = find_idle_class(m_pos.m_index_2);
          if (instr_class) {
            if (m_opened_index->match(instr_class)) {
              m_next_pos.set_after(&m_pos);
              return make_idle_row(instr_class);
            }
            m_pos.set_after(&m_pos);
          }
        } while (instr_class != nullptr);
        break;
      case pos_ews_global_by_event_name::VIEW_METADATA:
        do {
          instr_class = find_metadata_class(m_pos.m_index_2);
          if (instr_class) {
            if (m_opened_index->match(instr_class)) {
              m_next_pos.set_after(&m_pos);
              return make_metadata_row(instr_class);
            }
            m_pos.set_after(&m_pos);
          }
        } while (instr_class != nullptr);
        break;
      default:
        break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_ews_global_by_event_name::make_mutex_row(PFS_mutex_class *klass) {
  m_row.m_event_name.make_row(klass);

  PFS_instance_wait_visitor visitor;
  PFS_instance_iterator::visit_mutex_instances(klass, &visitor);

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);
  return 0;
}

int table_ews_global_by_event_name::make_rwlock_row(PFS_rwlock_class *klass) {
  m_row.m_event_name.make_row(klass);

  PFS_instance_wait_visitor visitor;
  PFS_instance_iterator::visit_rwlock_instances(klass, &visitor);

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);
  return 0;
}

int table_ews_global_by_event_name::make_cond_row(PFS_cond_class *klass) {
  m_row.m_event_name.make_row(klass);

  PFS_instance_wait_visitor visitor;
  PFS_instance_iterator::visit_cond_instances(klass, &visitor);

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);
  return 0;
}

int table_ews_global_by_event_name::make_file_row(PFS_file_class *klass) {
  m_row.m_event_name.make_row(klass);

  PFS_instance_wait_visitor visitor;
  PFS_instance_iterator::visit_file_instances(klass, &visitor);

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);
  return 0;
}

int table_ews_global_by_event_name::make_table_io_row(PFS_instr_class *klass) {
  m_row.m_event_name.make_row(klass);

  PFS_table_io_wait_visitor visitor;
  PFS_object_iterator::visit_all_tables(&visitor);

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);
  return 0;
}

int table_ews_global_by_event_name::make_table_lock_row(
    PFS_instr_class *klass) {
  m_row.m_event_name.make_row(klass);

  PFS_table_lock_wait_visitor visitor;
  PFS_object_iterator::visit_all_tables(&visitor);

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);
  return 0;
}

int table_ews_global_by_event_name::make_socket_row(PFS_socket_class *klass) {
  m_row.m_event_name.make_row(klass);

  PFS_instance_wait_visitor visitor;
  PFS_instance_iterator::visit_socket_instances(klass, &visitor);

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);
  return 0;
}

int table_ews_global_by_event_name::make_idle_row(PFS_instr_class *klass) {
  m_row.m_event_name.make_row(klass);

  PFS_connection_wait_visitor visitor(klass);
  PFS_connection_iterator::visit_global(false, /* hosts */
                                        false, /* users */
                                        false, /* accounts */
                                        true,  /* threads */
                                        false, /* THDs */
                                        &visitor);

  time_normalizer *normalizer = time_normalizer::get_idle();
  m_row.m_stat.set(normalizer, &visitor.m_stat);
  return 0;
}

int table_ews_global_by_event_name::make_metadata_row(PFS_instr_class *klass) {
  m_row.m_event_name.make_row(klass);

  PFS_connection_wait_visitor visitor(klass);
  PFS_connection_iterator::visit_global(false, /* hosts */
                                        true,  /* users */
                                        true,  /* accounts */
                                        true,  /* threads */
                                        false, /* THDs */
                                        &visitor);
  m_row.m_stat.set(m_normalizer, &visitor.m_stat);
  return 0;
}

int table_ews_global_by_event_name::read_row_values(TABLE *table,
                                                    unsigned char *,
                                                    Field **fields,
                                                    bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 0);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* EVENT_NAME */
          m_row.m_event_name.set_field(f);
          break;
        default: /* 1, ... COUNT/SUM/MIN/AVG/MAX */
          m_row.m_stat.set_field(f->field_index() - 1, f);
          break;
      }
    }
  }

  return 0;
}
