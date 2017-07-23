/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <my_global.h>
#include <my_thread.h>
#include <pfs_instr_class.h>
#include <pfs_instr.h>
#include <pfs_global.h>
#include <tap.h>
#include <sql_class.h>
#include <pfs_buffer_container.h>

#include "stub_pfs_global.h"
#include "stub_global_status_var.h"

void test_oom()
{
  int rc;
  PFS_global_param param;
  TABLE_SHARE table_share;
  PFS_thread pfs_thread;
  PFS_table_share *pfs_table_share;

  rc= init_sync_class(1000, 0, 0);
  ok(rc == 1, "oom (mutex)");
  rc= init_sync_class(0, 1000, 0);
  ok(rc == 1, "oom (rwlock)");
  rc= init_sync_class(0, 0, 1000);
  ok(rc == 1, "oom (cond)");
  rc= init_thread_class(1000);
  ok(rc == 1, "oom (thread)");
  rc= init_file_class(1000);
  ok(rc == 1, "oom (file)");
  rc= init_socket_class(1000);
  ok(rc == 1, "oom (socket)");
  rc= init_stage_class(1000);
  ok(rc == 1, "oom (stage)");
  rc= init_statement_class(1000);
  ok(rc == 1, "oom (statement)");
  rc= init_memory_class(1000);
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
  param.m_enabled= true;
  param.m_table_share_sizing= 100;
  param.m_setup_object_sizing= 100;

  pfs_thread.m_table_share_hash_pins= NULL;
  pfs_thread.m_setup_object_hash_pins= NULL;
  
  char db_name[]= "schema 1";
  char table_name[]= "table 1";
  table_share.db.str= db_name;
  table_share.db.length= strlen(db_name);
  table_share.table_name.str= table_name;
  table_share.table_name.length= strlen(table_name);

  init_table_share(param.m_table_share_sizing);
  init_table_share_hash(&param);
  init_setup_object_hash(&param);

  stub_alloc_always_fails= false;
  pfs_table_share= find_or_create_table_share(&pfs_thread, false, &table_share);
  ok(pfs_table_share == NULL, "oom (pfs table share)");
  ok(global_table_share_container.m_lost == 1, "oom (table share)");

  cleanup_table_share();
  cleanup_table_share_hash();
  cleanup_setup_object_hash();
}

void do_all_tests()
{
  test_oom();
}

int main(int, char **)
{
  plan(11);
  MY_INIT("pfs_instr_info-oom-t");
  do_all_tests();
  return (exit_status());
}

