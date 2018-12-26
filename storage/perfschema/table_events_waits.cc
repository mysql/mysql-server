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
  @file storage/perfschema/table_events_waits.cc
  Table EVENTS_WAITS_xxx (implementation).
*/

#include "storage/perfschema/table_events_waits.h"

#include "lex_string.h"
#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_events_waits.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_timer.h"

bool PFS_index_events_waits::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_events_waits::match(PFS_events_waits *pfs) {
  if (m_fields >= 2) {
    if (!m_key_2.match(pfs)) {
      return false;
    }
  }

  return true;
}

THR_LOCK table_events_waits_current::m_table_lock;

Plugin_table table_events_waits_current::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_waits_current",
    /* Definition */
    "  THREAD_ID BIGINT unsigned not null,\n"
    "  EVENT_ID BIGINT unsigned not null,\n"
    "  END_EVENT_ID BIGINT unsigned,\n"
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  SOURCE VARCHAR(64),\n"
    "  TIMER_START BIGINT unsigned,\n"
    "  TIMER_END BIGINT unsigned,\n"
    "  TIMER_WAIT BIGINT unsigned,\n"
    "  SPINS INTEGER unsigned,\n"
    "  OBJECT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_NAME VARCHAR(512),\n"
    "  INDEX_NAME VARCHAR(64),\n"
    "  OBJECT_TYPE VARCHAR(64),\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  NESTING_EVENT_ID BIGINT unsigned,\n"
    "  NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),\n"
    "  OPERATION VARCHAR(32) not null,\n"
    "  NUMBER_OF_BYTES BIGINT,\n"
    "  FLAGS INTEGER unsigned,\n"
    "  PRIMARY KEY (THREAD_ID, EVENT_ID)\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_events_waits_current::m_share = {
    &pfs_truncatable_acl,
    table_events_waits_current::create,
    NULL, /* write_row */
    table_events_waits_current::delete_all_rows,
    table_events_waits_current::get_row_count,
    sizeof(pos_events_waits_current), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

THR_LOCK table_events_waits_history::m_table_lock;

Plugin_table table_events_waits_history::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_waits_history",
    /* Definition */
    "  THREAD_ID BIGINT unsigned not null,\n"
    "  EVENT_ID BIGINT unsigned not null,\n"
    "  END_EVENT_ID BIGINT unsigned,\n"
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  SOURCE VARCHAR(64),\n"
    "  TIMER_START BIGINT unsigned,\n"
    "  TIMER_END BIGINT unsigned,\n"
    "  TIMER_WAIT BIGINT unsigned,\n"
    "  SPINS INTEGER unsigned,\n"
    "  OBJECT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_NAME VARCHAR(512),\n"
    "  INDEX_NAME VARCHAR(64),\n"
    "  OBJECT_TYPE VARCHAR(64),\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  NESTING_EVENT_ID BIGINT unsigned,\n"
    "  NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),\n"
    "  OPERATION VARCHAR(32) not null,\n"
    "  NUMBER_OF_BYTES BIGINT,\n"
    "  FLAGS INTEGER unsigned,\n"
    "  PRIMARY KEY (THREAD_ID, EVENT_ID)\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_events_waits_history::m_share = {
    &pfs_truncatable_acl,
    table_events_waits_history::create,
    NULL, /* write_row */
    table_events_waits_history::delete_all_rows,
    table_events_waits_history::get_row_count,
    sizeof(pos_events_waits_history), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

THR_LOCK table_events_waits_history_long::m_table_lock;

Plugin_table table_events_waits_history_long::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "events_waits_history_long",
    /* Definition */
    "  THREAD_ID BIGINT unsigned not null,\n"
    "  EVENT_ID BIGINT unsigned not null,\n"
    "  END_EVENT_ID BIGINT unsigned,\n"
    "  EVENT_NAME VARCHAR(128) not null,\n"
    "  SOURCE VARCHAR(64),\n"
    "  TIMER_START BIGINT unsigned,\n"
    "  TIMER_END BIGINT unsigned,\n"
    "  TIMER_WAIT BIGINT unsigned,\n"
    "  SPINS INTEGER unsigned,\n"
    "  OBJECT_SCHEMA VARCHAR(64),\n"
    "  OBJECT_NAME VARCHAR(512),\n"
    "  INDEX_NAME VARCHAR(64),\n"
    "  OBJECT_TYPE VARCHAR(64),\n"
    "  OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,\n"
    "  NESTING_EVENT_ID BIGINT unsigned,\n"
    "  NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),\n"
    "  OPERATION VARCHAR(32) not null,\n"
    "  NUMBER_OF_BYTES BIGINT,\n"
    "  FLAGS INTEGER unsigned\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_events_waits_history_long::m_share = {
    &pfs_truncatable_acl,
    table_events_waits_history_long::create,
    NULL, /* write_row */
    table_events_waits_history_long::delete_all_rows,
    table_events_waits_history_long::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

