/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_error_log.cc
  TABLE ERROR_LOG (implementation for table,
  indices, and keys).

  Try

  > SELECT RIGHT(logged,15),prio,error_code,subsystem,LEFT(data,22)
      FROM performance_schema.error_log;

  > SELECT VARIABLE_NAME,VARIABLE_VALUE
      FROM performance_schema.global_status
      WHERE VARIABLE_NAME LIKE "Error_log_%";

  > SELECT logged,prio,error_code,subsystem,LEFT(data,9)
      FROM performance_schema.error_log WHERE prio="System";

  > SELECT RIGHT(logged,15),prio,error_code,subsystem,
           IF(LEFT(data,1)='{',JSON_EXTRACT(data,'$.msg'),data)
      FROM performance_schema.error_log;
*/

#include "storage/perfschema/table_error_log.h"

#include <assert.h>
#include "lex_string.h"
#include "my_compiler.h"

#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/sql_parse.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"

THR_LOCK table_error_log::m_table_lock;

// Table definition
Plugin_table table_error_log::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "error_log",
    /* Definition */
    "  LOGGED TIMESTAMP(6) NOT NULL,\n"
    "  THREAD_ID BIGINT UNSIGNED,\n"
    "  PRIO ENUM ('System', 'Error', 'Warning', 'Note') NOT NULL,\n"
    "  ERROR_CODE VARCHAR(10),\n"
    "  SUBSYSTEM VARCHAR(7),\n"
    "  DATA TEXT NOT NULL,\n"
    "  PRIMARY KEY (LOGGED) USING HASH,\n"
    "  KEY (THREAD_ID) USING HASH,\n"
    "  KEY (PRIO) USING HASH,\n"
    "  KEY (ERROR_CODE) USING HASH,\n"
    "  KEY (SUBSYSTEM) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

// Table share
PFS_engine_table_share table_error_log::m_share = {
    &pfs_readonly_acl,       /* table ACL */
    table_error_log::create, /* open_table function */
    nullptr,                 /* write_row function */
    nullptr,                 /* delete_all_rows function */
    cursor_by_error_log::get_row_count,
    sizeof(pos_t), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual -- should table exist if pfs is disabled */
    PFS_engine_table_proxy(),
    {0},  /* refcount */
    false /* m_in_purgatory */
};

// const
PFS_engine_table *table_error_log::create(PFS_engine_table_share *) {
  return new table_error_log();
}

// dest
table_error_log::table_error_log() : cursor_by_error_log(&m_share) {}

/// Match function / comparator for the key on the LOGGED column
bool PFS_key_error_log_logged::match(const log_sink_pfs_event *row) {
  return stateless_match(false, row->m_timestamp, false, m_key_value,
                         m_find_flag);
}

/// Match function for the index on the LOGGED column
bool PFS_index_error_log_by_logged::match(log_sink_pfs_event *row) {
  if (m_fields >= 1) {
    if (!m_key.match(row)) {
      return false;
    }
  }
  return true;
}

/// Match function / comparator for the key on the THREAD_ID column
bool PFS_key_error_log_thread_id::match(const log_sink_pfs_event *row) {
  return do_match(false, row->m_thread_id);
}

/// Match function for the index on the THREAD_ID column
bool PFS_index_error_log_by_thread_id::match(log_sink_pfs_event *row) {
  if (m_fields >= 1) {
    if (!m_key.match(row)) {
      return false;
    }
  }
  return true;
}

/// Match function / comparator for the key on the PRIO column
bool PFS_key_error_log_prio::match(const log_sink_pfs_event *row) {
  const auto record_value = (enum_prio)(row->m_prio + 1);
  int cmp = 0;

  if (m_is_null) {
    cmp = 1;
  } else {
    if (record_value < m_prio) {
      cmp = -1;
    } else if (record_value > m_prio) {
      cmp = +1;
    } else {
      cmp = 0;
    }
  }

  switch (m_find_flag) {
    case HA_READ_KEY_EXACT:
      return (cmp == 0);
    case HA_READ_KEY_OR_NEXT:
      return (cmp >= 0);
    case HA_READ_KEY_OR_PREV:
      return (cmp <= 0);
    case HA_READ_BEFORE_KEY:
      return (cmp < 0);
    case HA_READ_AFTER_KEY:
      return (cmp > 0);
    default:
      assert(false);
      return false;
  }
}

