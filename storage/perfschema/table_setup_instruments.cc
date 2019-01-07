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
  @file storage/perfschema/table_setup_instruments.cc
  Table SETUP_INSTRUMENTS (implementation).
*/

#include "storage/perfschema/table_setup_instruments.h"

#include <stddef.h>

#include "my_dbug.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_setup_object.h"

THR_LOCK table_setup_instruments::m_table_lock;

Plugin_table table_setup_instruments::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "setup_instruments",
    /* Definition */
    "  NAME VARCHAR(128) not null,\n"
    "  ENABLED ENUM ('YES', 'NO') not null,\n"
    "  TIMED ENUM ('YES', 'NO'),\n"
    "  PROPERTIES SET('singleton', 'progress', 'user', 'global_statistics', "
    "'mutable') not null,\n"
    "  VOLATILITY int not null,\n"
    "  DOCUMENTATION LONGTEXT,\n"
    "  PRIMARY KEY (NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_setup_instruments::m_share = {
    &pfs_updatable_acl,
    table_setup_instruments::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    table_setup_instruments::get_row_count,
    sizeof(pos_setup_instruments),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_setup_instruments::match_view(uint view) {
  if (m_fields >= 1) {
    return m_key.match_view(view);
  }
  return true;
}

bool PFS_index_setup_instruments::match(PFS_instr_class *klass) {
  if (m_fields >= 1) {
    return m_key.match(klass);
  }

  return true;
}

PFS_engine_table *table_setup_instruments::create(PFS_engine_table_share *) {
  return new table_setup_instruments();
}

ha_rows table_setup_instruments::get_row_count(void) {
  return wait_class_max + stage_class_max + statement_class_max +
         transaction_class_max + memory_class_max + error_class_max;
}

table_setup_instruments::table_setup_instruments()
    : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos() {}

void table_setup_instruments::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

