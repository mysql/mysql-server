/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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

#include "storage/perfschema/pfs_stat.h"

#include "unittest/mytap/tap.h"

static void test_basic() {
  PFS_all_memory_stat stat;

  diag("test_basic()");

  stat.reset();
  ok(stat.get_session_size() == 0, "size 0");
  ok(stat.get_session_max() == 0, "max 0");

  stat.count_alloc(1000);
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1000, "max 1000");

  stat.count_alloc(500);
  ok(stat.get_session_size() == 1500, "size 1500");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_free(500);
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_alloc(300);
  ok(stat.get_session_size() == 1300, "size 1300");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_alloc(300);
  ok(stat.get_session_size() == 1600, "size 1600");
  ok(stat.get_session_max() == 1600, "max 1600");
}

static void test_free_unclaimed() {
  PFS_all_memory_stat stat;

  diag("test_free_unclaimed()");

  stat.reset();
  ok(stat.get_session_size() == 0, "size 0");
  ok(stat.get_session_max() == 0, "max 0");

  stat.count_alloc(1000);
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1000, "max 1000");

  stat.count_alloc(500);
  ok(stat.get_session_size() == 1500, "size 1500");
  ok(stat.get_session_max() == 1500, "max 1500");

  // Free unclaimed memory
  stat.count_free(700);
  ok(stat.get_session_size() == 800, "size 800");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_free(1000);
  ok(stat.get_session_size() == 0, "size 0");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_free(500);
  ok(stat.get_session_size() == 0, "size 0");
  ok(stat.get_session_max() == 1500, "max 1500");
}

static void test_top() {
  PFS_all_memory_stat stat;
  size_t size;

  diag("test_top()");

  stat.reset();
  ok(stat.get_session_size() == 0, "size 0");
  ok(stat.get_session_max() == 0, "max 0");

  stat.count_alloc(1000);
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1000, "max 1000");

  stat.count_alloc(500);
  ok(stat.get_session_size() == 1500, "size 1500");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_free(500);
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.start_top_statement();
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_alloc(300);
  ok(stat.get_session_size() == 1300, "size 1300");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_alloc(300);
  ok(stat.get_session_size() == 1600, "size 1600");
  ok(stat.get_session_max() == 1600, "max 1600");

  stat.count_free(200);
  ok(stat.get_session_size() == 1400, "size 1600");
  ok(stat.get_session_max() == 1600, "max 1600");

  stat.end_top_statement(&size);
  ok(stat.get_session_size() == 1400, "size 1400");
  ok(stat.get_session_max() == 1600, "max 1600");
  ok(size == 1600, "stmt size 1600");
}

static void test_nest_shallow() {
  PFS_all_memory_stat stat;
  size_t size;
  size_t local_start_1;
  size_t stmt_start_1;
  size_t local_start_2;
  size_t stmt_start_2;
  size_t local_start_3;
  size_t stmt_start_3;

  diag("test_nest_shallow()");

  stat.reset();
  ok(stat.get_session_size() == 0, "size 0");
  ok(stat.get_session_max() == 0, "max 0");

  stat.count_alloc(1000);
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1000, "max 1000");

  stat.count_alloc(500);
  ok(stat.get_session_size() == 1500, "size 1500");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_free(500);
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.start_top_statement();
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_alloc(300);
  ok(stat.get_session_size() == 1300, "size 1300");
  ok(stat.get_session_max() == 1500, "max 1500");

  /* Nested STMT 1 */

  stat.start_nested_statement(&local_start_1, &stmt_start_1);
  ok(stat.get_session_size() == 1300, "size 1300");
  ok(stat.get_session_max() == 1500, "max 1500");
  ok(local_start_1 == 1300, "local start_1 1300");
  ok(stmt_start_1 == 1300, "stmt start_1 1300");

  stat.count_alloc(300);
  ok(stat.get_session_size() == 1600, "size 1600");
  ok(stat.get_session_max() == 1600, "max 1600");

  stat.count_free(200);
  ok(stat.get_session_size() == 1400, "size 1600");
  ok(stat.get_session_max() == 1600, "max 1600");

  stat.count_alloc(2000);
  ok(stat.get_session_size() == 3400, "size 3400");
  ok(stat.get_session_max() == 3400, "max 3400");

  stat.count_free(2000);
  ok(stat.get_session_size() == 1400, "size 1400");
  ok(stat.get_session_max() == 3400, "max 3400");

  stat.end_nested_statement(local_start_1, stmt_start_1, &size);
  ok(stat.get_session_size() == 1400, "size 1400");
  ok(stat.get_session_max() == 3400, "max 3400");
  ok(size == 2100, "stmt1 size 2100");

  /* Nested STMT 2 */

  stat.start_nested_statement(&local_start_2, &stmt_start_2);
  ok(stat.get_session_size() == 1400, "size 1400");
  ok(stat.get_session_max() == 3400, "max 3400");
  ok(local_start_2 == 1400, "local start_2 1400");
  ok(stmt_start_2 == 3400, "stmt start_2 3400");

  stat.count_alloc(5000);
  ok(stat.get_session_size() == 6400, "size 6400");
  ok(stat.get_session_max() == 6400, "max 6400");

  stat.count_free(4500);
  ok(stat.get_session_size() == 1900, "size 1900");
  ok(stat.get_session_max() == 6400, "max 6400");

  stat.end_nested_statement(local_start_2, stmt_start_2, &size);
  ok(stat.get_session_size() == 1900, "size 1900");
  ok(stat.get_session_max() == 6400, "max 6400");
  ok(size == 5000, "stmt2 size 5000");

  /* Nested STMT 3 */

  stat.start_nested_statement(&local_start_3, &stmt_start_3);
  ok(stat.get_session_size() == 1900, "size 1900");
  ok(stat.get_session_max() == 6400, "max 6400");
  ok(local_start_3 == 1900, "local start_3 1900");
  ok(stmt_start_3 == 6400, "stmt start_3 6400");

  stat.end_nested_statement(local_start_3, stmt_start_3, &size);
  ok(stat.get_session_size() == 1900, "size 1900");
  ok(stat.get_session_max() == 6400, "max 6400");
  ok(size == 0, "stmt3 size 0");

  stat.count_free(700);
  ok(stat.get_session_size() == 1200, "size 1200");
  ok(stat.get_session_max() == 6400, "max 6400");

  stat.end_top_statement(&size);
  ok(stat.get_session_size() == 1200, "size 1200");
  ok(stat.get_session_max() == 6400, "max 6400");
  ok(size == 6400, "stmt size 6400");
}

