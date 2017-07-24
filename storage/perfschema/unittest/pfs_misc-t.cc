/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <pfs_instr.h>
#include <pfs_stat.h>
#include <pfs_global.h>
#include <pfs_instr_class.h>
#include <pfs_buffer_container.h>
#include <tap.h>

#include "stub_global_status_var.h"

#include <memory.h>

void test_digest_length_overflow()
{
  if (sizeof(size_t) != 4)
  {
    skip(3, "digest length overflow requires a 32-bit environment");
    return;
  }

  PFS_global_param param;
  memset(&param, 0, sizeof(param));
  param.m_enabled= true;
  /*
     Force 32-bit arithmetic overflow using the digest memory allocation
     parameters. The Performance Schema should detect the overflow, free
     allocated memory and abort initialization with a warning.
  */

  /* Max digest length, events_statements_history_long. */
  param.m_events_statements_history_long_sizing= 10000;
  param.m_digest_sizing= 1000;
  param.m_max_digest_length= (1024 * 1024);
  param.m_max_sql_text_length= 0;
  pfs_max_digest_length= param.m_max_digest_length;
  pfs_max_sqltext= param.m_max_sql_text_length;

  int rc = init_events_statements_history_long(param.m_events_statements_history_long_sizing);
  ok(rc == 1, "digest length overflow (init_events_statements_history_long");

  /* Max sql text length, events_statements_history_long. */
  param.m_max_sql_text_length= (1024 * 1024);
  param.m_max_digest_length= 0;
  pfs_max_digest_length= param.m_max_digest_length;
  pfs_max_sqltext= param.m_max_sql_text_length;

  rc = init_events_statements_history_long(param.m_events_statements_history_long_sizing);
  ok(rc == 1, "sql text length overflow (init_events_statements_history_long");

  /* Max digest length, events_statements_summary_by_digest. */
  param.m_max_digest_length= (1024 * 1024);
  param.m_digest_sizing= 10000;
  pfs_max_digest_length= param.m_max_digest_length;
  pfs_max_sqltext= param.m_max_sql_text_length;

  rc = init_digest(&param);
  ok(rc == 1, "digest length overflow (init_digest)");
}

void do_all_tests()
{
  test_digest_length_overflow();
}

int main(int, char **)
{
  plan(3);
  MY_INIT("pfs_misc-t");
  do_all_tests();
  return (exit_status());
}