table_events_waits_common::table_events_waits_common(
    const PFS_engine_table_share *share, void *pos)
    : PFS_engine_table(share, pos) {
  m_normalizer = time_normalizer::get_wait();
}

void table_events_waits_common::clear_object_columns() {
  m_row.m_object_type_length = 0;
  m_row.m_object_schema_length = 0;
  m_row.m_object_name_length = 0;
  m_row.m_index_name_length = 0;
}

int table_events_waits_common::make_table_object_columns(
    PFS_events_waits *wait) {
  uint safe_index;
  PFS_table_share *safe_table_share;

  safe_table_share = sanitize_table_share(wait->m_weak_table_share);
  if (unlikely(safe_table_share == NULL)) {
    return 1;
  }

  if (wait->m_object_type == OBJECT_TYPE_TABLE) {
    m_row.m_object_type = "TABLE";
    m_row.m_object_type_length = 5;
  } else {
    m_row.m_object_type = "TEMPORARY TABLE";
    m_row.m_object_type_length = 15;
  }

  if (safe_table_share->get_version() == wait->m_weak_version) {
    /* OBJECT SCHEMA */
    m_row.m_object_schema_length = safe_table_share->m_schema_name_length;
    if (unlikely(
            (m_row.m_object_schema_length == 0) ||
            (m_row.m_object_schema_length > sizeof(m_row.m_object_schema)))) {
      return 1;
    }
    memcpy(m_row.m_object_schema, safe_table_share->m_schema_name,
           m_row.m_object_schema_length);

    /* OBJECT NAME */
    m_row.m_object_name_length = safe_table_share->m_table_name_length;
    if (unlikely((m_row.m_object_name_length == 0) ||
                 (m_row.m_object_name_length > sizeof(m_row.m_object_name)))) {
      return 1;
    }
    memcpy(m_row.m_object_name, safe_table_share->m_table_name,
           m_row.m_object_name_length);

    /* INDEX NAME */
    safe_index = wait->m_index;
    uint safe_key_count = sanitize_index_count(safe_table_share->m_key_count);
    if (safe_index < safe_key_count) {
      PFS_table_share_index *index_stat;
      index_stat = safe_table_share->find_index_stat(safe_index);

      if (index_stat != NULL) {
        m_row.m_index_name_length = index_stat->m_key.m_name_length;

        if (unlikely(
                (m_row.m_index_name_length == 0) ||
                (m_row.m_index_name_length > sizeof(m_row.m_index_name)))) {
          return 1;
        }

        memcpy(m_row.m_index_name, index_stat->m_key.m_name,
               m_row.m_index_name_length);
      } else {
        m_row.m_index_name_length = 0;
      }
    } else {
      m_row.m_index_name_length = 0;
    }
  } else {
    m_row.m_object_schema_length = 0;
    m_row.m_object_name_length = 0;
    m_row.m_index_name_length = 0;
  }

  m_row.m_object_instance_addr = (intptr)wait->m_object_instance_addr;
  return 0;
}

int table_events_waits_common::make_file_object_columns(
    PFS_events_waits *wait) {
  PFS_file *safe_file;

  safe_file = sanitize_file(wait->m_weak_file);
  if (unlikely(safe_file == NULL)) {
    return 1;
  }

  m_row.m_object_type = "FILE";
  m_row.m_object_type_length = 4;
  m_row.m_object_schema_length = 0;
  m_row.m_object_instance_addr = (intptr)wait->m_object_instance_addr;

  if (safe_file->get_version() == wait->m_weak_version) {
    /* OBJECT NAME */
    m_row.m_object_name_length = safe_file->m_filename_length;
    if (unlikely((m_row.m_object_name_length == 0) ||
                 (m_row.m_object_name_length > sizeof(m_row.m_object_name)))) {
      return 1;
    }
    memcpy(m_row.m_object_name, safe_file->m_filename,
           m_row.m_object_name_length);
  } else {
    m_row.m_object_name_length = 0;
  }

  m_row.m_index_name_length = 0;

  return 0;
}

