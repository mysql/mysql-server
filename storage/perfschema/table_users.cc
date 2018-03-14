/* Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_users.cc
  TABLE USERS.
*/

#include "storage/perfschema/table_users.h"

#include <stddef.h>

#include "my_dbug.h"
#include "my_thread.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_events_transactions.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_memory.h"
#include "storage/perfschema/pfs_status.h"
#include "storage/perfschema/pfs_user.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_users::m_table_lock;

Plugin_table table_users::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "users",
    /* Definition */
    "  USER CHAR(32) collate utf8_bin default null,\n"
    "  CURRENT_CONNECTIONS bigint not null,\n"
    "  TOTAL_CONNECTIONS bigint not null,\n"
    "  UNIQUE KEY (USER) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_users::m_share = {
    &pfs_truncatable_acl,
    table_users::create,
    NULL, /* write_row */
    table_users::delete_all_rows,
    cursor_by_user::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_users_by_user::match(PFS_user *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_users::create(PFS_engine_table_share *) {
  return new table_users();
}

int table_users::delete_all_rows(void) {
  reset_events_waits_by_thread();
  reset_events_waits_by_account();
  reset_events_waits_by_user();
  reset_events_stages_by_thread();
  reset_events_stages_by_account();
  reset_events_stages_by_user();
  reset_events_statements_by_thread();
  reset_events_statements_by_account();
  reset_events_statements_by_user();
  reset_events_transactions_by_thread();
  reset_events_transactions_by_account();
  reset_events_transactions_by_user();
  reset_memory_by_thread();
  reset_memory_by_account();
  reset_memory_by_user();
  reset_status_by_thread();
  reset_status_by_account();
  reset_status_by_user();
  purge_all_account();
  purge_all_user();
  return 0;
}

table_users::table_users() : cursor_by_user(&m_share) {}

int table_users::index_init(uint, bool) {
  PFS_index_users *result = NULL;
  result = PFS_NEW(PFS_index_users_by_user);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_users::make_row(PFS_user *pfs) {
  pfs_optimistic_state lock;

  pfs->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_user.make_row(pfs)) {
    return HA_ERR_RECORD_DELETED;
  }

  PFS_connection_stat_visitor visitor;
  PFS_connection_iterator::visit_user(pfs, true, /* accounts */
                                      true,      /* threads */
                                      false,     /* THDs */
                                      &visitor);

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_connection_stat.set(&visitor.m_stat);
  return 0;
}

int table_users::read_row_values(TABLE *table, unsigned char *buf,
                                 Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* USER */
          m_row.m_user.set_field(f);
          break;
        case 1: /* CURRENT_CONNECTIONS */
        case 2: /* TOTAL_CONNECTIONS */
          m_row.m_connection_stat.set_field(f->field_index - 1, f);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
