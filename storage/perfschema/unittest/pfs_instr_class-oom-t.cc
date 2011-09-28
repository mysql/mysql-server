/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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
#include <my_pthread.h>
#include <pfs_instr_class.h>
#include <pfs_global.h>
#include <tap.h>

#include "stub_pfs_global.h"
#include "stub_server_misc.h"

void test_oom()
{
  int rc;

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
  rc= init_table_share(1000);
  ok(rc == 1, "oom (cond)");
  rc= init_socket_class(1000);
  ok(rc == 1, "oom (socket)");
  rc= init_stage_class(1000);
  ok(rc == 1, "oom (stage)");
  rc= init_statement_class(1000);
  ok(rc == 1, "oom (statement)");

  cleanup_sync_class();
  cleanup_thread_class();
  cleanup_file_class();
  cleanup_table_share();
  cleanup_socket_class();
  cleanup_stage_class();
  cleanup_statement_class();
}

void do_all_tests()
{
  PFS_atomic::init();

  test_oom();

  PFS_atomic::cleanup();
}

int main(int, char **)
{
  plan(9);
  MY_INIT("pfs_instr_info-oom-t");
  do_all_tests();
  return 0;
}

