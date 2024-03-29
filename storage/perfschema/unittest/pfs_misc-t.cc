/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include <memory.h>

#include "my_thread.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_stat.h"
#include "storage/perfschema/unittest/stub_digest.h"
#include "storage/perfschema/unittest/stub_pfs_plugin_table.h"
#include "unittest/mytap/tap.h"

static void test_digest_length_overflow() {
  if (sizeof(size_t) != 4) {
    skip(3, "digest length overflow requires a 32-bit environment");
    return;
  }

  PFS_global_param param;
  memset(&param, 0, sizeof(param));
  param.m_enabled = true;
  /*
     Force 32-bit arithmetic overflow using the digest memory allocation
     parameters. The Performance Schema should detect the overflow, free
     allocated memory and abort initialization with a warning.
  */

  /* Max digest length, events_statements_history_long. */
  param.m_events_statements_history_long_sizing = 10000;
  param.m_digest_sizing = 1000;
  param.m_max_digest_length = (1024 * 1024);
  param.m_max_sql_text_length = 0;
  pfs_max_digest_length = param.m_max_digest_length;
  pfs_max_sqltext = param.m_max_sql_text_length;

  int rc = init_events_statements_history_long(
      param.m_events_statements_history_long_sizing);
  ok(rc == 1, "digest length overflow (init_events_statements_history_long");

  /* Max sql text length, events_statements_history_long. */
  param.m_max_sql_text_length = (1024 * 1024);
  param.m_max_digest_length = 0;
  pfs_max_digest_length = param.m_max_digest_length;
  pfs_max_sqltext = param.m_max_sql_text_length;

  rc = init_events_statements_history_long(
      param.m_events_statements_history_long_sizing);
  ok(rc == 1, "sql text length overflow (init_events_statements_history_long");

  /* Max digest length, events_statements_summary_by_digest. */
  param.m_max_digest_length = (1024 * 1024);
  param.m_digest_sizing = 10000;
  pfs_max_digest_length = param.m_max_digest_length;
  pfs_max_sqltext = param.m_max_sql_text_length;

  rc = init_digest(&param);
  ok(rc == 1, "digest length overflow (init_digest)");
}

static void do_all_tests() { test_digest_length_overflow(); }

int main(int, char **) {
  plan(3);
  MY_INIT("pfs_misc-t");
  do_all_tests();
  my_end(0);
  return (exit_status());
}
