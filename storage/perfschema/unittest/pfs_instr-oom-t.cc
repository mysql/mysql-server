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
#include <pfs_instr.h>
#include <pfs_stat.h>
#include <pfs_global.h>
#include <tap.h>

#include "stub_pfs_global.h"
#include "stub_server_misc.h"

void test_oom()
{
  int rc;
  PFS_global_param param;

  stub_alloc_always_fails= true;

  param.m_enabled= true;
  param.m_mutex_class_sizing= 10;
  param.m_rwlock_class_sizing= 0;
  param.m_cond_class_sizing= 0;
  param.m_thread_class_sizing= 0;
  param.m_table_share_sizing= 0;
  param.m_file_class_sizing= 0;
  param.m_mutex_sizing= 1000;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 0;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;

  rc= init_instruments(& param);
  ok(rc == 1, "oom (mutex)");

  param.m_enabled= true;
  param.m_mutex_class_sizing= 0;
  param.m_rwlock_class_sizing= 10;
  param.m_cond_class_sizing= 0;
  param.m_thread_class_sizing= 0;
  param.m_table_share_sizing= 0;
  param.m_file_class_sizing= 0;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 1000;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 0;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;

  rc= init_instruments(& param);
  ok(rc == 1, "oom (rwlock)");

  param.m_enabled= true;
  param.m_mutex_class_sizing= 0;
  param.m_rwlock_class_sizing= 0;
  param.m_cond_class_sizing= 10;
  param.m_thread_class_sizing= 0;
  param.m_table_share_sizing= 0;
  param.m_file_class_sizing= 0;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 1000;
  param.m_thread_sizing= 0;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;

  rc= init_instruments(& param);
  ok(rc == 1, "oom (cond)");

  param.m_enabled= true;
  param.m_mutex_class_sizing= 0;
  param.m_rwlock_class_sizing= 0;
  param.m_cond_class_sizing= 0;
  param.m_thread_class_sizing= 0;
  param.m_table_share_sizing= 0;
  param.m_file_class_sizing= 10;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 0;
  param.m_table_sizing= 0;
  param.m_file_sizing= 1000;
  param.m_file_handle_sizing= 1000;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;

  rc= init_instruments(& param);
  ok(rc == 1, "oom (file)");

  param.m_enabled= true;
  param.m_mutex_class_sizing= 0;
  param.m_rwlock_class_sizing= 0;
  param.m_cond_class_sizing= 0;
  param.m_thread_class_sizing= 0;
  param.m_table_share_sizing= 10;
  param.m_file_class_sizing= 0;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 0;
  param.m_table_sizing= 1000;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;

  rc= init_instruments(& param);
  ok(rc == 1, "oom (table)");

  param.m_enabled= true;
  param.m_mutex_class_sizing= 0;
  param.m_rwlock_class_sizing= 0;
  param.m_cond_class_sizing= 0;
  param.m_thread_class_sizing= 10;
  param.m_table_share_sizing= 0;
  param.m_file_class_sizing= 0;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 1000;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;

  rc= init_instruments(& param);
  ok(rc == 1, "oom (thread)");

  stub_alloc_always_fails= false;

  param.m_enabled= true;
  param.m_mutex_class_sizing= 0;
  param.m_rwlock_class_sizing= 0;
  param.m_cond_class_sizing= 0;
  param.m_thread_class_sizing= 10;
  param.m_table_share_sizing= 0;
  param.m_file_class_sizing= 0;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 1000;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 10;
  param.m_events_waits_history_long_sizing= 0;

  stub_alloc_fails_after_count= 2;
  rc= init_instruments(& param);
  ok(rc == 1, "oom (thread history sizing)");

  param.m_enabled= true;
  param.m_mutex_class_sizing= 50;
  param.m_rwlock_class_sizing= 50;
  param.m_cond_class_sizing= 50;
  param.m_thread_class_sizing= 10;
  param.m_table_share_sizing= 0;
  param.m_file_class_sizing= 50;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 1000;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;

  stub_alloc_fails_after_count= 2;
  rc= init_instruments(& param);
  ok(rc == 1, "oom (per thread wait)");

  cleanup_instruments();
}

void do_all_tests()
{
  PFS_atomic::init();

  test_oom();

  PFS_atomic::cleanup();
}

int main(int, char **)
{
  plan(8);
  MY_INIT("pfs_instr-oom-t");
  do_all_tests();
  return 0;
}