int table_events_waits_common::make_socket_object_columns(
    PFS_events_waits *wait) {
  PFS_socket *safe_socket;

  safe_socket = sanitize_socket(wait->m_weak_socket);
  if (unlikely(safe_socket == NULL)) {
    return 1;
  }

  m_row.m_object_type = "SOCKET";
  m_row.m_object_type_length = 6;
  m_row.m_object_schema_length = 0;
  m_row.m_object_instance_addr = (intptr)wait->m_object_instance_addr;

  if (safe_socket->get_version() == wait->m_weak_version) {
    /* Convert port number to string, include delimiter in port name length */

    uint port;
    char port_str[128];
    char ip_str[INET6_ADDRSTRLEN + 1];
    uint ip_len = 0;
    port_str[0] = ':';

    /* Get the IP address and port number */
    ip_len = pfs_get_socket_address(ip_str, sizeof(ip_str), &port,
                                    &safe_socket->m_sock_addr,
                                    safe_socket->m_addr_len);

    /* Convert port number to a string (length includes ':') */
    size_t port_len = int10_to_str(port, (port_str + 1), 10) - port_str + 1;

    /* OBJECT NAME */
    m_row.m_object_name_length = ip_len + port_len;

    if (unlikely((m_row.m_object_name_length == 0) ||
                 (m_row.m_object_name_length > sizeof(m_row.m_object_name)))) {
      return 1;
    }

    char *name = m_row.m_object_name;
    memcpy(name, ip_str, ip_len);
    memcpy(name + ip_len, port_str, port_len);
  } else {
    m_row.m_object_name_length = 0;
  }

  m_row.m_index_name_length = 0;

  return 0;
}

int table_events_waits_common::make_metadata_lock_object_columns(
    PFS_events_waits *wait) {
  PFS_metadata_lock *safe_metadata_lock;

  safe_metadata_lock = sanitize_metadata_lock(wait->m_weak_metadata_lock);
  if (unlikely(safe_metadata_lock == NULL)) {
    return 1;
  }

  if (safe_metadata_lock->get_version() == wait->m_weak_version) {
    // TODO: remove code duplication with PFS_column_row::make_row()
    static_assert(MDL_key::NAMESPACE_END == 17,
                  "Adjust performance schema when changing enum_mdl_namespace");

    MDL_key *mdl = &safe_metadata_lock->m_mdl_key;

    switch (mdl->mdl_namespace()) {
      case MDL_key::GLOBAL:
        m_row.m_object_type = "GLOBAL";
        m_row.m_object_type_length = 6;
        m_row.m_object_schema_length = 0;
        m_row.m_object_name_length = 0;
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::TABLESPACE:
        m_row.m_object_type = "TABLESPACE";
        m_row.m_object_type_length = 10;
        m_row.m_object_schema_length = 0;
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::SCHEMA:
        m_row.m_object_type = "SCHEMA";
        m_row.m_object_type_length = 6;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = 0;
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::TABLE:
        m_row.m_object_type = "TABLE";
        m_row.m_object_type_length = 5;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::FUNCTION:
        m_row.m_object_type = "FUNCTION";
        m_row.m_object_type_length = 8;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::PROCEDURE:
        m_row.m_object_type = "PROCEDURE";
        m_row.m_object_type_length = 9;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::TRIGGER:
        m_row.m_object_type = "TRIGGER";
        m_row.m_object_type_length = 7;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::EVENT:
        m_row.m_object_type = "EVENT";
        m_row.m_object_type_length = 5;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::COMMIT:
        m_row.m_object_type = "COMMIT";
        m_row.m_object_type_length = 6;
        m_row.m_object_schema_length = 0;
        m_row.m_object_name_length = 0;
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::USER_LEVEL_LOCK:
        m_row.m_object_type = "USER LEVEL LOCK";
        m_row.m_object_type_length = 15;
        m_row.m_object_schema_length = 0;
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::LOCKING_SERVICE:
        m_row.m_object_type = "LOCKING SERVICE";
        m_row.m_object_type_length = 15;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::SRID:
        m_row.m_object_type = "SRID";
        m_row.m_object_type_length = 4;
        m_row.m_object_schema_length = 0;
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::ACL_CACHE:
        m_row.m_object_type = "ACL CACHE";
        m_row.m_object_type_length = 9;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::COLUMN_STATISTICS:
        m_row.m_object_type = "COLUMN STATISTICS";
        m_row.m_object_type_length = 17;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = mdl->name_length();
        // Reusing the INDEX_NAME column for COLUMN_NAME
        m_row.m_index_name_length = mdl->col_name_length();
        break;
      case MDL_key::BACKUP_LOCK:
        m_row.m_object_type = "BACKUP_LOCK";
        m_row.m_object_type_length = sizeof("BACKUP_LOCK") - 1;
        m_row.m_object_schema_length = 0;
        m_row.m_object_name_length = 0;
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::RESOURCE_GROUPS:
        m_row.m_object_type = "RESOURCE_GROUPS";
        m_row.m_object_type_length = 15;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = mdl->name_length();
        m_row.m_index_name_length = 0;
        break;
      case MDL_key::FOREIGN_KEY:
        m_row.m_object_type = "FOREIGN KEY";
        m_row.m_object_type_length = 11;
        m_row.m_object_schema_length = mdl->db_name_length();
        m_row.m_object_name_length = mdl->name_length();
        break;
      case MDL_key::NAMESPACE_END:
      default:
        m_row.m_object_type_length = 0;
        m_row.m_object_schema_length = 0;
        m_row.m_object_name_length = 0;
        m_row.m_index_name_length = 0;
        break;
    }

    if (m_row.m_object_schema_length > sizeof(m_row.m_object_schema)) {
      return 1;
    }
    if (m_row.m_object_schema_length > 0) {
      memcpy(m_row.m_object_schema, mdl->db_name(),
             m_row.m_object_schema_length);
    }

    if (m_row.m_object_name_length > sizeof(m_row.m_object_name)) {
      return 1;
    }
    if (m_row.m_object_name_length > 0) {
      memcpy(m_row.m_object_name, mdl->name(), m_row.m_object_name_length);
    }

    if (m_row.m_index_name_length > sizeof(m_row.m_index_name)) {
      return 1;
    }
    if (m_row.m_index_name_length > 0) {
      memcpy(m_row.m_index_name, mdl->col_name(), m_row.m_index_name_length);
    }

    m_row.m_object_instance_addr = (intptr)wait->m_object_instance_addr;
  } else {
    m_row.m_object_type_length = 0;
    m_row.m_object_schema_length = 0;
    m_row.m_object_name_length = 0;
    m_row.m_index_name_length = 0;
    m_row.m_object_instance_addr = 0;
  }

  return 0;
}