/// Read function for the key on the PRIO column
// Since this is an enum rather than a stock scalar, we have our own function.
void PFS_key_error_log_prio::read(PFS_key_reader &reader,
                                  enum ha_rkey_function find_flag) {
  uchar object_type = 0;

  m_find_flag = reader.read_uint8(find_flag, m_is_null, &object_type);

  if (m_is_null) {
    m_prio = PS_ERROR_LOG_PRIO_ERROR;  // default value
  } else {
    m_prio = static_cast<enum enum_prio>(object_type);
  }
}

/// Match function for the index on the PRIO column
bool PFS_index_error_log_by_prio::match(log_sink_pfs_event *row) {
  if (m_fields >= 1) {
    if (!m_key.match(row)) {
      return false;
    }
  }
  return true;
}

/// Match function for the index on the ERROR_CODE column
bool PFS_index_error_log_by_error_code::match(log_sink_pfs_event *row) {
  if (m_fields >= 1) {
    if (!m_key.match(row->m_error_code, row->m_error_code_length)) {
      return false;
    }
  }
  return true;
}

/// Match function for the index on the SUBSYSTEM column
bool PFS_index_error_log_by_subsys::match(log_sink_pfs_event *row) {
  if (m_fields >= 1) {
    if (!m_key.match(row->m_subsys, row->m_subsys_length)) {
      return false;
    }
  }
  return true;
}

/**
  Create an index on the column with the ordinal idx.

  @param  idx     ordinal of the column to create an index for
  @param  sorted  unused

  @retval 0    success
*/
int table_error_log::index_init(uint idx, bool sorted [[maybe_unused]]) {
  PFS_index_error_log *result = nullptr;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_error_log_by_logged);
      break;
    case 1:
      result = PFS_NEW(PFS_index_error_log_by_thread_id);
      break;
    case 2:
      result = PFS_NEW(PFS_index_error_log_by_prio);
      break;
    case 3:
      result = PFS_NEW(PFS_index_error_log_by_error_code);
      break;
    case 4:
      result = PFS_NEW(PFS_index_error_log_by_subsys);
      break;
    default:
      assert(false);
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

/**
  Copy a log-event from the ring-buffer into
  (the private variables of our instance of the class)
  table_error_log.

  Caller must hold a read lock on the ring-buffer.

  @param   e  the event in the ring-buffer

  @retval  0  success
*/
int table_error_log::make_row(log_sink_pfs_event *e) {
  memcpy(&m_header, e, sizeof(log_sink_pfs_event));
  /* Max message length should be the same for both, but let's play it safe. */
  const size_t len =
      std::min<size_t>(e->m_message_length, sizeof(m_message) - 1);
  m_message[len] = '\0';
  memcpy(m_message, ((const char *)e) + sizeof(log_sink_pfs_event), len);

  return 0;
}

/**
  Fill in a row's fields from internal representation
  (i.e. from the private variables in the instance of table_error_log
  that contain the current row).

  As we have previously copied the event from the ring-buffer, holding
  a read-lock on the ring-buffer is not necessary here.

  @retval 0  success
*/
int table_error_log::read_row_values(TABLE *table, unsigned char *buf,
                                     Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* LOGGED (timestamp) */
          set_field_timestamp(f, m_header.m_timestamp);
          break;
        case 1: /* THREAD_ID */
          set_field_ulonglong(f, m_header.m_thread_id);
          break;
        case 2: /* PRIO */
          set_field_enum(f, (enum_prio)(m_header.m_prio + 1));
          break;
        case 3: /* ERROR_CODE */
          if (m_header.m_error_code_length > 0) {
            set_field_varchar_utf8mb4(f, m_header.m_error_code,
                                      m_header.m_error_code_length);
          } else {
            f->set_null();
          }
          break;
        case 4: /* SUBSYSTEM */
          if (m_header.m_subsys_length > 0) {
            set_field_varchar_utf8mb4(f, m_header.m_subsys,
                                      m_header.m_subsys_length);
          } else {
            f->set_null();
          }
          break;
        case 5: /* MESSAGE */
          if (m_header.m_message_length > 0) {
            set_field_text(f, (char *)&(m_message[0]),
                           m_header.m_message_length, &my_charset_utf8mb4_bin);
          } else {
            f->set_null();
          }
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