int table_setup_instruments::rnd_next(void) {
  PFS_instr_class *instr_class = NULL;
  PFS_builtin_memory_class *pfs_builtin;
  bool update_enabled;

  /* Do not advertise hard coded instruments when disabled. */
  if (!pfs_initialized) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_view(); m_pos.next_view()) {
    update_enabled = true;

    switch (m_pos.m_index_1) {
      case pos_setup_instruments::VIEW_MUTEX:
        instr_class = find_mutex_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_RWLOCK:
        instr_class = find_rwlock_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_COND:
        instr_class = find_cond_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_FILE:
        instr_class = find_file_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_TABLE:
        instr_class = find_table_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_STAGE:
        instr_class = find_stage_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_STATEMENT:
        instr_class = find_statement_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_TRANSACTION:
        instr_class = find_transaction_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_SOCKET:
        instr_class = find_socket_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_IDLE:
        instr_class = find_idle_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_BUILTIN_MEMORY:
        update_enabled = false;
        pfs_builtin = find_builtin_memory_class(m_pos.m_index_2);
        if (pfs_builtin != NULL) {
          instr_class = &pfs_builtin->m_class;
        } else {
          instr_class = NULL;
        }
        break;
      case pos_setup_instruments::VIEW_MEMORY:
        instr_class = find_memory_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_METADATA:
        instr_class = find_metadata_class(m_pos.m_index_2);
        break;
      case pos_setup_instruments::VIEW_ERROR:
        instr_class = find_error_class(m_pos.m_index_2);
        break;
    }

    if (instr_class) {
      m_next_pos.set_after(&m_pos);
      return make_row(instr_class, update_enabled);
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_instruments::rnd_pos(const void *pos) {
  PFS_instr_class *instr_class = NULL;
  PFS_builtin_memory_class *pfs_builtin;
  bool update_enabled;

  /* Do not advertise hard coded instruments when disabled. */
  if (!pfs_initialized) {
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);

  update_enabled = true;

  switch (m_pos.m_index_1) {
    case pos_setup_instruments::VIEW_MUTEX:
      instr_class = find_mutex_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_RWLOCK:
      instr_class = find_rwlock_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_COND:
      instr_class = find_cond_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_FILE:
      instr_class = find_file_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_TABLE:
      instr_class = find_table_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_STAGE:
      instr_class = find_stage_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_STATEMENT:
      instr_class = find_statement_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_TRANSACTION:
      instr_class = find_transaction_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_SOCKET:
      instr_class = find_socket_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_IDLE:
      instr_class = find_idle_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_BUILTIN_MEMORY:
      update_enabled = false;
      pfs_builtin = find_builtin_memory_class(m_pos.m_index_2);
      if (pfs_builtin != NULL) {
        instr_class = &pfs_builtin->m_class;
      } else {
        instr_class = NULL;
      }
      break;
    case pos_setup_instruments::VIEW_MEMORY:
      instr_class = find_memory_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_METADATA:
      instr_class = find_metadata_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_ERROR:
      instr_class = find_error_class(m_pos.m_index_2);
      break;
  }

  if (instr_class) {
    return make_row(instr_class, update_enabled);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_setup_instruments::index_init(uint idx MY_ATTRIBUTE((unused)), bool) {
  PFS_index_setup_instruments *result;

  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_setup_instruments);

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_setup_instruments::index_next(void) {
  PFS_instr_class *instr_class = NULL;
  PFS_builtin_memory_class *pfs_builtin;
  bool update_enabled;

  /* Do not advertise hard coded instruments when disabled. */
  if (!pfs_initialized) {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_view(); m_pos.next_view()) {
    if (!m_opened_index->match_view(m_pos.m_index_1)) {
      continue;
    }

    do {
      update_enabled = true;

      switch (m_pos.m_index_1) {
        case pos_setup_instruments::VIEW_MUTEX:
          instr_class = find_mutex_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_RWLOCK:
          instr_class = find_rwlock_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_COND:
          instr_class = find_cond_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_FILE:
          instr_class = find_file_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_TABLE:
          instr_class = find_table_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_STAGE:
          instr_class = find_stage_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_STATEMENT:
          instr_class = find_statement_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_TRANSACTION:
          instr_class = find_transaction_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_SOCKET:
          instr_class = find_socket_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_IDLE:
          instr_class = find_idle_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_BUILTIN_MEMORY:
          update_enabled = false;
          pfs_builtin = find_builtin_memory_class(m_pos.m_index_2);
          if (pfs_builtin != NULL) {
            instr_class = &pfs_builtin->m_class;
          } else {
            instr_class = NULL;
          }
          break;
        case pos_setup_instruments::VIEW_MEMORY:
          instr_class = find_memory_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_METADATA:
          instr_class = find_metadata_class(m_pos.m_index_2);
          break;
        case pos_setup_instruments::VIEW_ERROR:
          instr_class = find_error_class(m_pos.m_index_2);
          break;
      }

      if (instr_class) {
        if (m_opened_index->match(instr_class)) {
          if (!make_row(instr_class, update_enabled)) {
            m_next_pos.set_after(&m_pos);
            return 0;
          }
        }
        m_pos.m_index_2++;
      }
    } while (instr_class != NULL);
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_instruments::make_row(PFS_instr_class *klass,
                                      bool update_enabled) {
  m_row.m_instr_class = klass;
  m_row.m_update_enabled = update_enabled;
  m_row.m_update_timed = klass->can_be_timed();

  return 0;
}

int table_setup_instruments::read_row_values(TABLE *table, unsigned char *buf,
                                             Field **fields, bool read_all) {
  Field *f;
  const char *doc;
  uint properties;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  /*
    The row always exist, the instrument classes
    are static and never disappear.
  */

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* NAME */
          set_field_varchar_utf8(f, m_row.m_instr_class->m_name,
                                 m_row.m_instr_class->m_name_length);
          break;
        case 1: /* ENABLED */
          set_field_enum(f,
                         m_row.m_instr_class->m_enabled ? ENUM_YES : ENUM_NO);
          break;
        case 2: /* TIMED */
          if (m_row.m_update_timed) {
            set_field_enum(f,
                           m_row.m_instr_class->m_timed ? ENUM_YES : ENUM_NO);
          } else {
            f->set_null();
          }
          break;
        case 3: /* PROPERTIES */
          properties = 0;
          if (m_row.m_instr_class->is_singleton()) {
            properties |= INSTR_PROPERTIES_SET_SINGLETON;
          }
          if (m_row.m_instr_class->is_mutable()) {
            properties |= INSTR_PROPERTIES_SET_MUTABLE;
          }
          if (m_row.m_instr_class->is_progress()) {
            properties |= INSTR_PROPERTIES_SET_PROGRESS;
          }
          if (m_row.m_instr_class->is_user()) {
            properties |= INSTR_PROPERTIES_SET_USER;
          }
          if (m_row.m_instr_class->is_global()) {
            properties |= INSTR_PROPERTIES_SET_GLOBAL_STAT;
          }
          set_field_set(f, properties);
          break;
        case 4: /* VOLATILITY */
          set_field_ulong(f, m_row.m_instr_class->m_volatility);
          break;
        case 5: /* DOCUMENTATION */
          doc = m_row.m_instr_class->m_documentation;
          if (doc != NULL) {
            set_field_blob(f, doc, strlen(doc));
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

int table_setup_instruments::update_row_values(TABLE *table,
                                               const unsigned char *,
                                               unsigned char *,
                                               Field **fields) {
  Field *f;
  enum_yes_no value;

  for (; (f = *fields); fields++) {
    if (bitmap_is_set(table->write_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* NAME */
          return HA_ERR_WRONG_COMMAND;
        case 1: /* ENABLED */
          /* Do not raise error if m_update_enabled is false, silently ignore.
           */
          if (m_row.m_update_enabled) {
            value = (enum_yes_no)get_field_enum(f);
            m_row.m_instr_class->m_enabled = (value == ENUM_YES) ? true : false;
          }
          break;
        case 2: /* TIMED */
          /* Do not raise error if m_update_timed is false, silently ignore. */
          if (m_row.m_update_timed) {
            value = (enum_yes_no)get_field_enum(f);
            m_row.m_instr_class->m_timed = (value == ENUM_YES) ? true : false;
          }
          break;
        case 3: /* PROPERTIES */
        case 4: /* VOLATILITY */
        case 5: /* DOCUMENTATION */
          return HA_ERR_WRONG_COMMAND;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  switch (m_pos.m_index_1) {
    case pos_setup_instruments::VIEW_MUTEX:
      update_mutex_derived_flags();
      break;
    case pos_setup_instruments::VIEW_RWLOCK:
      update_rwlock_derived_flags();
      break;
    case pos_setup_instruments::VIEW_COND:
      update_cond_derived_flags();
      break;
    case pos_setup_instruments::VIEW_FILE:
      update_file_derived_flags();
      break;
    case pos_setup_instruments::VIEW_TABLE:
      update_table_derived_flags();
      break;
    case pos_setup_instruments::VIEW_STAGE:
    case pos_setup_instruments::VIEW_STATEMENT:
    case pos_setup_instruments::VIEW_TRANSACTION:
      /* No flag to update. */
      break;
    case pos_setup_instruments::VIEW_SOCKET:
      update_socket_derived_flags();
      break;
    case pos_setup_instruments::VIEW_IDLE:
      /* No flag to update. */
      break;
    case pos_setup_instruments::VIEW_BUILTIN_MEMORY:
    case pos_setup_instruments::VIEW_MEMORY:
      /* No flag to update. */
      break;
    case pos_setup_instruments::VIEW_METADATA:
      update_metadata_derived_flags();
      break;
    case pos_setup_instruments::VIEW_ERROR:
      /* No flag to update. */
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }

  return 0;
}
