/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/arg_handler.h"

#include <gmock/gmock.h>

#include "helpers/router_test_helpers.h"  // EXPECT_THROW_LIKE

struct ArgHandlerProcessParam {
  std::string test_name;
  std::vector<std::string> args;
  bool allow_rest_arguments;
  bool ignore_unknown_arguments;
  bool expected_success;
  std::map<std::string, std::vector<std::string>> expected_opts;
  std::vector<std::string> expected_rest_args;
  std::string err_msg;
};

/**
 * @test ensure arghandler process() works
 */
class ArgHandlerProcessTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ArgHandlerProcessParam> {};

static void collect_opts(std::map<std::string, std::vector<std::string>> &opts,
                         const std::string &key, const std::string &value) {
  const auto elem_it = opts.find(key);
  if (elem_it == opts.end()) {
    opts.insert({key, std::vector<std::string>{value}});
  } else {
    // key exists, append the value
    elem_it->second.push_back(value);
  }
}

TEST_P(ArgHandlerProcessTest, ensure) {
  CmdArgHandler arg_handler(GetParam().allow_rest_arguments,
                            GetParam().ignore_unknown_arguments);
  std::map<std::string, std::vector<std::string>> captured;

  arg_handler.add_option({"--opt"}, "an option", CmdOptionValueReq::optional,
                         "", [&captured](const std::string &value) {
                           collect_opts(captured, "opt", value);
                         });

  arg_handler.add_option({"--required"}, "an option with required value",
                         CmdOptionValueReq::required, "",
                         [&captured](const std::string &value) {
                           collect_opts(captured, "required", value);
                         });

  arg_handler.add_option({"--none"}, "an option without a value",
                         CmdOptionValueReq::none, "",
                         [&captured](const std::string &value) {
                           collect_opts(captured, "none", value);
                         });

  if (GetParam().expected_success) {
    EXPECT_NO_THROW(arg_handler.process(GetParam().args));
    EXPECT_THAT(captured, ::testing::Eq(GetParam().expected_opts));
  } else {
    EXPECT_THROW_LIKE(arg_handler.process(GetParam().args), std::exception,
                      GetParam().err_msg);
  }
  EXPECT_THAT(arg_handler.get_rest_arguments(),
              ::testing::Eq(GetParam().expected_rest_args));
}

const ArgHandlerProcessParam arg_handler_process_params[]{
    // rest args
    {"rest", {"rest"}, true, false, true, {}, {"rest"}, ""},
    {"rest_equal_bar", {"rest=bar"}, true, false, true, {}, {"rest=bar"}, ""},
    {"rest_no_rest_args_allowed",
     {"rest"},
     false,
     false,
     false,
     {},
     {},
     "invalid argument 'rest'."},
    // option with optional value
    {"__opt_with_val",
     {"--opt=bar"},
     false,
     false,
     true,
     {{"opt", {"bar"}}},
     {},
     ""},
    {"__opt_eq_no_value",
     {"--opt="},
     false,
     false,
     true,
     {{"opt", {""}}},
     {},
     ""},
    {"__opt_next_empty",
     {"--opt", ""},
     false,
     false,
     true,
     {{"opt", {""}}},
     {},
     ""},
    {"__opt_eol", {"--opt"}, false, false, true, {{"opt", {""}}}, {}, ""},
    {"__opt_multi",
     {"--opt", "--opt", "abc"},
     false,
     false,
     true,
     {{"opt", {"", "abc"}}},
     {},
     ""},
    {"__opt_multi_eq",
     {"--opt=", "--opt", "abc"},
     false,
     false,
     true,
     {{"opt", {"", "abc"}}},
     {},
     ""},
    // option with required value
    {"__required_eq_value",
     {"--required=bar"},
     false,
     false,
     true,
     {{"required", {"bar"}}},
     {},
     ""},
    {"__required_next_value",
     {"--required", "bar"},
     false,
     false,
     true,
     {{"required", {"bar"}}},
     {},
     ""},
    {"__required_next_empty",
     {"--required", ""},
     false,
     false,
     true,
     {{"required", {""}}},
     {},
     ""},
    {"__required_eq_empty",
     {"--required="},
     false,
     false,
     true,
     {{"required", {""}}},
     {},
     ""},
    {"__required_eol",
     {"--required"},
     false,
     false,
     false,
     {},
     {},
     "option '--required' expects a value, got nothing"},
    // option with no value
    {"__none_with_val",
     {"--none=bar"},
     false,
     false,
     false,
     {},
     {},
     "option '--none' does not expect a value, but got a value"},
    {"__none", {"--none"}, false, false, true, {{"none", {""}}}, {}, ""},
    // unknown arg
    {"__not_exists_with_val",
     {"--not-exists=bar"},
     false,
     false,
     false,
     {},
     {},
     "unknown option '--not-exists'"},
    // unknown arg ignored
    {"__not_exists_with_val_ignored",
     {"--not-exists=bar"},
     false,
     true,
     true,
     {},
     {},
     ""},
    {"__not_exists_with_no_val_ignored",
     {"--not-exists"},
     false,
     true,
     true,
     {},
     {},
     ""},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, ArgHandlerProcessTest,
    ::testing::ValuesIn(arg_handler_process_params),
    [](const testing::TestParamInfo<ArgHandlerProcessParam> &info) {
      return info.param.test_name +
             (info.param.expected_success ? "_works" : "_fails");
    });

