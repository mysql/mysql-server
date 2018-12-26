/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
 * BUG22074209 --HELP OUTPUT DOES NOT DISPLAY VERSION
 *
 */

#include "cmd_exec.h"
#include "gtest_consoleoutput.h"
#include "router_app.h"

#include "gmock/gmock.h"

using ::testing::StartsWith;

Path g_origin;

class Bug22074209 : public ConsoleOutputTest {
 protected:
  virtual void SetUp() {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
  }
};

TEST_F(Bug22074209, HelpShowsVersion) {
  MySQLRouter r;
  std::string cmd = app_mysqlrouter->str() + " --help";

  auto cmd_result = cmd_exec(cmd, false);
  EXPECT_THAT(cmd_result.output, StartsWith(r.get_version_line()));
}

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
