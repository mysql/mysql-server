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

#include "mysql/harness/arg_handler.h"

#include <cstdint>
#include <cstdio>
#include <sstream>
#include <streambuf>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

// ignore GMock warnings
#ifdef __clang__
#ifndef __has_warning
#define __has_warning(x) 0
#endif
#if __has_warning("-Wkeyword-macro")
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#include "gmock/gmock.h"
#pragma clang diagnostic pop
#else
#include "gmock/gmock.h"
#endif

using ::testing::ContainerEq;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrEq;
using std::string;
using std::vector;

class CmdOptionTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}

  vector<string> names = {"-a", "--some-long-a"};
  string description = "Testing -a and --some-long-a";
  string metavar = "test";
  string action_result;
};

class ArgHandlerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}

  vector<CmdOption> cmd_options = {
      CmdOption({"-a", "--novalue-a"}, "Testing -a", CmdOptionValueReq::none,
                "", nullptr),
      CmdOption({"-b", "--optional-b"}, "Testing -b",
                CmdOptionValueReq::optional, "optional",
                [this](const string &value) { this->action_result = value; }),
      CmdOption({"-c", "--required-c"}, "Testing -c",
                CmdOptionValueReq::required, "required",
                [this](const string &value) { this->action_result = value; }),
  };
  string action_result;
};

// We don't actually need it but without it for each call of ASSERT_ macro
// with CmdOption as one of the params, gtest generates binary printer (hex
// representation) for the CmdOption that causes Valgrind "conditional jump"
// errors from vfprintf()
std::ostream &operator<<(std::ostream &o, const CmdOption &cmd) {
  for (const auto &name : cmd.names) {
    o << name << " ";
  }
  o << "\n" << cmd.description << "\n" << cmd.metavar << "\n";
  return o;
}

TEST(CmdOptionValueReq, CheckConstants) {
  ASSERT_EQ(static_cast<uint8_t>(CmdOptionValueReq::none), 0x01);
  ASSERT_EQ(static_cast<uint8_t>(CmdOptionValueReq::required), 0x02);
  ASSERT_EQ(static_cast<uint8_t>(CmdOptionValueReq::optional), 0x03);
}

TEST_F(CmdOptionTest, Constructor) {
  CmdOption opt(names, description, CmdOptionValueReq::none, metavar, nullptr);
  ASSERT_THAT(opt.names, ContainerEq(names));
  ASSERT_THAT(opt.description, StrEq(description));
  ASSERT_THAT(opt.value_req, CmdOptionValueReq::none);
  ASSERT_THAT(opt.metavar, StrEq(metavar));
}

TEST_F(CmdOptionTest, ConstructorWithAction) {
  string value = "the value";
  CmdOption opt(names, description, CmdOptionValueReq::none, metavar,
                [this](const string &v) { this->action_result = v; });
  ASSERT_TRUE(nullptr != opt.action);
  std::bind(opt.action, value)();
  ASSERT_EQ(action_result, value);
}

TEST_F(ArgHandlerTest, DefaultConstructor) {
  CmdArgHandler c;
  ASSERT_FALSE(c.allow_rest_arguments);
}

TEST_F(ArgHandlerTest, ConstructorAllowRestArguments) {
  CmdArgHandler c(true);
  ASSERT_TRUE(c.allow_rest_arguments);
}

TEST_F(ArgHandlerTest, AddOption) {
  // Function arguments tested at compile time using static_assert
  CmdArgHandler c;
  auto opt = cmd_options.at(0);
  c.add_option(opt.names, opt.description, opt.value_req, opt.metavar,
               opt.action);
  auto options = c.get_options();
  ASSERT_THAT(options, SizeIs(1));
  ASSERT_THAT(options.at(0).names, ContainerEq(opt.names));
  ASSERT_THAT(options.at(0).description, StrEq(opt.description));
  ASSERT_THAT(options.at(0).value_req, opt.value_req);
  ASSERT_THAT(options.at(0).metavar, opt.metavar);
}

TEST_F(ArgHandlerTest, AddOptionWithAction) {
  CmdArgHandler c;
  string value = "the value";

  c.add_option(cmd_options.at(1));
  auto options = c.get_options();
  ASSERT_THAT(options, SizeIs(1));
  ASSERT_TRUE(nullptr != options.at(0).action);
  std::bind(options.at(0).action, value)();
  ASSERT_EQ(action_result, value);
}