/**
 * @test ensure ::none, ::required and ::optional don't changed later.
 */
TEST(CmdOptionValueReq, CheckConstants) {
  ASSERT_EQ(static_cast<uint8_t>(CmdOptionValueReq::none), 0x01);
  ASSERT_EQ(static_cast<uint8_t>(CmdOptionValueReq::required), 0x02);
  ASSERT_EQ(static_cast<uint8_t>(CmdOptionValueReq::optional), 0x03);
}

struct CmdOptionParam {
  std::string test_name;
  std::vector<std::string> names;
  std::string description;
  CmdOptionValueReq req;
  std::string metavar;

  bool use_action;
};

/**
 * @test ensure arghandler process() works
 */
class CmdOptionTest : public ::testing::Test,
                      public ::testing::WithParamInterface<CmdOptionParam> {};

/**
 * @test option passed to constructor of CmdOption can be read back.
 */
TEST_P(CmdOptionTest, Constructor) {
  CmdOption::ActionFunc dummy_action_func = [](const std::string &) {};
  CmdOption::ActionFunc action_func =
      GetParam().use_action ? dummy_action_func : nullptr;
  CmdOption opt(GetParam().names, GetParam().description, GetParam().req,
                GetParam().metavar, action_func);

  EXPECT_THAT(opt.names, ::testing::ContainerEq(GetParam().names));
  EXPECT_THAT(opt.description, ::testing::StrEq(GetParam().description));
  EXPECT_EQ(opt.value_req, GetParam().req);
  EXPECT_THAT(opt.metavar, ::testing::StrEq(GetParam().metavar));

  if (GetParam().use_action) {
    EXPECT_NE(opt.action, nullptr);
  } else {
    EXPECT_EQ(opt.action, nullptr);
  }
}

const CmdOptionParam cmd_options_params[]{
    {"default_action",
     {"-a", "--some-long-a"},
     "Testing -a and --some-long-a",
     CmdOptionValueReq::none,
     "test",
     false},
    {"with_action",
     {"-a", "--some-long-a"},
     "Testing -a and --some-long-a",
     CmdOptionValueReq::none,
     "test",
     false},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CmdOptionTest, ::testing::ValuesIn(cmd_options_params),
    [](const testing::TestParamInfo<CmdOptionParam> &info) {
      return info.param.test_name + "_works";
    });

TEST(CmdArgHandlerConstructorTest, Default) {
  CmdArgHandler c;
  EXPECT_FALSE(c.allow_rest_arguments);
}

TEST(CmdArgHandlerConstructorTest, AllowRestArguments) {
  CmdArgHandler c(true);
  EXPECT_TRUE(c.allow_rest_arguments);
}

// CmdArgHandler.add_option()
//
struct AddOptionArg {
  std::vector<std::string> names;
  std::string description;
  CmdOptionValueReq req;
  std::string metavar;
};

struct CmdArgHandlerAddOptionParam {
  std::string test_name;
  std::vector<AddOptionArg> args;

  bool use_action;
};

/**
 * @test ensure arghandler process() works
 */
class CmdArgHandlerAddOptionTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<CmdArgHandlerAddOptionParam> {};

/**
 * @test option added with add_option() can be read back and action can be
 * called.
 */
