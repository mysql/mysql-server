/* Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
#include "test_utils.h"

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
#include "../storage/perfschema/pfs_server.h"
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */


#include "fake_table.h"

// We choose non-zero to avoid it working by coincidence.
int Fake_TABLE::highest_table_id= 5;

namespace {

bool opt_use_tap= true;
bool opt_unit_help= false;

struct my_option unittest_options[] =
{
  { "tap-output", 1, "TAP (default) or gunit output.",
    &opt_use_tap, &opt_use_tap, NULL,
    GET_BOOL, OPT_ARG,
    opt_use_tap, 0, 1, 0,
    0, NULL
  },
  { "help", 2, "Help.",
    &opt_unit_help, &opt_unit_help, NULL,
    GET_BOOL, NO_ARG,
    opt_unit_help, 0, 1, 0,
    0, NULL
  },
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


extern "C" bool get_one_option(int, const struct my_option *, char *)
{
  return FALSE;
}

}  // namespace


extern void install_tap_listener();

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  pre_initialize_performance_schema();
#endif /*WITH_PERFSCHEMA_STORAGE_ENGINE */

  ::testing::InitGoogleMock(&argc, argv);
  MY_INIT(argv[0]);

  if (handle_options(&argc, &argv, unittest_options, get_one_option))
    return EXIT_FAILURE;
  if (opt_use_tap)
    install_tap_listener();
  if (opt_unit_help)
    printf("\n\nTest options: [--[disable-]tap-output]\n");

  my_testing::setup_server_for_unit_tests();
  int ret= RUN_ALL_TESTS();
  my_testing::teardown_server_for_unit_tests();
  return ret;
}