/**
  Build a row.
  @param wait                       the wait the cursor is reading
  @return 0 on success or HA_ERR_RECORD_DELETED
*/
int table_events_waits_common::make_row(PFS_events_waits *wait) {
  PFS_instr_class *safe_class;
  ulonglong timer_end;
  /* wait normalizer for most rows. */
  time_normalizer *normalizer = m_normalizer;

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

  /*
    PFS_events_waits::m_class needs to be sanitized,
    for race conditions when this code:
    - reads a new value in m_wait_class,
    - reads an old value in m_class.
  */
  switch (wait->m_wait_class) {
    case WAIT_CLASS_METADATA:
      if (make_metadata_lock_object_columns(wait)) {
        return HA_ERR_RECORD_DELETED;
      }
      safe_class = sanitize_metadata_class(wait->m_class);
      break;
    case WAIT_CLASS_IDLE:
      clear_object_columns();
      m_row.m_object_instance_addr = 0;
      safe_class = sanitize_idle_class(wait->m_class);
      normalizer = time_normalizer::get_idle();
      break;
    case WAIT_CLASS_MUTEX:
      clear_object_columns();
      m_row.m_object_instance_addr = (intptr)wait->m_object_instance_addr;
      safe_class = sanitize_mutex_class((PFS_mutex_class *)wait->m_class);
      break;
    case WAIT_CLASS_RWLOCK:
      clear_object_columns();
      m_row.m_object_instance_addr = (intptr)wait->m_object_instance_addr;
      safe_class = sanitize_rwlock_class((PFS_rwlock_class *)wait->m_class);
      break;
    case WAIT_CLASS_COND:
      clear_object_columns();
      m_row.m_object_instance_addr = (intptr)wait->m_object_instance_addr;
      safe_class = sanitize_cond_class((PFS_cond_class *)wait->m_class);
      break;
    case WAIT_CLASS_TABLE:
      if (make_table_object_columns(wait)) {
        return HA_ERR_RECORD_DELETED;
      }
      safe_class = sanitize_table_class(wait->m_class);
      break;
    case WAIT_CLASS_FILE:
      if (make_file_object_columns(wait)) {
        return HA_ERR_RECORD_DELETED;
      }
      safe_class = sanitize_file_class((PFS_file_class *)wait->m_class);
      break;
    case WAIT_CLASS_SOCKET:
      if (make_socket_object_columns(wait)) {
        return HA_ERR_RECORD_DELETED;
      }
      safe_class = sanitize_socket_class((PFS_socket_class *)wait->m_class);
      break;
    case NO_WAIT_CLASS:
    default:
      return HA_ERR_RECORD_DELETED;
  }

  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_thread_internal_id = wait->m_thread_internal_id;
  m_row.m_event_id = wait->m_event_id;
  m_row.m_end_event_id = wait->m_end_event_id;
  m_row.m_nesting_event_id = wait->m_nesting_event_id;
  m_row.m_nesting_event_type = wait->m_nesting_event_type;

  if (m_row.m_end_event_id == 0) {
    if (wait->m_wait_class == WAIT_CLASS_IDLE) {
      timer_end = get_idle_timer();
    } else {
      timer_end = get_wait_timer();
    }
  } else {
    timer_end = wait->m_timer_end;
  }

  normalizer->to_pico(wait->m_timer_start, timer_end, &m_row.m_timer_start,
                      &m_row.m_timer_end, &m_row.m_timer_wait);

  m_row.m_name = safe_class->m_name;
  m_row.m_name_length = safe_class->m_name_length;

  make_source_column(wait->m_source_file, wait->m_source_line, m_row.m_source,
                     sizeof(m_row.m_source), m_row.m_source_length);

  m_row.m_operation = wait->m_operation;
  m_row.m_number_of_bytes = wait->m_number_of_bytes;
  m_row.m_flags = wait->m_flags;

  return 0;
}