TEST_P(CmdArgHandlerAddOptionTest, ensure) {
  CmdArgHandler c;
  std::string capture_val;
  CmdOption::ActionFunc capture_func = [&capture_val](const std::string &val) {
    capture_val = val;
  };

  CmdOption::ActionFunc action_func =
      GetParam().use_action ? capture_func : nullptr;
  for (const auto &arg : GetParam().args) {
    c.add_option(arg.names, arg.description, arg.req, arg.metavar, action_func);
  }

  const auto opts = c.get_options();
  ASSERT_EQ(opts.size(), GetParam().args.size());
  for (size_t ndx = 0; ndx < opts.size(); ++ndx) {
    const auto &current_arg = opts.at(ndx);
    const auto &expected_arg = GetParam().args.at(ndx);

    EXPECT_THAT(current_arg.names, ::testing::ContainerEq(expected_arg.names));
    EXPECT_THAT(current_arg.description,
                ::testing::StrEq(expected_arg.description));
    EXPECT_THAT(current_arg.value_req, expected_arg.req);
    EXPECT_THAT(current_arg.metavar, ::testing::StrEq(expected_arg.metavar));

    // check find_option
    for (const auto &name : current_arg.names) {
      auto it = c.find_option(name);
      ASSERT_NE(it, c.end());  // each name must be findable
    }
  }
  EXPECT_EQ(c.find_option("--non-existing-option"), c.end());

  if (GetParam().use_action) {
    const auto &current_arg = opts.at(0);
    current_arg.action("foo");
    EXPECT_EQ(capture_val, "foo");
  }
}

const CmdArgHandlerAddOptionParam cmd_arg_handler_add_options_params[]{
    {"no_action",
     {{{"-a", "--some-long-a"},
       "Testing -a and --some-long-a",
       CmdOptionValueReq::none,
       "test"}},
     false},
    {"with_action",
     {{{"-a", "--some-long-a"},
       "Testing -a and --some-long-a",
       CmdOptionValueReq::none,
       "test"}},
     true},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, CmdArgHandlerAddOptionTest,
    ::testing::ValuesIn(cmd_arg_handler_add_options_params),
    [](const testing::TestParamInfo<CmdArgHandlerAddOptionParam> &info) {
      return info.param.test_name + "_works";
    });

//

struct ValidOptionNameParam {
  std::string test_name;
  std::string arg;

  bool expected_success;
};

/**
 * @test ensure arghandler process() works
 */
class ValidOptionNameTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ValidOptionNameParam> {};

/**
 * @test option added with add_option() can be read back and action can be
 * called.
 */
TEST_P(ValidOptionNameTest, ensure) {
  CmdArgHandler c;

  if (GetParam().expected_success) {
    ASSERT_TRUE(c.is_valid_option_name(GetParam().arg));
  } else {
    ASSERT_FALSE(c.is_valid_option_name(GetParam().arg));
  }
}

const ValidOptionNameParam valid_option_name_params[]{
    {"short_opt", "-a", true},
    {"long_opt", "--ab", true},
    {"long_opt_with_dash", "--with-ab", true},
    {"long_opt_with_underscore", "--with-ab", true},
    {"short_uppercase_opt", "-a", true},
    {"long_uppercase_opt", "--ab", true},
    {"long_uppercase_opt_with_dash", "--with-ab", true},
    {"long_uppercase_opt_with_underscore", "--with-ab", true},
    {"short_opt_multi_char", "-ab", false},
    {"short_opt_uppercase_multi_char", "-AB", false},
    {"short_opt_no_name", "-", false},
    {"long_opt_no_name", "--", false},
    {"long_opt_dash_in_name", "---a", false},
    {"long_opt_uppercase_dash_in_name", "---U", false},
    {"long_opt_trailing_dash", "--with-ab-", false},
    {"long_opt_trailing_underscore", "--with-ab__", false},
    {"long_opt_prefix_dot", "--.ab", false},
    {"long_opt_prefix_underscore", "--__ab", false},
    {"long_opt_space", "--AB ", false},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, ValidOptionNameTest, ::testing::ValuesIn(valid_option_name_params),
    [](const testing::TestParamInfo<ValidOptionNameParam> &info) {
      return info.param.test_name +
             (info.param.expected_success ? "_works" : "_fails");
    });

TEST(UsageLineTest, WithRestArguments) {
  CmdArgHandler c(true);

  const std::vector<CmdOption> cmd_options{
      {{"-a", "--novalue-a"},
       "Testing -a",
       CmdOptionValueReq::none,
       "",
       nullptr},
      {{"-b", "--optional-b"},
       "Testing -b",
       CmdOptionValueReq::optional,
       "optional",
       nullptr},
      {{"-c", "--required-c"},
       "Testing -c",
       CmdOptionValueReq::required,
       "required",
       nullptr},
  };

  for (const auto &opt : cmd_options) {
    c.add_option(opt);
  }

  std::vector<std::string> lines = c.usage_lines("testarg", "REST", 120);
  ASSERT_THAT(lines, ::testing::SizeIs(1));

  std::string usage_line = lines.at(0);
  ASSERT_THAT(usage_line, ::testing::StartsWith("testarg"));
  ASSERT_THAT(usage_line, ::testing::EndsWith("[REST]"));

  for (const auto &opt : cmd_options) {
    for (const auto &name : opt.names) {
      ASSERT_THAT(usage_line, ::testing::HasSubstr(name));
    }
  }
}

