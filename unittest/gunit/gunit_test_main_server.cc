/* Copyright (c) 2009, 2023, Oracle and/or its affiliates.

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

#include "my_config.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>

#include "my_getopt.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "sql/check_stack.h"
#include "storage/temptable/include/temptable/allocator.h"
#include "unittest/gunit/test_utils.h"

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
#include "storage/perfschema/pfs_server.h"
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

#include "unittest/gunit/fake_table.h"

// We choose non-zero to avoid it working by coincidence.
int Fake_TABLE::highest_table_id = 5;

int main(int argc, char **argv) {
  initialize_stack_direction();

  ::testing::InitGoogleTest(&argc, argv);

#if defined(HAVE_WINNUMA)
  SYSTEM_INFO systemInfo;
  GetSystemInfo(&systemInfo);

  temptable::win_page_size = systemInfo.dwPageSize;
#endif /* HAVE_WINNUMA */

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  pre_initialize_performance_schema();
#endif /*WITH_PERFSCHEMA_STORAGE_ENGINE */

  ::testing::InitGoogleMock(&argc, argv);
  MY_INIT(argv[0]);

  my_testing::setup_server_for_unit_tests();
  int ret = RUN_ALL_TESTS();
  my_testing::teardown_server_for_unit_tests();
  return ret;
}
