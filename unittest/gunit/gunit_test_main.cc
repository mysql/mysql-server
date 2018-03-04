/* Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "my_getopt.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/psi/mysql_mutex.h"

class Cost_constant_cache;
extern "C" {
  CHARSET_INFO *system_charset_info= NULL;
}

namespace {

bool opt_use_tap= false;
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

// Some globals needed for merge_small_tests.cc
mysql_mutex_t LOCK_open;
uint    opt_debug_sync_timeout= 0;
thread_local MEM_ROOT **THR_MALLOC= nullptr;
thread_local THD *current_thd= nullptr;
// Needed for linking with opt_costconstantcache.cc and Fake_Cost_model_server
Cost_constant_cache *cost_constant_cache= NULL;

extern "C" void sql_alloc_error_handler(void)
{
  ADD_FAILURE();
}


extern void install_tap_listener();

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::InitGoogleMock(&argc, argv);
  MY_INIT(argv[0]);

  mysql_mutex_init(PSI_NOT_INSTRUMENTED, &LOCK_open, MY_MUTEX_INIT_FAST);

  if (handle_options(&argc, &argv, unittest_options, get_one_option))
    return EXIT_FAILURE;
  if (opt_use_tap)
    install_tap_listener();
  if (opt_unit_help)
    printf("\n\nTest options: [--[enable-]tap-output] output TAP "
           "rather than googletest format\n");

  const int retval= RUN_ALL_TESTS();
  mysql_mutex_destroy(&LOCK_open);
  my_end(0);
  return retval;
}