static void test_nest_deep() {
  PFS_all_memory_stat stat;
  size_t size;
  size_t local_start_1;
  size_t stmt_start_1;
  size_t local_start_2;
  size_t stmt_start_2;
  size_t local_start_3;
  size_t stmt_start_3;

  diag("test_nest_deep()");

  stat.reset();
  ok(stat.get_session_size() == 0, "size 0");
  ok(stat.get_session_max() == 0, "max 0");

  stat.count_alloc(1000);
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1000, "max 1000");

  stat.count_alloc(500);
  ok(stat.get_session_size() == 1500, "size 1500");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_free(500);
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1500, "max 1500");

  /* Top statement(begin) */

  stat.start_top_statement();
  ok(stat.get_session_size() == 1000, "size 1000");
  ok(stat.get_session_max() == 1500, "max 1500");

  stat.count_alloc(300);
  ok(stat.get_session_size() == 1300, "size 1300");
  ok(stat.get_session_max() == 1500, "max 1500");

  /* Nested STMT 1 (begin) */

  stat.start_nested_statement(&local_start_1, &stmt_start_1);
  ok(stat.get_session_size() == 1300, "size 1300");
  ok(stat.get_session_max() == 1500, "max 1500");
  ok(local_start_1 == 1300, "local start_1 1300");
  ok(stmt_start_1 == 1300, "stmt start_1 1300");

  stat.count_alloc(1000);
  ok(stat.get_session_size() == 2300, "size 2300");
  ok(stat.get_session_max() == 2300, "max 2300");

  /* Nested STMT 2 (begin) */

  stat.start_nested_statement(&local_start_2, &stmt_start_2);
  ok(stat.get_session_size() == 2300, "size 2300");
  ok(stat.get_session_max() == 2300, "max 2300");
  ok(local_start_2 == 2300, "local start_2 2300");
  ok(stmt_start_2 == 2300, "stmt start_2 2300");

  stat.count_alloc(2000);
  ok(stat.get_session_size() == 4300, "size 4300");
  ok(stat.get_session_max() == 4300, "max 4300");

  /* Nested STMT 3 (begin) */

  stat.start_nested_statement(&local_start_3, &stmt_start_3);
  ok(stat.get_session_size() == 4300, "size 4300");
  ok(stat.get_session_max() == 4300, "max 4300");
  ok(local_start_3 == 4300, "local start_3 4300");
  ok(stmt_start_3 == 4300, "stmt start_3 4300");

  stat.count_alloc(3000);
  ok(stat.get_session_size() == 7300, "size 7300");
  ok(stat.get_session_max() == 7300, "max 7300");

  stat.count_free(3000);
  ok(stat.get_session_size() == 4300, "size 4300");
  ok(stat.get_session_max() == 7300, "max 7300");

  /* Nested STMT 3 (begin) */

  stat.end_nested_statement(local_start_3, stmt_start_3, &size);
  ok(stat.get_session_size() == 4300, "size 4300");
  ok(stat.get_session_max() == 7300, "max 7300");
  ok(size == 3000, "stmt3 size 3000");

  stat.count_free(2000);
  ok(stat.get_session_size() == 2300, "size 2300");
  ok(stat.get_session_max() == 7300, "max 7300");

  /* Nested STMT 2 (end) */

  stat.end_nested_statement(local_start_2, stmt_start_2, &size);
  ok(stat.get_session_size() == 2300, "size 2300");
  ok(stat.get_session_max() == 7300, "max 7300");
  ok(size == 5000, "stmt2 size 5000");

  stat.count_free(1000);
  ok(stat.get_session_size() == 1300, "size 1300");
  ok(stat.get_session_max() == 7300, "max 7300");

  /* Nested STMT 1 (end) */

  stat.end_nested_statement(local_start_1, stmt_start_1, &size);
  ok(stat.get_session_size() == 1300, "size 1300");
  ok(stat.get_session_max() == 7300, "max 7300");
  ok(size == 6000, "stmt1 size 6000");

  stat.count_free(700);
  ok(stat.get_session_size() == 600, "size 600");
  ok(stat.get_session_max() == 7300, "max 7300");

  /* Top statement(end) */

  stat.end_top_statement(&size);
  ok(stat.get_session_size() == 600, "size 600");
  ok(stat.get_session_max() == 7300, "max 7300");
  ok(size == 7300, "stmt size 7300");
}

static void do_all_tests() {
  test_basic();
  test_free_unclaimed();
  test_top();
  test_nest_shallow();
  test_nest_deep();
}

int main(int, char **) {
  plan(143);

  MY_INIT("pfs_mem-t");
  do_all_tests();
  my_end(0);
  return (exit_status());
}