/**
  Operations names map, as displayed in the 'OPERATION' column.
  Indexed by enum_operation_type - 1.
  Note: enum_operation_type contains a more precise definition,
  since more details are needed internally by the instrumentation.
  Different similar operations (CLOSE vs STREAMCLOSE) are displayed
  with the same name 'close'.
*/
static const LEX_STRING operation_names_map[] = {
    /* Mutex operations */
    {C_STRING_WITH_LEN("lock")},
    {C_STRING_WITH_LEN("try_lock")},

    /* RWLock operations (RW-lock) */
    {C_STRING_WITH_LEN("read_lock")},
    {C_STRING_WITH_LEN("write_lock")},
    {C_STRING_WITH_LEN("try_read_lock")},
    {C_STRING_WITH_LEN("try_write_lock")},

    /* RWLock operations (SX-lock) */
    {C_STRING_WITH_LEN("shared_lock")},
    {C_STRING_WITH_LEN("shared_exclusive_lock")},
    {C_STRING_WITH_LEN("exclusive_lock")},
    {C_STRING_WITH_LEN("try_shared_lock")},
    {C_STRING_WITH_LEN("try_shared_exclusive_lock")},
    {C_STRING_WITH_LEN("try_exclusive_lock")},

    /* Condition operations */
    {C_STRING_WITH_LEN("wait")},
    {C_STRING_WITH_LEN("timed_wait")},

    /* File operations */
    {C_STRING_WITH_LEN("create")},
    {C_STRING_WITH_LEN("create")}, /* create tmp */
    {C_STRING_WITH_LEN("open")},
    {C_STRING_WITH_LEN("open")}, /* stream open */
    {C_STRING_WITH_LEN("close")},
    {C_STRING_WITH_LEN("close")}, /* stream close */
    {C_STRING_WITH_LEN("read")},
    {C_STRING_WITH_LEN("write")},
    {C_STRING_WITH_LEN("seek")},
    {C_STRING_WITH_LEN("tell")},
    {C_STRING_WITH_LEN("flush")},
    {C_STRING_WITH_LEN("stat")},
    {C_STRING_WITH_LEN("stat")}, /* fstat */
    {C_STRING_WITH_LEN("chsize")},
    {C_STRING_WITH_LEN("delete")},
    {C_STRING_WITH_LEN("rename")},
    {C_STRING_WITH_LEN("sync")},

    /* Table I/O operations */
    {C_STRING_WITH_LEN("fetch")},
    {C_STRING_WITH_LEN("insert")}, /* write row */
    {C_STRING_WITH_LEN("update")}, /* update row */
    {C_STRING_WITH_LEN("delete")}, /* delete row */

    /* Table lock operations */
    {C_STRING_WITH_LEN("read normal")},
    {C_STRING_WITH_LEN("read with shared locks")},
    {C_STRING_WITH_LEN("read high priority")},
    {C_STRING_WITH_LEN("read no inserts")},
    {C_STRING_WITH_LEN("write allow write")},
    {C_STRING_WITH_LEN("write concurrent insert")},
    {C_STRING_WITH_LEN("write low priority")},
    {C_STRING_WITH_LEN("write normal")},
    {C_STRING_WITH_LEN("read external")},
    {C_STRING_WITH_LEN("write external")},

    /* Socket operations */
    {C_STRING_WITH_LEN("create")},
    {C_STRING_WITH_LEN("connect")},
    {C_STRING_WITH_LEN("bind")},
    {C_STRING_WITH_LEN("close")},
    {C_STRING_WITH_LEN("send")},
    {C_STRING_WITH_LEN("recv")},
    {C_STRING_WITH_LEN("sendto")},
    {C_STRING_WITH_LEN("recvfrom")},
    {C_STRING_WITH_LEN("sendmsg")},
    {C_STRING_WITH_LEN("recvmsg")},
    {C_STRING_WITH_LEN("seek")},
    {C_STRING_WITH_LEN("opt")},
    {C_STRING_WITH_LEN("stat")},
    {C_STRING_WITH_LEN("shutdown")},
    {C_STRING_WITH_LEN("select")},

    /* Idle operations */
    {C_STRING_WITH_LEN("idle")},

    /* Medatada lock operations */
    {C_STRING_WITH_LEN("metadata lock")}};

