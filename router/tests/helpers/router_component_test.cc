/*
  Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <thread>

#include "router_component_test.h"

#include "dim.h"
#include "random_generator.h"

using namespace std::chrono_literals;

void RouterComponentTest::SetUp() {
  mysql_harness::DIM &dim = mysql_harness::DIM::instance();
  // RandomGenerator
  dim.set_RandomGenerator(
      []() {
        static mysql_harness::RandomGenerator rg;
        return &rg;
      },
      [](mysql_harness::RandomGeneratorInterface *) {});
}

void RouterComponentTest::TearDown() {
  shutdown_all();
  ensure_clean_exit();

  if (::testing::Test::HasFailure()) {
    dump_all();
  }
}

bool RouterComponentTest::wait_log_contains(const ProcessWrapper &router,
                                            const std::string &pattern,
                                            std::chrono::milliseconds timeout) {
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
  }

  const auto MSEC_STEP = 50ms;
  bool found = false;
  const auto started = std::chrono::steady_clock::now();
  do {
    const std::string log_content = router.get_full_logfile();
    found = pattern_found(log_content, pattern);
    if (!found) {
      auto step = std::min(timeout, MSEC_STEP);
      std::this_thread::sleep_for(step);
      timeout -= step;
    }
  } while (!found && timeout > std::chrono::steady_clock::now() - started);

  return found;
}
