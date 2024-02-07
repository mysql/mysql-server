/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include "process_launcher.h"

#ifdef _WIN32

struct ArgQuoteParam {
  std::string input;
  std::string expected;
};
class ArgQuoteTest : public ::testing::Test,
                     public ::testing::WithParamInterface<ArgQuoteParam> {};

TEST_P(ArgQuoteTest, quote) {
  ASSERT_EQ(mysql_harness::win32::cmdline_quote_arg(GetParam().input),
            GetParam().expected);
}

const ArgQuoteParam arg_quote_params[] = {
    // empty input is quoted
    {R"()", R"("")"},
    // a space
    {R"( )", R"(" ")"},
    // non quoted
    {R"(a)", R"(a)"},
    // trailing "
    {R"(a")", R"("a\"")"},
    // trailing space, needs quoting
    {R"(a )", R"("a ")"},
    // middle " needs quoting and escaping
    {R"(a"b)", R"("a\"b")"},
    // backslash quote needs quoting and escaping
    {R"(a\"b)", R"("a\\\"b")"},
    // trailing backslash, no escaping
    {R"(a\)", R"(a\)"},
    // double trailing backslash, no escaping
    {R"(a\\)", R"(a\\)"},
    // double trailing backslash with space, quoting, escaping
    {R"(a \\)", R"("a \\\\")"},
    // trailing quote with multiple backslash and space, quoting, escaping
    {R"(a \\")", R"("a \\\\\"")"},
    // just a backslash, no quoting, no escaping
    {R"(a\b)", R"(a\b)"},
};

INSTANTIATE_TEST_SUITE_P(Spec, ArgQuoteTest,
                         ::testing::ValuesIn(arg_quote_params));

// cmdline_from_args

struct CmdLineQuoteParam {
  std::string executable_path;
  std::vector<std::string> args;

  std::string expected;
};

class CmdLineQuoteTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<CmdLineQuoteParam> {};

TEST_P(CmdLineQuoteTest, quote) {
  ASSERT_EQ(mysql_harness::win32::cmdline_from_args(GetParam().executable_path,
                                                    GetParam().args),
            GetParam().expected);
}

const CmdLineQuoteParam cmdline_quote_params[] = {
    {"foo", {"bar"}, "foo bar"},
    {"foo bar", {"bar"}, R"("foo bar" bar)"},
    {R"(c:\foo bar\)", {"bar"}, R"("c:\foo bar\\" bar)"},
    {R"(c:\foo bar\)", {"--bar", ""}, R"("c:\foo bar\\" --bar "")"},
};

INSTANTIATE_TEST_SUITE_P(Spec, CmdLineQuoteTest,
                         ::testing::ValuesIn(cmdline_quote_params));
#endif

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