int table_events_waits_common::read_row_values(TABLE *table, unsigned char *buf,
                                               Field **fields, bool read_all) {
  Field *f;
  const LEX_STRING *operation;

  static_assert(COUNT_OPERATION_TYPE == array_elements(operation_names_map),
                "COUNT_OPERATION_TYPE needs to be the last element.");

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 2);
  buf[0] = 0;
  buf[1] = 0;

  /*
    Some columns are unreliable, because they are joined with other buffers,
    which could have changed and been reused for something else.
    These columns are:
    - THREAD_ID (m_thread joins with PFS_thread),
    - SCHEMA_NAME (m_schema_name joins with PFS_table_share)
    - OBJECT_NAME (m_object_name joins with PFS_table_share)
  */
  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* THREAD_ID */
          set_field_ulonglong(f, m_row.m_thread_internal_id);
          break;
        case 1: /* EVENT_ID */
          set_field_ulonglong(f, m_row.m_event_id);
          break;
        case 2: /* END_EVENT_ID */
          if (m_row.m_end_event_id > 0) {
            set_field_ulonglong(f, m_row.m_end_event_id - 1);
          } else {
            f->set_null();
          }
          break;
        case 3: /* EVENT_NAME */
          set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
          break;
        case 4: /* SOURCE */
          set_field_varchar_utf8(f, m_row.m_source, m_row.m_source_length);
          break;
        case 5: /* TIMER_START */
          if (m_row.m_timer_start != 0) {
            set_field_ulonglong(f, m_row.m_timer_start);
          } else {
            f->set_null();
          }
          break;
        case 6: /* TIMER_END */
          if (m_row.m_timer_end != 0) {
            set_field_ulonglong(f, m_row.m_timer_end);
          } else {
            f->set_null();
          }
          break;
        case 7: /* TIMER_WAIT */
          if (m_row.m_timer_wait != 0) {
            set_field_ulonglong(f, m_row.m_timer_wait);
          } else {
            f->set_null();
          }
          break;
        case 8: /* SPINS */
          f->set_null();
          break;
        case 9: /* OBJECT_SCHEMA */
          if (m_row.m_object_schema_length > 0) {
            set_field_varchar_utf8(f, m_row.m_object_schema,
                                   m_row.m_object_schema_length);
          } else {
            f->set_null();
          }
          break;
        case 10: /* OBJECT_NAME */
          if (m_row.m_object_name_length > 0) {
            set_field_varchar_utf8(f, m_row.m_object_name,
                                   m_row.m_object_name_length);
          } else {
            f->set_null();
          }
          break;
        case 11: /* INDEX_NAME */
          if (m_row.m_index_name_length > 0) {
            set_field_varchar_utf8(f, m_row.m_index_name,
                                   m_row.m_index_name_length);
          } else {
            f->set_null();
          }
          break;
        case 12: /* OBJECT_TYPE */
          if (m_row.m_object_type_length > 0) {
            set_field_varchar_utf8(f, m_row.m_object_type,
                                   m_row.m_object_type_length);
          } else {
            f->set_null();
          }
          break;
        case 13: /* OBJECT_INSTANCE */
          set_field_ulonglong(f, m_row.m_object_instance_addr);
          break;
        case 14: /* NESTING_EVENT_ID */
          if (m_row.m_nesting_event_id != 0) {
            set_field_ulonglong(f, m_row.m_nesting_event_id);
          } else {
            f->set_null();
          }
          break;
        case 15: /* NESTING_EVENT_TYPE */
          if (m_row.m_nesting_event_id != 0) {
            set_field_enum(f, m_row.m_nesting_event_type);
          } else {
            f->set_null();
          }
          break;
        case 16: /* OPERATION */
          operation = &operation_names_map[(int)m_row.m_operation - 1];
          set_field_varchar_utf8(f, operation->str, operation->length);
          break;
        case 17: /* NUMBER_OF_BYTES (also used for ROWS) */
          if ((m_row.m_operation == OPERATION_TYPE_FILEREAD) ||
              (m_row.m_operation == OPERATION_TYPE_FILEWRITE) ||
              (m_row.m_operation == OPERATION_TYPE_FILECHSIZE) ||
              (m_row.m_operation == OPERATION_TYPE_SOCKETSEND) ||
              (m_row.m_operation == OPERATION_TYPE_SOCKETRECV) ||
              (m_row.m_operation == OPERATION_TYPE_SOCKETSENDTO) ||
              (m_row.m_operation == OPERATION_TYPE_SOCKETRECVFROM) ||
              (m_row.m_operation == OPERATION_TYPE_TABLE_FETCH) ||
              (m_row.m_operation == OPERATION_TYPE_TABLE_WRITE_ROW) ||
              (m_row.m_operation == OPERATION_TYPE_TABLE_UPDATE_ROW) ||
              (m_row.m_operation == OPERATION_TYPE_TABLE_DELETE_ROW)) {
            set_field_ulonglong(f, m_row.m_number_of_bytes);
          } else {
            f->set_null();
          }
          break;
        case 18: /* FLAGS */
          f->set_null();
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}

