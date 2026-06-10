/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gtest/gtest.h>

#include "helpers/process_manager.h"
#include "mysql/harness/logging/registry.h"
#include "test/helpers.h"

int main(int argc, char *argv[]) {
  init_test_logger();

  ::testing::InitGoogleTest(&argc, argv);

  const char *kEnvLiveServer = "RUN_WITH_LIVE_MYSQLD";
  if (nullptr == getenv(kEnvLiveServer)) {
    auto &filter = testing::GTEST_FLAG(filter);
    if (filter.empty() || filter == "*") {
      filter = "-DatabaseQuery*.*:JsonMapping*.*";
    }
    std::cerr << filter << std::endl;
    std::cerr << "Filtering out tests that run on live database. To run those "
                 "tests set following environment variable: "
              << kEnvLiveServer << std::endl;
  }

  if (nullptr != getenv("DISABLE_DEBUG_LOG")) {
    mysql_harness::logging::set_log_level_for_all_loggers(
        mysql_harness::logging::LogLevel::kFatal);
  }

  ProcessManager::set_origin(Path(argv[0]).dirname());

  return RUN_ALL_TESTS();
}
