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
  @file storage/perfschema/table_threads.cc
  TABLE THREADS.
*/

#include "storage/perfschema/table_threads.h"

#include "lex_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_threads::m_table_lock;

Plugin_table table_threads::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "threads",
    /* Definition */
    "  THREAD_ID BIGINT unsigned not null,\n"
    "  NAME VARCHAR(128) not null,\n"
    "  TYPE VARCHAR(10) not null,\n"
    "  PROCESSLIST_ID BIGINT unsigned,\n"
    "  PROCESSLIST_USER VARCHAR(32),\n"
    "  PROCESSLIST_HOST VARCHAR(60),\n"
    "  PROCESSLIST_DB VARCHAR(64),\n"
    "  PROCESSLIST_COMMAND VARCHAR(16),\n"
    "  PROCESSLIST_TIME BIGINT,\n"
    "  PROCESSLIST_STATE VARCHAR(64),\n"
    "  PROCESSLIST_INFO LONGTEXT,\n"
    "  PARENT_THREAD_ID BIGINT unsigned,\n"
    "  `ROLE` VARCHAR(64),\n"
    "  INSTRUMENTED ENUM ('YES', 'NO') not null,\n"
    "  HISTORY ENUM ('YES', 'NO') not null,\n"
    "  CONNECTION_TYPE VARCHAR(16),\n"
    "  THREAD_OS_ID BIGINT unsigned,\n"
    "  RESOURCE_GROUP VARCHAR(64),\n"
    "  PRIMARY KEY (THREAD_ID) USING HASH,\n"
    "  KEY (PROCESSLIST_ID) USING HASH,\n"
    "  KEY (THREAD_OS_ID) USING HASH,\n"
    "  KEY (NAME) USING HASH,\n"
    "  KEY `PROCESSLIST_ACCOUNT` (PROCESSLIST_USER,\n"
    "                             PROCESSLIST_HOST) USING HASH,\n"
    "  KEY (PROCESSLIST_HOST) USING HASH,\n"
    "  KEY (RESOURCE_GROUP) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_threads::m_share = {
    &pfs_updatable_acl,
    table_threads::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    cursor_by_thread::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_threads::create(PFS_engine_table_share *) {
  return new table_threads();
}

table_threads::table_threads() : cursor_by_thread(&m_share) {}

