/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

/**
  @file storage/perfschema/table_ews_by_host_by_event_name.cc
  Table EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME (implementation).
*/

#include "storage/perfschema/table_ews_by_host_by_event_name.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_ews_by_host_by_event_name::m_table_lock;

Plugin_table table_ews_by_host_by_event_name::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_waits_summary_by_host_by_event_name",
    /* Definition */
    "  HOST CHAR(255) CHARACTER SET ASCII default null,\n"
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  COUNT_STAR BIGINT unsigned not null,\n"
    "  SUM_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MIN_TIMER_WAIT BIGINT unsigned not null,\n"
    "  AVG_TIMER_WAIT BIGINT unsigned not null,\n"
    "  MAX_TIMER_WAIT BIGINT unsigned not null,\n"
    "  UNIQUE KEY (HOST, EVENT_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_ews_by_host_by_event_name::m_share = {
    &pfs_truncatable_acl,
    table_ews_by_host_by_event_name::create,
    nullptr, /* write_row */
    table_ews_by_host_by_event_name::delete_all_rows,
    table_ews_by_host_by_event_name::get_row_count,
    sizeof(pos_ews_by_host_by_event_name),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_ews_by_host_by_event_name::match(PFS_host *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_ews_by_host_by_event_name::match_view(uint view) {
  if (m_fields >= 2) {
    return m_key_2.match_view(view);
  }
  return true;
}

bool PFS_index_ews_by_host_by_event_name::match(PFS_instr_class *instr_class) {
  if (m_fields >= 2) {
    return m_key_2.match(instr_class);
  }
  return true;
}

PFS_engine_table *table_ews_by_host_by_event_name::create(
    PFS_engine_table_share *) {
  return new table_ews_by_host_by_event_name();
}

int table_ews_by_host_by_event_name::delete_all_rows() {
  reset_events_waits_by_thread();
  reset_events_waits_by_account();
  reset_events_waits_by_host();
  return 0;
}

ha_rows table_ews_by_host_by_event_name::get_row_count() {
  return global_host_container.get_row_count() * wait_class_max;
}

table_ews_by_host_by_event_name::table_ews_by_host_by_event_name()
    : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos() {
  // For all cases except IDLE
  m_normalizer = time_normalizer::get_wait();
}

void table_ews_by_host_by_event_name::reset_position() {
  m_pos.reset();
  m_next_pos.reset();
}

int table_ews_by_host_by_event_name::rnd_next() {
  PFS_host *host;
  PFS_instr_class *instr_class;
  bool has_more_host = true;

  for (m_pos.set_at(&m_next_pos); has_more_host; m_pos.next_host()) {
    host = global_host_container.get(m_pos.m_index_1, &has_more_host);
    if (host != nullptr) {
      for (; m_pos.has_more_view(); m_pos.next_view()) {
        switch (m_pos.m_index_2) {
          case pos_ews_by_host_by_event_name::VIEW_MUTEX:
            instr_class = find_mutex_class(m_pos.m_index_3);
            break;
          case pos_ews_by_host_by_event_name::VIEW_RWLOCK:
            instr_class = find_rwlock_class(m_pos.m_index_3);
            break;
          case pos_ews_by_host_by_event_name::VIEW_COND:
            instr_class = find_cond_class(m_pos.m_index_3);
            break;
          case pos_ews_by_host_by_event_name::VIEW_FILE:
            instr_class = find_file_class(m_pos.m_index_3);
            break;
          case pos_ews_by_host_by_event_name::VIEW_TABLE:
            instr_class = find_table_class(m_pos.m_index_3);
            break;
          case pos_ews_by_host_by_event_name::VIEW_SOCKET:
            instr_class = find_socket_class(m_pos.m_index_3);
            break;
          case pos_ews_by_host_by_event_name::VIEW_IDLE:
            instr_class = find_idle_class(m_pos.m_index_3);
            break;
          case pos_ews_by_host_by_event_name::VIEW_METADATA:
            instr_class = find_metadata_class(m_pos.m_index_3);
            break;
          default:
            instr_class = nullptr;
            assert(false);
            break;
        }

        if (instr_class) {
          m_next_pos.set_after(&m_pos);
          return make_row(host, instr_class);
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_ews_by_host_by_event_name::rnd_pos(const void *pos) {
  PFS_host *host;
  PFS_instr_class *instr_class;

  set_position(pos);

  host = global_host_container.get(m_pos.m_index_1);
  if (host == nullptr) {
    return HA_ERR_RECORD_DELETED;
  }

  switch (m_pos.m_index_2) {
    case pos_ews_by_host_by_event_name::VIEW_MUTEX:
      instr_class = find_mutex_class(m_pos.m_index_3);
      break;
    case pos_ews_by_host_by_event_name::VIEW_RWLOCK:
      instr_class = find_rwlock_class(m_pos.m_index_3);
      break;
    case pos_ews_by_host_by_event_name::VIEW_COND:
      instr_class = find_cond_class(m_pos.m_index_3);
      break;
    case pos_ews_by_host_by_event_name::VIEW_FILE:
      instr_class = find_file_class(m_pos.m_index_3);
      break;
    case pos_ews_by_host_by_event_name::VIEW_TABLE:
      instr_class = find_table_class(m_pos.m_index_3);
      break;
    case pos_ews_by_host_by_event_name::VIEW_SOCKET:
      instr_class = find_socket_class(m_pos.m_index_3);
      break;
    case pos_ews_by_host_by_event_name::VIEW_IDLE:
      instr_class = find_idle_class(m_pos.m_index_3);
      break;
    case pos_ews_by_host_by_event_name::VIEW_METADATA:
      instr_class = find_metadata_class(m_pos.m_index_3);
      break;
    default:
      instr_class = nullptr;
      assert(false);
      break;
  }

  if (instr_class) {
    return make_row(host, instr_class);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_ews_by_host_by_event_name::index_init(uint idx [[maybe_unused]],
                                                bool) {
  PFS_index_ews_by_host_by_event_name *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_ews_by_host_by_event_name);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_ews_by_host_by_event_name::index_next() {
  PFS_host *host;
  PFS_instr_class *instr_class;
  bool has_more_host = true;

  for (m_pos.set_at(&m_next_pos); has_more_host; m_pos.next_host()) {
    host = global_host_container.get(m_pos.m_index_1, &has_more_host);
    if (host != nullptr) {
      if (m_opened_index->match(host)) {
        for (; m_pos.has_more_view(); m_pos.next_view()) {
          if (!m_opened_index->match_view(m_pos.m_index_2)) {
            continue;
          }

          do {
            switch (m_pos.m_index_2) {
              case pos_ews_by_host_by_event_name::VIEW_MUTEX:
                instr_class = find_mutex_class(m_pos.m_index_3);
                break;
              case pos_ews_by_host_by_event_name::VIEW_RWLOCK:
                instr_class = find_rwlock_class(m_pos.m_index_3);
                break;
              case pos_ews_by_host_by_event_name::VIEW_COND:
                instr_class = find_cond_class(m_pos.m_index_3);
                break;
              case pos_ews_by_host_by_event_name::VIEW_FILE:
                instr_class = find_file_class(m_pos.m_index_3);
                break;
              case pos_ews_by_host_by_event_name::VIEW_TABLE:
                instr_class = find_table_class(m_pos.m_index_3);
                break;
              case pos_ews_by_host_by_event_name::VIEW_SOCKET:
                instr_class = find_socket_class(m_pos.m_index_3);
                break;
              case pos_ews_by_host_by_event_name::VIEW_IDLE:
                instr_class = find_idle_class(m_pos.m_index_3);
                break;
              case pos_ews_by_host_by_event_name::VIEW_METADATA:
                instr_class = find_metadata_class(m_pos.m_index_3);
                break;
              default:
                instr_class = nullptr;
                assert(false);
                break;
            }

            if (instr_class != nullptr) {
              if (m_opened_index->match(instr_class)) {
                if (!make_row(host, instr_class)) {
                  m_next_pos.set_after(&m_pos);
                  return 0;
                }
              }
              m_pos.set_after(&m_pos);
            }
          } while (instr_class != nullptr);
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_ews_by_host_by_event_name::make_row(PFS_host *host,
                                              PFS_instr_class *klass) {
  time_normalizer *normalizer = m_normalizer;
  pfs_optimistic_state lock;

  host->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_host.make_row(host)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_event_name.make_row(klass);

  PFS_connection_wait_visitor visitor(klass);
  PFS_connection_iterator::visit_host(host, true, /* accounts */
                                      true,       /* threads */
                                      false,      /* THDs */
                                      &visitor);

  if (!host->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (klass->m_type == PFS_CLASS_IDLE) {
    normalizer = time_normalizer::get_idle();
  }

  m_row.m_stat.set(normalizer, &visitor.m_stat);

  return 0;
}

int table_ews_by_host_by_event_name::read_row_values(TABLE *table,
                                                     unsigned char *buf,
                                                     Field **fields,
                                                     bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* HOST */
          m_row.m_host.set_nullable_field(f);
          break;
        case 1: /* EVENT_NAME */
          m_row.m_event_name.set_field(f);
          break;
        default: /* 2, ... COUNT/SUM/MIN/AVG/MAX */
          m_row.m_stat.set_field(f->field_index() - 2, f);
          break;
      }
    }
  }

  return 0;
}