TEST_F(ArgHandlerTest, FindOption) {
  CmdArgHandler c;
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }
  auto options = c.get_options();
  ASSERT_THAT((*c.find_option("-a")).names, ContainerEq(options.at(0).names));
  ASSERT_THAT((*c.find_option("--novalue-a")).names,
              ContainerEq(options.at(0).names));
  ASSERT_THAT((*c.find_option("-b")).names, ContainerEq(options.at(1).names));
  ASSERT_EQ(c.find_option("--non-existing-options"), c.end());
}

TEST_F(ArgHandlerTest, IsValidOptionNameValids) {
  CmdArgHandler c;
  vector<string> valids = {
      "-a",
      "--ab",
      "--with-ab"
      "--with_ab"
      "-U",
      "--UC",
      "--WITH-AC",
      "--WITH_AC",
  };
  for (auto &name : valids) {
    SCOPED_TRACE("Supposed to be valid: " + name);
    ASSERT_TRUE(c.is_valid_option_name(name));
  }
}

TEST_F(ArgHandlerTest, IsValidOptionNameInvalids) {
  CmdArgHandler c;
  vector<string> invalids = {
      "-ab",    "--",    "-",   "---a", "--with-ab-", "--with-ab__", "--.ab",
      "--__ab", "--AB ", "-AB", "--",   "-",          "---U"};
  for (auto &name : invalids) {
    SCOPED_TRACE("Supposed to be invalid: " + name);
    ASSERT_FALSE(c.is_valid_option_name(name));
  }
}

TEST_F(ArgHandlerTest, ProcessOptionNoValue) {
  CmdArgHandler c(true);
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }
  c.process({"-a", "some value after a"});
  ASSERT_THAT(action_result, StrEq(""));

  action_result = "";
  c.process({"--novalue-a", "rest"});
  ASSERT_THAT(action_result, StrEq(""));

  c.process({"-a", "-b"});
  ASSERT_THAT(action_result, StrEq(""));
}

TEST_F(ArgHandlerTest, ProcessRequiredOptional) {
  CmdArgHandler c;
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }
  string value_b = "value_option_b";
  c.process({"-b", value_b});
  ASSERT_THAT(action_result, StrEq(value_b));

  action_result = "";
  c.process({"--optional-b", "-a"});
  ASSERT_THAT(action_result, IsEmpty());

  action_result = "";
  c.process({"-b", "-a"});
  ASSERT_THAT(action_result, IsEmpty());

  action_result = "";
  c.process({"--optional-b", "-a"});
  ASSERT_THAT(action_result, IsEmpty());
}

TEST_F(ArgHandlerTest, ProcessOptionalRequired) {
  CmdArgHandler c;
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }

  string value_c = "value_option_c";
  c.process({"-c", value_c});
  ASSERT_THAT(action_result, value_c);

  action_result = "";
  ASSERT_THROW({ c.process({"--required-c"}); }, std::invalid_argument);
  try {
    c.process({"--required-c"});
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), HasSubstr("requires a value"));
  }
}

TEST_F(ArgHandlerTest, ProcessEmptyValue) {
  // Bug#24809081
  CmdArgHandler c;
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }

  string value = "";
  ASSERT_NO_THROW(c.process({"-c", value}));
  ASSERT_THAT(action_result, value);

  ASSERT_NO_THROW(c.process({"-b", value}));
  ASSERT_THAT(action_result, value);
}

TEST_F(ArgHandlerTest, ProcessUnknownOption) {
  CmdArgHandler c;
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }
  ASSERT_THROW({ c.process({"--unknown-option"}); }, std::invalid_argument);
  try {
    c.process({"--unknown-option"});
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), HasSubstr("unknown option"));
  }
}

TEST_F(ArgHandlerTest, ProcessRestArguments) {
  vector<string> args;
  vector<string> rest;
  CmdArgHandler c(true);
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }
  rest = {"some", "rest", "values"};
  args = {"--novalue-a"};
  args.insert(args.end(), rest.begin(), rest.end());
  c.process(args);
  ASSERT_THAT(c.get_rest_arguments(), ContainerEq(rest));

  rest.clear();
  args.clear();
  rest = {"rest", "values"};
  args = {"--optional-b", "some"};
  args.insert(args.end(), rest.begin(), rest.end());
  c.process(args);
  ASSERT_THAT(c.get_rest_arguments(), ContainerEq(rest));

  rest.clear();
  args.clear();
  rest = {"rest", "values"};
  args = {"rest", "-b", "some", "values"};
  c.process(args);
  ASSERT_THAT(c.get_rest_arguments(), ContainerEq(rest));
}

