/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

#include "lex_string.h"
#include "my_thread.h"
#include "sql/sql_class.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_events_transactions.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/unittest/stub_digest.h"
#include "storage/perfschema/unittest/stub_pfs_global.h"
#include "storage/perfschema/unittest/stub_pfs_plugin_table.h"
#include "storage/perfschema/unittest/stub_pfs_tls_channel.h"
#include "storage/perfschema/unittest/stub_server_telemetry.h"
#include "storage/perfschema/unittest/stub_telemetry_metrics.h"
#include "unittest/mytap/tap.h"

static void test_oom() {
  int rc;
  PFS_global_param param;
  TABLE_SHARE table_share;
  PFS_thread pfs_thread;
  PFS_table_share *pfs_table_share;

  rc = init_sync_class(1000, 0, 0);
  ok(rc == 1, "oom (mutex)");
  rc = init_sync_class(0, 1000, 0);
  ok(rc == 1, "oom (rwlock)");
  rc = init_sync_class(0, 0, 1000);
  ok(rc == 1, "oom (cond)");
  rc = init_thread_class(1000);
  ok(rc == 1, "oom (thread)");
  rc = init_file_class(1000);
  ok(rc == 1, "oom (file)");
  rc = init_socket_class(1000);
  ok(rc == 1, "oom (socket)");
  rc = init_stage_class(1000);
  ok(rc == 1, "oom (stage)");
  rc = init_statement_class(1000);
  ok(rc == 1, "oom (statement)");
  rc = init_memory_class(1000);
  ok(rc == 1, "oom (memory)");

  cleanup_sync_class();
  cleanup_thread_class();
  cleanup_file_class();
  cleanup_table_share();
  cleanup_socket_class();
  cleanup_stage_class();
  cleanup_statement_class();
  cleanup_memory_class();

  /* Table share classes. */
  memset(&param, 0, sizeof(param));
  param.m_enabled = true;
  param.m_table_share_sizing = 100;
  param.m_setup_object_sizing = 100;

  pfs_thread.m_table_share_hash_pins = nullptr;
  pfs_thread.m_setup_object_hash_pins = nullptr;

  char db_name[] = "schema 1";
  char table_name[] = "table 1";
  table_share.db.str = db_name;
  table_share.db.length = strlen(db_name);
  table_share.table_name.str = table_name;
  table_share.table_name.length = strlen(table_name);

  init_table_share(param.m_table_share_sizing);
  init_table_share_hash(&param);
  init_setup_object_hash(&param);

  stub_alloc_always_fails = false;
  pfs_table_share =
      find_or_create_table_share(&pfs_thread, false, &table_share);
  ok(pfs_table_share == nullptr, "oom (pfs table share)");
  ok(global_table_share_container.m_lost == 1, "oom (table share)");

  cleanup_table_share();
  cleanup_table_share_hash();
  cleanup_setup_object_hash();
}

static void do_all_tests() { test_oom(); }

int main(int, char **) {
  plan(11);
  MY_INIT("pfs_instr_info-oom-t");
  do_all_tests();
  my_end(0);
  return (exit_status());
}
