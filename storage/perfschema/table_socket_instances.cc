/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_socket_instances.cc
  Table SOCKET_INSTANCES (implementation).
*/

#include "storage/perfschema/table_socket_instances.h"

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

THR_LOCK table_socket_instances::m_table_lock;

Plugin_table table_socket_instances::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "socket_instances",
    /* Definition */
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  THREAD_ID BIGINT unsigned,\n"
    "  SOCKET_ID INTEGER not null,\n"
    "  IP VARCHAR(64) not null,\n"
    "  PORT INTEGER not null,\n"
    "  STATE ENUM('IDLE','ACTIVE') not null,\n"
    "  PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,\n"
    "  KEY (THREAD_ID) USING HASH,\n"
    "  KEY (SOCKET_ID) USING HASH,\n"
    "  KEY (IP, PORT) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_socket_instances::m_share = {
    &pfs_readonly_acl,
    table_socket_instances::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    table_socket_instances::get_row_count,
    sizeof(PFS_simple_index),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_socket_instances_by_instance::match(const PFS_socket *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_socket_instances_by_thread::match(const PFS_socket *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match_owner(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_socket_instances_by_socket::match(const PFS_socket *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_socket_instances_by_ip_port::match(const PFS_socket *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!m_key_2.match(pfs)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_socket_instances::create(PFS_engine_table_share *) {
  return new table_socket_instances();
}

ha_rows table_socket_instances::get_row_count(void) {
  return global_socket_container.get_row_count();
}

table_socket_instances::table_socket_instances()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

void table_socket_instances::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_socket_instances::rnd_next(void) {
  PFS_socket *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_socket_iterator it = global_socket_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != NULL) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int table_socket_instances::rnd_pos(const void *pos) {
  PFS_socket *pfs;

  set_position(pos);

  pfs = global_socket_container.get(m_pos.m_index);
  if (pfs != NULL) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_socket_instances::index_init(uint idx, bool) {
  PFS_index_socket_instances *result = NULL;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_socket_instances_by_instance);
      break;
    case 1:
      result = PFS_NEW(PFS_index_socket_instances_by_thread);
      break;
    case 2:
      result = PFS_NEW(PFS_index_socket_instances_by_socket);
      break;
    case 3:
      result = PFS_NEW(PFS_index_socket_instances_by_ip_port);
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_socket_instances::index_next(void) {
  PFS_socket *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_socket_iterator it = global_socket_container.iterate(m_pos.m_index);

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

int table_socket_instances::make_row(PFS_socket *pfs) {
  pfs_optimistic_state lock;
  PFS_socket_class *safe_class;

  /* Protect this reader against a socket delete */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_socket_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  /** Extract ip address and port from raw address */
  m_row.m_ip_length =
      pfs_get_socket_address(m_row.m_ip, sizeof(m_row.m_ip), &m_row.m_port,
                             &pfs->m_sock_addr, pfs->m_addr_len);
  m_row.m_event_name = safe_class->m_name;
  m_row.m_event_name_length = safe_class->m_name_length;
  m_row.m_identity = pfs->m_identity;
  m_row.m_fd = pfs->m_fd;
  m_row.m_state =
      (pfs->m_idle ? PSI_SOCKET_STATE_IDLE : PSI_SOCKET_STATE_ACTIVE);
  PFS_thread *safe_thread = sanitize_thread(pfs->m_thread_owner);

  if (safe_thread != NULL) {
    m_row.m_thread_id = safe_thread->m_thread_internal_id;
    m_row.m_thread_id_set = true;
  } else {
    m_row.m_thread_id_set = false;
  }

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_socket_instances::read_row_values(TABLE *table, unsigned char *buf,
                                            Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* EVENT_NAME */
          set_field_varchar_utf8(f, m_row.m_event_name,
                                 m_row.m_event_name_length);
          break;
        case 1: /* OBJECT_INSTANCE_BEGIN */
          set_field_ulonglong(f, (intptr)m_row.m_identity);
          break;
        case 2: /* THREAD_ID */
          if (m_row.m_thread_id_set) {
            set_field_ulonglong(f, m_row.m_thread_id);
          } else {
            f->set_null();
          }
          break;
        case 3: /* SOCKET_ID */
          set_field_ulong(f, m_row.m_fd);
          break;
        case 4: /* IP */
          set_field_varchar_utf8(f, m_row.m_ip, m_row.m_ip_length);
          break;
        case 5: /* PORT */
          set_field_ulong(f, m_row.m_port);
          break;
        case 6: /* STATE */
          set_field_enum(f, m_row.m_state);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