TEST_F(ArgHandlerTest, ProcessNotAllowedRestArguments) {
  vector<string> args;
  CmdArgHandler c(false);
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }
  args = {"-a", "rest", "arguments"};
  ASSERT_THROW({ c.process(args); }, std::invalid_argument);
  try {
    c.process(args);
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), HasSubstr("invalid argument"));
    ASSERT_THAT(exc.what(), HasSubstr("'" + args.at(1) + "'"));
  }
}

TEST_F(ArgHandlerTest, UsageLineWithRestArguments) {
  CmdArgHandler c(true);
  vector<string> lines;
  string usage_line;
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }

  lines = c.usage_lines("testarg", "REST", 120);
  ASSERT_THAT(lines, SizeIs(1));
  usage_line = lines.at(0);

  ASSERT_THAT(usage_line, StartsWith("testarg"));
  ASSERT_THAT(usage_line, EndsWith("[REST]"));

  for (auto &opt : cmd_options) {
    for (auto &name : opt.names) {
      ASSERT_THAT(usage_line, HasSubstr(name));
    }
  }
}

TEST_F(ArgHandlerTest, UsageLineWithoutRestArguments) {
  CmdArgHandler c(false);
  vector<string> lines;
  string usage_line;
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }

  lines = c.usage_lines("testarg", "REST", 120);
  ASSERT_THAT(lines, SizeIs(1));
  usage_line = lines.at(0);

  ASSERT_THAT(usage_line, StartsWith("testarg"));
  ASSERT_THAT(usage_line, Not(EndsWith("[REST]")));
}

TEST_F(ArgHandlerTest, UsageLineMultiLine) {
  CmdArgHandler c(true);
  vector<string> lines;
  string usage_line;
  size_t width = 40;
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }

  lines = c.usage_lines("testarg", "REST", width);
  ASSERT_THAT(lines, SizeIs(4));
  ASSERT_THAT(lines.at(lines.size() - 1), EndsWith("[REST]"));

  for (auto &line : lines) {
    ASSERT_THAT(line.size(), Le(width));
  }
}

TEST_F(ArgHandlerTest, OptionDescriptions) {
  CmdArgHandler c(false);
  vector<string> lines;
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }

  lines = c.option_descriptions(120, 8);
  ASSERT_THAT(lines.at(0), StrEq("  -a, --novalue-a"));
  ASSERT_THAT(lines.at(1), StrEq("        Testing -a"));
  ASSERT_THAT(lines.at(2),
              StrEq("  -b [ <optional>], --optional-b [ <optional>]"));
  ASSERT_THAT(lines.at(3), StrEq("        Testing -b"));
  ASSERT_THAT(lines.at(4), StrEq("  -c <required>, --required-c <required>"));
  ASSERT_THAT(lines.at(5), StrEq("        Testing -c"));
}

TEST_F(ArgHandlerTest, CheckIfTwoOptionsAreProvided) {
  CmdArgHandler c(false);

  bool option_a_set = false, option_b_set = false;

  CmdOption option_a(
      {"--option-a"}, "Testing --option-a", CmdOptionValueReq::required,
      "option-a_value",
      [&](const string & /* value */) { option_a_set = true; },
      [&] {
        if (option_a_set != option_b_set)
          throw std::invalid_argument(
              "--option-a has to be used together with --option-b");
      });

  CmdOption option_b(
      {"--option-b"}, "Testing --option-b", CmdOptionValueReq::required,
      "option-b_value",
      [&](const string & /* value */) { option_b_set = true; },
      [&] {
        if (option_a_set != option_b_set)
          throw std::invalid_argument(
              "--option-b has to be used together with --option-a");
      });

  c.add_option(option_a);
  c.add_option(option_b);

  ASSERT_THROW({ c.process({"--option-a=value-a"}); }, std::invalid_argument);

  option_a_set = false, option_b_set = false;
  try {
    c.process({"--option-a=value-a"});
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(
        exc.what(),
        HasSubstr("--option-a has to be used together with --option-b"));
  }

  option_a_set = false, option_b_set = false;
  ASSERT_THROW({ c.process({"--option-b=value-b"}); }, std::invalid_argument);

  option_a_set = false, option_b_set = false;
  try {
    c.process({"--option-b=value-b"});
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(
        exc.what(),
        HasSubstr("--option-b has to be used together with --option-a"));
  }

  option_a_set = false, option_b_set = false;
  ASSERT_NO_THROW(c.process({"--option-a=value-a", "--option-b=value-b"}));

  option_a_set = false, option_b_set = false;
  ASSERT_NO_THROW(c.process({"--option-b=value-b", "--option-a=value-a"}));
}