bool PFS_index_threads_by_thread_id::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_threads_by_processlist_id::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_threads_by_name::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_threads_by_user_host::match(PFS_thread *pfs) {
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

bool PFS_index_threads_by_host::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_threads_by_thread_os_id::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_threads_by_resource_group::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

int table_threads::index_init(uint idx, bool) {
  PFS_index_threads *result = NULL;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_threads_by_thread_id);
      break;
    case 1:
      result = PFS_NEW(PFS_index_threads_by_processlist_id);
      break;
    case 2:
      result = PFS_NEW(PFS_index_threads_by_thread_os_id);
      break;
    case 3:
      result = PFS_NEW(PFS_index_threads_by_name);
      break;
    case 4:
      result = PFS_NEW(PFS_index_threads_by_user_host);
      break;
    case 5:
      result = PFS_NEW(PFS_index_threads_by_host);
      break;
    case 6:
      result = PFS_NEW(PFS_index_threads_by_resource_group);
      break;
    default:
      DBUG_ASSERT(false);
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_threads::make_row(PFS_thread *pfs) {
  pfs_optimistic_state lock;
  pfs_optimistic_state session_lock;
  pfs_optimistic_state stmt_lock;
  PFS_stage_class *stage_class;
  PFS_thread_class *safe_class;

  /* Protect this reader against thread termination */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_thread_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_thread_internal_id = pfs->m_thread_internal_id;
  m_row.m_parent_thread_internal_id = pfs->m_parent_thread_internal_id;
  m_row.m_processlist_id = pfs->m_processlist_id;
  m_row.m_thread_os_id = pfs->m_thread_os_id;
  m_row.m_name = safe_class->m_name;
  m_row.m_name_length = safe_class->m_name_length;

  /* Protect this reader against session attribute changes */
  pfs->m_session_lock.begin_optimistic_lock(&session_lock);

  m_row.m_username_length = pfs->m_username_length;
  if (unlikely(m_row.m_username_length > sizeof(m_row.m_username))) {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_username_length != 0) {
    memcpy(m_row.m_username, pfs->m_username, m_row.m_username_length);
  }

  m_row.m_hostname_length = pfs->m_hostname_length;
  if (unlikely(m_row.m_hostname_length > sizeof(m_row.m_hostname))) {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_hostname_length != 0) {
    memcpy(m_row.m_hostname, pfs->m_hostname, m_row.m_hostname_length);
  }

  m_row.m_groupname_length = pfs->m_groupname_length;
  if (unlikely(m_row.m_groupname_length > sizeof(m_row.m_groupname))) {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_groupname_length != 0) {
    memcpy(m_row.m_groupname, pfs->m_groupname, m_row.m_groupname_length);
  }

  if (!pfs->m_session_lock.end_optimistic_lock(&session_lock)) {
    /*
      One of the columns:
      - PROCESSLIST_USER
      - PROCESSLIST_HOST
      is being updated.
      Do not discard the entire row.
      Do not loop waiting for a stable value.
      Just return NULL values.
    */
    m_row.m_username_length = 0;
    m_row.m_hostname_length = 0;
  }

  /* Protect this reader against statement attributes changes */
  pfs->m_stmt_lock.begin_optimistic_lock(&stmt_lock);

  m_row.m_dbname_length = pfs->m_dbname_length;
  if (unlikely(m_row.m_dbname_length > sizeof(m_row.m_dbname))) {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_dbname_length != 0) {
    memcpy(m_row.m_dbname, pfs->m_dbname, m_row.m_dbname_length);
  }

  m_row.m_processlist_info_ptr = &pfs->m_processlist_info[0];
  m_row.m_processlist_info_length = pfs->m_processlist_info_length;

  if (!pfs->m_stmt_lock.end_optimistic_lock(&stmt_lock)) {
    /*
      One of the columns:
      - PROCESSLIST_DB
      - PROCESSLIST_INFO
      is being updated.
      Do not discard the entire row.
      Do not loop waiting for a stable value.
      Just return NULL values.
    */
    m_row.m_dbname_length = 0;
    m_row.m_processlist_info_length = 0;
  }

  /* Dirty read, sanitize the command. */
  m_row.m_command = pfs->m_command;
  if ((m_row.m_command < 0) || (m_row.m_command > COM_END)) {
    m_row.m_command = COM_END;
  }

  m_row.m_start_time = pfs->m_start_time;

  stage_class = find_stage_class(pfs->m_stage);
  if (stage_class != NULL) {
    m_row.m_processlist_state_ptr =
        stage_class->m_name + stage_class->m_prefix_length;
    m_row.m_processlist_state_length =
        stage_class->m_name_length - stage_class->m_prefix_length;
    if (m_row.m_processlist_state_length > 64) {
      /*
        Column PROCESSLIST_STATE is VARCHAR(64)
        for compatibility reasons with the historical
        INFORMATION_SCHEMA.PROCESSLIST table.
        Stages however can have longer names.
        We silently truncate data here,
        so it fits into PROCESSLIST_STATE.
      */
      m_row.m_processlist_state_length = 64;
    }
  } else {
    m_row.m_processlist_state_length = 0;
  }
  m_row.m_connection_type = pfs->m_connection_type;

  m_row.m_enabled = pfs->m_enabled;
  m_row.m_history = pfs->m_history;
  m_row.m_psi = pfs;

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_threads::read_row_values(TABLE *table, unsigned char *buf,
                                   Field **fields, bool read_all) {
  Field *f;
  const char *str = NULL;
  int len = 0;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 2);
  buf[0] = 0;
  buf[1] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* THREAD_ID */
          set_field_ulonglong(f, m_row.m_thread_internal_id);
          break;
        case 1: /* NAME */
          set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
          break;
        case 2: /* TYPE */
          if (m_row.m_processlist_id != 0) {
            set_field_varchar_utf8(f, "FOREGROUND", 10);
          } else {
            set_field_varchar_utf8(f, "BACKGROUND", 10);
          }
          break;
        case 3: /* PROCESSLIST_ID */
          if (m_row.m_processlist_id != 0) {
            set_field_ulonglong(f, m_row.m_processlist_id);
          } else {
            f->set_null();
          }
          break;
        case 4: /* PROCESSLIST_USER */
          if (m_row.m_username_length > 0) {
            set_field_varchar_utf8(f, m_row.m_username,
                                   m_row.m_username_length);
          } else {
            f->set_null();
          }
          break;
        case 5: /* PROCESSLIST_HOST */
          if (m_row.m_hostname_length > 0) {
            set_field_varchar_utf8(f, m_row.m_hostname,
                                   m_row.m_hostname_length);
          } else {
            f->set_null();
          }
          break;
        case 6: /* PROCESSLIST_DB */
          if (m_row.m_dbname_length > 0) {
            set_field_varchar_utf8(f, m_row.m_dbname, m_row.m_dbname_length);
          } else {
            f->set_null();
          }
          break;
        case 7: /* PROCESSLIST_COMMAND */
          if (m_row.m_processlist_id != 0)
            set_field_varchar_utf8(f, command_name[m_row.m_command].str,
                                   (uint)command_name[m_row.m_command].length);
          else {
            f->set_null();
          }
          break;
        case 8: /* PROCESSLIST_TIME */
          if (m_row.m_start_time) {
            time_t now = my_time(0);
            ulonglong elapsed =
                (now > m_row.m_start_time ? now - m_row.m_start_time : 0);
            set_field_ulonglong(f, elapsed);
          } else {
            f->set_null();
          }
          break;
        case 9: /* PROCESSLIST_STATE */
          if (m_row.m_processlist_state_length > 0) {
            set_field_varchar_utf8(f, m_row.m_processlist_state_ptr,
                                   m_row.m_processlist_state_length);
          } else {
            f->set_null();
          }
          break;
        case 10: /* PROCESSLIST_INFO */
          if (m_row.m_processlist_info_length > 0)
            set_field_blob(f, m_row.m_processlist_info_ptr,
                           m_row.m_processlist_info_length);
          else {
            f->set_null();
          }
          break;
        case 11: /* PARENT_THREAD_ID */
          if (m_row.m_parent_thread_internal_id != 0) {
            set_field_ulonglong(f, m_row.m_parent_thread_internal_id);
          } else {
            f->set_null();
          }
          break;
        case 12: /* ROLE */
          f->set_null();
          break;
        case 13: /* INSTRUMENTED */
          set_field_enum(f, m_row.m_enabled ? ENUM_YES : ENUM_NO);
          break;
        case 14: /* HISTORY */
          set_field_enum(f, m_row.m_history ? ENUM_YES : ENUM_NO);
          break;
        case 15: /* CONNECTION_TYPE */
          get_vio_type_name(m_row.m_connection_type, &str, &len);
          if (len > 0) {
            set_field_varchar_utf8(f, str, len);
          } else {
            f->set_null();
          }
          break;
        case 16: /* THREAD_OS_ID */
          if (m_row.m_thread_os_id > 0) {
            set_field_ulonglong(f, m_row.m_thread_os_id);
          } else {
            f->set_null();
          }
          break;
        case 17: /* RESOURCE_GROUP */
          if (m_row.m_groupname_length > 0) {
            set_field_varchar_utf8(f, m_row.m_groupname,
                                   m_row.m_groupname_length);
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

int table_threads::update_row_values(TABLE *table, const unsigned char *,
                                     unsigned char *, Field **fields) {
  Field *f;
  enum_yes_no value;

  for (; (f = *fields); fields++) {
    if (bitmap_is_set(table->write_set, f->field_index)) {
      switch (f->field_index) {
        case 0:  /* THREAD_ID */
        case 1:  /* NAME */
        case 2:  /* TYPE */
        case 3:  /* PROCESSLIST_ID */
        case 4:  /* PROCESSLIST_USER */
        case 5:  /* PROCESSLIST_HOST */
        case 6:  /* PROCESSLIST_DB */
        case 7:  /* PROCESSLIST_COMMAND */
        case 8:  /* PROCESSLIST_TIME */
        case 9:  /* PROCESSLIST_STATE */
        case 10: /* PROCESSLIST_INFO */
        case 11: /* PARENT_THREAD_ID */
        case 12: /* ROLE */
          return HA_ERR_WRONG_COMMAND;
        case 13: /* INSTRUMENTED */
          value = (enum_yes_no)get_field_enum(f);
          m_row.m_psi->set_enabled((value == ENUM_YES) ? true : false);
          break;
        case 14: /* HISTORY */
          value = (enum_yes_no)get_field_enum(f);
          m_row.m_psi->set_history((value == ENUM_YES) ? true : false);
          break;
        case 15: /* CONNECTION_TYPE */
        case 16: /* THREAD_OS_ID */
          return HA_ERR_WRONG_COMMAND;
        case 17: /* RESOURCE_GROUP */
          return HA_ERR_WRONG_COMMAND;
        default:
          DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