TEST(UsageLineTest, WithoutRestArguments) {
  CmdArgHandler c(false);

  std::vector<CmdOption> cmd_options{};
  for (auto &opt : cmd_options) {
    c.add_option(opt);
  }

  std::vector<std::string> lines = c.usage_lines("testarg", "REST", 120);
  ASSERT_THAT(lines, ::testing::SizeIs(1));

  std::string usage_line = lines.at(0);
  ASSERT_THAT(usage_line, ::testing::StartsWith("testarg"));
  ASSERT_THAT(usage_line, ::testing::Not(::testing::EndsWith("[REST]")));
}

TEST(UsageLineTest, MultiLine) {
  constexpr const size_t width{40};

  CmdArgHandler c(true);

  const std::vector<CmdOption> cmd_options{
      {{"-a", "--novalue-a"},
       "Testing -a",
       CmdOptionValueReq::none,
       "",
       nullptr},
      {{"-b", "--optional-b"},
       "Testing -b",
       CmdOptionValueReq::optional,
       "optional",
       nullptr},
      {{"-c", "--required-c"},
       "Testing -c",
       CmdOptionValueReq::required,
       "required",
       nullptr},
  };
  for (const auto &opt : cmd_options) {
    c.add_option(opt);
  }

  std::vector<std::string> lines = c.usage_lines("testarg", "REST", width);
  ASSERT_THAT(lines, ::testing::SizeIs(4));
  ASSERT_THAT(lines.at(lines.size() - 1), ::testing::EndsWith("[REST]"));

  for (const auto &line : lines) {
    ASSERT_THAT(line.size(), ::testing::Le(width));
  }
}

TEST(OptionDescriptionTest, OptionDescriptions) {
  CmdArgHandler c(false);

  std::vector<CmdOption> cmd_options{
      {{"-a", "--novalue-a"},
       "Testing -a",
       CmdOptionValueReq::none,
       "",
       nullptr},
      {{"-b", "--optional-b"},
       "Testing -b",
       CmdOptionValueReq::optional,
       "optional",
       nullptr},
      {{"-c", "--required-c"},
       "Testing -c",
       CmdOptionValueReq::required,
       "required",
       nullptr},

  };
  for (const auto &opt : cmd_options) {
    c.add_option(opt);
  }

  std::vector<std::string> lines = c.option_descriptions(120, 8);
  ASSERT_THAT(lines.at(0), ::testing::StrEq("  -a, --novalue-a"));
  ASSERT_THAT(lines.at(1), ::testing::StrEq("        Testing -a"));
  ASSERT_THAT(
      lines.at(2),
      ::testing::StrEq("  -b [ <optional>], --optional-b [ <optional>]"));
  ASSERT_THAT(lines.at(3), ::testing::StrEq("        Testing -b"));
  ASSERT_THAT(lines.at(4),
              ::testing::StrEq("  -c <required>, --required-c <required>"));
  ASSERT_THAT(lines.at(5), ::testing::StrEq("        Testing -c"));
}

// process().post_action

struct EntangledOptionsParam {
  std::string test_name;
  std::vector<std::string> args;

  bool expected_success;
};

/**
 * @test ensure arghandler process() calls post-action handlers.
 */
class EntangledOptionsTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<EntangledOptionsParam> {};

/**
 * @test post_action handler is called after all other action handlers are
 * called.
 */
TEST_P(EntangledOptionsTest, ensure) {
  CmdArgHandler c;

  bool action_a_called{false}, action_b_called{false};
  bool both_called{false};

  {
    CmdOption option_a(
        {"--option-a"}, "Testing --option-a", CmdOptionValueReq::none,
        "option-a_value",
        [&](const std::string & /* value */) { action_a_called = true; },
        [&](const std::string & /* value */) {
          both_called = action_a_called && action_b_called;
        });

    c.add_option(option_a);
  }

  {
    CmdOption option_b(
        {"--option-b"}, "Testing --option-b", CmdOptionValueReq::none,
        "option-b_value",
        [&](const std::string & /* value */) { action_b_called = true; },
        [&](const std::string & /* value */) {
          both_called = action_a_called && action_b_called;
        });

    c.add_option(option_b);
  }

  c.process(GetParam().args);
  EXPECT_EQ(both_called, GetParam().expected_success);
}

const EntangledOptionsParam post_action_params[]{
    {"only_opt_a", {"--option-a"}, false},
    {"only_opt_b", {"--option-b"}, false},
    {"both_options", {"--option-a", "--option-b"}, true},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, EntangledOptionsTest, ::testing::ValuesIn(post_action_params),
    [](const testing::TestParamInfo<EntangledOptionsParam> &info) {
      return info.param.test_name +
             (info.param.expected_success ? "_works" : "_fails");
    });

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