PFS_engine_table *table_events_waits_current::create(PFS_engine_table_share *) {
  return new table_events_waits_current();
}

table_events_waits_current::table_events_waits_current()
    : table_events_waits_common(&m_share, &m_pos), m_pos() {}

void table_events_waits_current::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

PFS_events_waits *table_events_waits_current::get_wait(
    PFS_thread *pfs_thread, uint index_2 MY_ATTRIBUTE((unused))) {
  PFS_events_waits *wait;

  /*
    We do not show nested events for now,
    this will be revised with TABLE I/O
  */
  // #define ONLY_SHOW_ONE_WAIT

#ifdef ONLY_SHOW_ONE_WAIT
  if (index_2 >= 1) {
    return NULL;
  }
#else
  /* m_events_waits_stack[0] is a dummy record */
  PFS_events_waits *top_wait =
      &pfs_thread->m_events_waits_stack[WAIT_STACK_BOTTOM];
  wait = &pfs_thread->m_events_waits_stack[m_pos.m_index_2 + WAIT_STACK_BOTTOM];

  PFS_events_waits *safe_current = pfs_thread->m_events_waits_current;

  if (safe_current == top_wait) {
    /* Display the last top level wait, when completed */
    if (m_pos.m_index_2 >= 1) {
      return NULL;
    }
  } else {
    /* Display all pending waits, when in progress */
    if (wait >= safe_current) {
      return NULL;
    }
  }
#endif

  if (wait->m_wait_class == NO_WAIT_CLASS) {
    /*
      This locker does not exist.
      There can not be more lockers in the stack, skip to the next thread
    */
    return NULL;
  }

  return wait;
}

