/*  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>  // NOLINT(build/c++11)
#include <thread>  // NOLINT(build/c++11)

#include "collector/counted_mysql_session.h"
#include "helper/make_shared_ptr.h"
#include "mrs/database/helper/query_gtid_executed.h"

using CountedMySQLSession = collector::CountedMySQLSession;

static int get_env_int(const char *name) {
  auto env = getenv(name);
  if (!env) throw std::runtime_error("Enverioment variable not set.");

  return atoi(env);
}

class CountedMySQLSessionTests : public testing::Test {
 public:
  helper::MakeSharedPtr<CountedMySQLSession> sut_;
};

TEST_F(CountedMySQLSessionTests, first_test) {
  auto port = get_env_int("PORT");

  sut_->connect("127.0.0.1", port, "root", "", {}, {});

  auto result = mrs::database::get_gtid_executed(sut_.get());

  std::cout << "size:" << result.size() << "\n";

  for (const auto &a : result) {
    std::cout << "element:" << a.to_string() << "\n";
  }
}
