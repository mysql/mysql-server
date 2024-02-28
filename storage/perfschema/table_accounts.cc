/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_accounts.cc
  TABLE ACCOUNTS.
*/

#include "storage/perfschema/table_accounts.h"

#include <assert.h>
#include <stddef.h>

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
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_accounts::m_table_lock;

Plugin_table table_accounts::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "accounts",
    /* Definition */
    "  USER CHAR(32) collate utf8mb4_bin default null,\n"
    "  HOST CHAR(255) CHARACTER SET ASCII default null,\n"
    "  CURRENT_CONNECTIONS bigint not null,\n"
    "  TOTAL_CONNECTIONS bigint not null,\n"
    "  MAX_SESSION_CONTROLLED_MEMORY BIGINT unsigned not null,\n"
    "  MAX_SESSION_TOTAL_MEMORY BIGINT unsigned not null,\n"
    "  UNIQUE KEY `ACCOUNT` (USER, HOST) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_accounts::m_share = {
    &pfs_truncatable_acl,
    table_accounts::create,
    nullptr, /* write_row */
    table_accounts::delete_all_rows,
    cursor_by_account::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    false /* perpetual */,
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_accounts_by_user_host::match(PFS_account *pfs) {
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

PFS_engine_table *table_accounts::create(PFS_engine_table_share *) {
  return new table_accounts();
}

int table_accounts::delete_all_rows() {
  reset_events_waits_by_thread();
  reset_events_waits_by_account();
  reset_events_stages_by_thread();
  reset_events_stages_by_account();
  reset_events_statements_by_thread();
  reset_events_statements_by_account();
  reset_events_transactions_by_thread();
  reset_events_transactions_by_account();
  reset_memory_by_thread();
  reset_memory_by_account();
  reset_status_by_thread();
  reset_status_by_account();
  purge_all_account();
  return 0;
}

table_accounts::table_accounts() : cursor_by_account(&m_share) {}

int table_accounts::index_init(uint, bool) {
  PFS_index_accounts *result = PFS_NEW(PFS_index_accounts_by_user_host);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_accounts::make_row(PFS_account *pfs) {
  pfs_optimistic_state lock;

  pfs->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_account.make_row(pfs)) {
    return HA_ERR_RECORD_DELETED;
  }

  PFS_connection_stat_visitor visitor;
  PFS_connection_iterator::visit_account(pfs, true, /* threads */
                                         false,     /* THDs */
                                         &visitor);

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_connection_stat.set(&visitor.m_stat);
  return 0;
}

int table_accounts::read_row_values(TABLE *table, unsigned char *buf,
                                    Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* USER */
        case 1: /* HOST */
          m_row.m_account.set_nullable_field(f->field_index(), f);
          break;
        case 2: /* CURRENT_CONNECTIONS */
        case 3: /* TOTAL_CONNECTIONS */
        case 4: /* MAX_SESSION_CONTROLLED_MEMORY */
        case 5: /* MAX_SESSION_TOTAL_MEMORY */
          m_row.m_connection_stat.set_field(f->field_index() - 2, f);
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