int table_events_waits_current::rnd_next(void) {
  PFS_thread *pfs_thread;
  PFS_events_waits *wait;
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    pfs_thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (pfs_thread != NULL) {
      wait = get_wait(pfs_thread, m_pos.m_index_2);
      if (wait != NULL) {
        /* Next iteration, look for the next locker in this thread */
        m_next_pos.set_after(&m_pos);
        return make_row(pfs_thread, wait);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_waits_current::rnd_pos(const void *pos) {
  PFS_thread *pfs_thread;
  PFS_events_waits *wait;

  set_position(pos);

  pfs_thread = global_thread_container.get(m_pos.m_index_1);
  if (pfs_thread != NULL) {
    DBUG_ASSERT(m_pos.m_index_2 < WAIT_STACK_LOGICAL_SIZE);
    wait = get_wait(pfs_thread, m_pos.m_index_2);
    if (wait != NULL) {
      return make_row(pfs_thread, wait);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_events_waits_current::index_init(uint idx MY_ATTRIBUTE((unused)),
                                           bool) {
  PFS_index_events_waits *result;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_events_waits);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_events_waits_current::index_next(void) {
  PFS_thread *pfs_thread;
  PFS_events_waits *wait;
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    pfs_thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (pfs_thread != NULL) {
      if (m_opened_index->match(pfs_thread)) {
        do {
          wait = get_wait(pfs_thread, m_pos.m_index_2);
          if (wait != NULL) {
            if (m_opened_index->match(wait)) {
              if (!make_row(pfs_thread, wait)) {
                /* Next iteration, look for the next locker in this thread */
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
            m_pos.set_after(&m_pos);
          }
        } while (wait != NULL);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_waits_current::make_row(PFS_thread *thread,
                                         PFS_events_waits *wait) {
  pfs_optimistic_state lock;

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  if (table_events_waits_common::make_row(wait)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (!thread->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_events_waits_current::delete_all_rows(void) {
  reset_events_waits_current();
  return 0;
}

ha_rows table_events_waits_current::get_row_count(void) {
  return WAIT_STACK_SIZE * global_thread_container.get_row_count();
}

PFS_engine_table *table_events_waits_history::create(PFS_engine_table_share *) {
  return new table_events_waits_history();
}

table_events_waits_history::table_events_waits_history()
    : table_events_waits_common(&m_share, &m_pos), m_pos(), m_next_pos() {}

void table_events_waits_history::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

PFS_events_waits *table_events_waits_history::get_wait(PFS_thread *pfs_thread,
                                                       uint index_2) {
  PFS_events_waits *wait;

  if (index_2 >= events_waits_history_per_thread) {
    /* This thread does not have more (full) history */
    return NULL;
  }

  if (!pfs_thread->m_waits_history_full &&
      (index_2 >= pfs_thread->m_waits_history_index)) {
    /* This thread does not have more (not full) history */
    return NULL;
  }

  wait = &pfs_thread->m_waits_history[index_2];
  if (wait->m_wait_class == NO_WAIT_CLASS) {
    return NULL;
  }

  return wait;
}

int table_events_waits_history::rnd_next(void) {
  PFS_thread *pfs_thread;
  PFS_events_waits *wait;
  bool has_more_thread = true;

  if (events_waits_history_per_thread == 0) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    pfs_thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (pfs_thread != NULL) {
      wait = get_wait(pfs_thread, m_pos.m_index_2);
      if (wait != NULL) {
        /* Next iteration, look for the next history in this thread */
        m_next_pos.set_after(&m_pos);
        return make_row(pfs_thread, wait);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_waits_history::rnd_pos(const void *pos) {
  PFS_thread *pfs_thread;
  PFS_events_waits *wait;

  DBUG_ASSERT(events_waits_history_per_thread != 0);
  set_position(pos);

  pfs_thread = global_thread_container.get(m_pos.m_index_1);
  if (pfs_thread != NULL) {
    DBUG_ASSERT(m_pos.m_index_2 < events_waits_history_per_thread);

    wait = get_wait(pfs_thread, m_pos.m_index_2);
    if (wait != NULL) {
      return make_row(pfs_thread, wait);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_events_waits_history::index_init(uint idx MY_ATTRIBUTE((unused)),
                                           bool) {
  PFS_index_events_waits *result;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_events_waits);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_events_waits_history::index_next(void) {
  PFS_thread *pfs_thread;
  PFS_events_waits *wait;
  bool has_more_thread = true;

  if (events_waits_history_per_thread == 0) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    pfs_thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (pfs_thread != NULL) {
      if (m_opened_index->match(pfs_thread)) {
        do {
          wait = get_wait(pfs_thread, m_pos.m_index_2);
          if (wait != NULL) {
            if (m_opened_index->match(wait)) {
              if (!make_row(pfs_thread, wait)) {
                /* Next iteration, look for the next history in this thread */
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
            m_pos.set_after(&m_pos);
          }
        } while (wait != NULL);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_waits_history::make_row(PFS_thread *thread,
                                         PFS_events_waits *wait) {
  pfs_optimistic_state lock;

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  if (table_events_waits_common::make_row(wait)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (!thread->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_events_waits_history::delete_all_rows(void) {
  reset_events_waits_history();
  return 0;
}

ha_rows table_events_waits_history::get_row_count(void) {
  return events_waits_history_per_thread *
         global_thread_container.get_row_count();
}

PFS_engine_table *table_events_waits_history_long::create(
    PFS_engine_table_share *) {
  return new table_events_waits_history_long();
}

table_events_waits_history_long::table_events_waits_history_long()
    : table_events_waits_common(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

void table_events_waits_history_long::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_events_waits_history_long::rnd_next(void) {
  PFS_events_waits *wait;
  uint limit;

  if (events_waits_history_long_size == 0) {
    return HA_ERR_END_OF_FILE;
  }

  if (events_waits_history_long_full) {
    limit = events_waits_history_long_size;
  } else
    limit =
        events_waits_history_long_index.m_u32 % events_waits_history_long_size;

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < limit; m_pos.next()) {
    wait = &events_waits_history_long_array[m_pos.m_index];

    if (wait->m_wait_class != NO_WAIT_CLASS) {
      /* Next iteration, look for the next entry */
      m_next_pos.set_after(&m_pos);
      return make_row(wait);
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_events_waits_history_long::rnd_pos(const void *pos) {
  PFS_events_waits *wait;
  uint limit;

  if (events_waits_history_long_size == 0) {
    return HA_ERR_RECORD_DELETED;
  }

  set_position(pos);

  if (events_waits_history_long_full) {
    limit = events_waits_history_long_size;
  } else
    limit =
        events_waits_history_long_index.m_u32 % events_waits_history_long_size;

  if (m_pos.m_index >= limit) {
    return HA_ERR_RECORD_DELETED;
  }

  wait = &events_waits_history_long_array[m_pos.m_index];

  if (wait->m_wait_class == NO_WAIT_CLASS) {
    return HA_ERR_RECORD_DELETED;
  }

  return make_row(wait);
}

int table_events_waits_history_long::delete_all_rows(void) {
  reset_events_waits_history_long();
  return 0;
}

ha_rows table_events_waits_history_long::get_row_count(void) {
  return events_waits_history_long_size;
}
