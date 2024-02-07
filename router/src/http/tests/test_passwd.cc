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

/**
 * Test the mysqlrouter_passwd tool.
 */
#include "passwd.h"

#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>

#include <gmock/gmock.h>

#include "mysql/harness/filesystem.h"
#include "mysql/harness/utility/string.h"  // mysql_harness::join
#include "mysqlrouter/utils.h"             // set_prompt_password
#include "print_version.h"                 // build_version
#include "router_config.h"                 // MYSQL_ROUTER_PACKAGE_NAME
#include "welcome_copyright_notice.h"      // ORACLE_WELCOME_COPYRIGHT_NOTICE

constexpr const char kAppExeFileName[]{"mysqlrouter_passwd"};

using mysql_harness::join;

mysql_harness::Path g_origin_path;

constexpr size_t kOptIndent = 2;
constexpr size_t kDescIndent = 6;

static void ParamPrinter(
    const std::vector<std::pair<std::string, std::string>> &fields,
    std::ostream *os) {
  *os << "(";
  bool is_first{true};
  for (const auto &kv : fields) {
    if (is_first) {
      is_first = false;
    } else {
      *os << ", ";
    }
    *os << kv.first << ": " << kv.second;
  }
  *os << ")";
}

struct Option {
  std::vector<std::string> opts;
  std::string arg;
  std::string desc;
};

std::vector<Option> cmdline_opts{
    // should be alphabetically ordered
    {{"-?", "--help"}, "", "Display this help and exit."},
    {{"--kdf"},
     "<name>",
     "Key Derivation Function for 'set'. One of pbkdf2-sha256, pbkdf2-sha512,\n"
     "      sha256-crypt, sha512-crypt. default: sha256-crypt"},
    {{"-V", "--version"}, "", "Display version information and exit."},
    {{"--work-factor"},
     "<num>",
     "Work-factor hint for KDF if account is updated."}};

std::vector<std::pair<std::string, std::string>> cmdline_cmds{
    {"delete", "Delete username (if it exists) from <filename>."},
    {"list", "list one or all accounts of <filename>."},
    {"set", "add or overwrite account of <username> in <filename>."},
    {"verify",
     "verify if password matches <username>'s credentials in <filename>."},
};

// build param of description
static std::string format_desc_opt(const Option &opt) {
  auto val = opt.arg;
  return join(std::accumulate(
                  opt.opts.begin(), opt.opts.end(), std::vector<std::string>(),
                  [&val](std::vector<std::string> acc, std::string cur) {
                    acc.push_back(cur + (val.empty() ? "" : " " + val));

                    return acc;
                  }),
              ", ");
}

// build help-text from options
static std::string help_builder(const std::vector<Option> &opts) {
  std::vector<std::string> out;

  {
    out.push_back("Usage");
    out.push_back("");

    // opts
    std::vector<std::string> formatted_options;
    formatted_options.push_back(kAppExeFileName);
    formatted_options.push_back("[opts]");
    formatted_options.push_back("<cmd>");
    formatted_options.push_back("<filename>");
    formatted_options.push_back("[<username>]");

    std::string line{" "};
    for (const auto &opt : formatted_options) {
      if (line.size() + 1 + opt.size() > 93) {
        out.push_back(line);

        // prepare next line
        line = " ";
      }
      line += " " + opt;
    }

    out.push_back(line);

    // --help
    formatted_options.clear();
    formatted_options.push_back(kAppExeFileName);
    formatted_options.push_back("--help");

    line = " ";
    for (const auto &opt : formatted_options) {
      if (line.size() + 1 + opt.size() > 93) {
        out.push_back(line);

        // prepare next line
        line = " ";
      }
      line += " " + opt;
    }

    out.push_back(line);

    // --version
    formatted_options.clear();
    formatted_options.push_back(kAppExeFileName);
    formatted_options.push_back("--version");

    line = " ";
    for (const auto &opt : formatted_options) {
      if (line.size() + 1 + opt.size() > 93) {
        out.push_back(line);

        // prepare next line
        line = " ";
      }
      line += " " + opt;
    }

    out.push_back(line);
  }

  if (!cmdline_cmds.empty()) {
    out.push_back("");
    out.push_back("Commands");
    out.push_back("");
    for (const auto &opt : cmdline_cmds) {
      out.push_back(std::string(kOptIndent, ' ') + opt.first);
      out.push_back(std::string(kDescIndent, ' ') + opt.second);
    }
  }

  if (!opts.empty()) {
    out.push_back("");
    out.push_back("Options");
    out.push_back("");

    for (const auto &opt : opts) {
      out.push_back(std::string(kOptIndent, ' ') + format_desc_opt(opt));
      out.push_back(std::string(kDescIndent, ' ') + opt.desc);
    }
  }

  // enforce a newline at the end
  out.push_back("");
  return join(out, "\n");
}

static std::string version_builder() {
  std::string version_string;
  build_version(MYSQL_ROUTER_PACKAGE_NAME, &version_string);

  std::stringstream os;
  os << version_string << std::endl
     << ORACLE_WELCOME_COPYRIGHT_NOTICE("2018") << std::endl;

  return os.str();
}

const std::string kHelpText(help_builder(cmdline_opts));
const std::string kVersionText(version_builder());

// placeholder in the opts to replace by the temp-filename
const std::string kPasswdPlaceholder("@passwdfile@");

struct PasswdFrontendTestParam {
  std::string test_name;
  std::string test_scenario_id;

  std::vector<std::string> cmdline_args;
  int exit_code;
  std::string stdin_content;
  std::string stdout_content;
  std::string stderr_content;
  std::string passwd_content;

  friend void PrintTo(const PasswdFrontendTestParam &p, std::ostream *os) {
    ParamPrinter(
        {
            {"test_scenario_id", ::testing::PrintToString(p.test_scenario_id)},
            {"cmdline", ::testing::PrintToString(p.cmdline_args)},
        },
        os);
  }
};

class PasswdFrontendTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<PasswdFrontendTestParam> {};

class TempDirectory {
 public:
  explicit TempDirectory(const std::string &prefix = "router")
      : name_{mysql_harness::get_tmp_dir(prefix)} {}

  ~TempDirectory() { mysql_harness::delete_dir_recursive(name_); }

  std::string name() const { return name_; }

 private:
  std::string name_;
};

/**
 * @test ensure PasswordFrontent behaves correctly.
 */
TEST_P(PasswdFrontendTest, ensure) {
  std::istringstream cin(GetParam().stdin_content);
  std::ostringstream cout;
  std::ostringstream cerr;

  mysqlrouter::set_prompt_password([&cin](const std::string &) {
    std::string s;
    std::getline(cin, s);
    return s;
  });

  TempDirectory tmpdir;
  std::string passwd_filename(
      mysql_harness::Path(tmpdir.name()).join("passwd").str());

  std::fstream pw(passwd_filename, std::ios::out);
  ASSERT_TRUE(pw.is_open());
  pw << GetParam().passwd_content;
  pw.close();

  // replace the placeholder with the name of the temp passwd-file
  std::vector<std::string> args{GetParam().cmdline_args};
  for (auto &arg : args) {
    if (arg == kPasswdPlaceholder) {
      arg = passwd_filename;
    }
  }

  // do what passwd_cli.cc's main does
  int exit_code = 0;
  try {
    exit_code = PasswdFrontend(kAppExeFileName, args, cout, cerr).run();
  } catch (const FrontendError &e) {
    cerr << e.what() << std::endl;
    exit_code = EXIT_FAILURE;
  }

  EXPECT_EQ(exit_code, GetParam().exit_code);
  EXPECT_EQ(cout.str(), GetParam().stdout_content);
  EXPECT_THAT(cerr.str(), ::testing::StartsWith(GetParam().stderr_content));

  // in case we add/set and success, verify auth works
  auto set_cmd_iter = std::find(args.begin(), args.end(), "set");

  if ((set_cmd_iter != args.end()) && (GetParam().exit_code == EXIT_SUCCESS)) {
    SCOPED_TRACE("// check successfully set password can be verified");
    std::istringstream v_cin(GetParam().stdin_content);
    std::ostringstream v_cout;
    std::ostringstream v_cerr;

    // replace the command, leave the other options unchanged
    *set_cmd_iter = "verify";

    // replace the prompt function
    mysqlrouter::set_prompt_password([&v_cin](const std::string &) {
      std::string s;
      std::getline(v_cin, s);
      return s;
    });

    EXPECT_NO_THROW(
        exit_code =
            PasswdFrontend(kAppExeFileName, args, v_cout, v_cerr).run());

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(v_cout.str(), "");
    EXPECT_EQ(v_cerr.str(), "");
  }
}

constexpr const char kPasswdEmpty[]{""};
constexpr const char kPasswdUserKarlNoPw[]{"karl:"};
constexpr const char kPasswdUserKarlGoodPw[]{
    "karl:$6$3ieWD5TQkakPm.iT$"  // sha512 and salt
    "4HI5XzmE4UCSOsu14jujlXYNYk2SB6gi2yVoAncaOzynEnTI0Rc9."
    "78jHABgKm2DHr1LHc7Kg9kCVs9/uCOR7/\n"  // password: test
};

// cleanup test-names to satisfy googletest's requirements
static std::string sanitise(const std::string &name) {
  std::string out{name};

  for (auto &c : out) {
    if (!isalnum(c)) {
      c = '_';
    }
  }

  return out;
}

const PasswdFrontendTestParam password_frontend_param[]{
    {"--help",
     "WL12604::TS-1_1",
     {"--help"},
     EXIT_SUCCESS,
     "",
     kHelpText + "\n",
     "",
     kPasswdEmpty},

    {"--version",
     "WL12604::TS-1_2",
     {"--version"},
     EXIT_SUCCESS,
     "",
     kVersionText + "\n",
     "",
     kPasswdEmpty},

    // set

    {"set: no args",
     "",
     {},
     EXIT_FAILURE,
     "",
     "",
     "expected a <cmd>\n",
     kPasswdEmpty},
    {"set: missing username",
     "",
     {"set", "filename"},
     EXIT_FAILURE,
     "",
     "",
     "expected <filename> and <username>\n",
     kPasswdEmpty},
    {"set: username with colon",
     "WL12503::TS_PW_F1F2_5",
     {"set", kPasswdPlaceholder, "karl:bar"},
     EXIT_FAILURE,
     "",
     "",
     "<username> contained ':' at pos 4, allowed are [a-zA-Z0-9]+\n",
     kPasswdEmpty},
    {"set: empty password",
     "",
     {"set", kPasswdPlaceholder, "karl"},
     EXIT_SUCCESS,
     "",
     "",
     "",
     kPasswdEmpty},
    {"set: implicit kdf",
     "WL12503::TS-1_3,WL12503::TS_PS_F3_1",
     {"set", kPasswdPlaceholder, "karl"},
     EXIT_SUCCESS,
     "pw2",
     "",
     "",
     kPasswdEmpty},
    {"set: implicit kdf, update",
     "WL12503::TS-1_6",
     {"set", kPasswdPlaceholder, "karl"},
     EXIT_SUCCESS,
     "test2",
     "",
     "",
     kPasswdUserKarlGoodPw},
    {"set: explicit kdf, sha256-crypt, add",
     "WL12503::TS_PW_F1F2_1",
     {"set", kPasswdPlaceholder, "karl", "--kdf", "sha256-crypt"},
     EXIT_SUCCESS,
     "pw2",
     "",
     "",
     kPasswdEmpty},
    {"set: explicit kdf, sha512-crypt, add",
     "WL12503::TS_PW_F1F2_1",
     {"set", kPasswdPlaceholder, "karl", "--kdf", "sha512-crypt"},
     EXIT_SUCCESS,
     "pw2",
     "",
     "",
     kPasswdEmpty},
    {"set: explicit kdf, sha512-crypt, update, same kdf, different pw",
     "WL12503::TS_PW_F1F2_2",
     {"set", kPasswdPlaceholder, "karl", "--kdf", "sha512-crypt"},
     EXIT_SUCCESS,
     "pw2",
     "",
     "",
     kPasswdUserKarlGoodPw},
    {"set: explicit kdf, sha256-crypt, update, different kdf, same pw",
     "WL12503::TS-1_7",
     {"set", kPasswdPlaceholder, "karl", "--kdf", "sha512-crypt"},
     EXIT_SUCCESS,
     "test",
     "",
     "",
     kPasswdUserKarlGoodPw},
    {"set: explicit kdf, pbkdf2-sha256",
     "WL12503::TS_PW_F1F2_1",
     {"set", kPasswdPlaceholder, "karl", "--kdf", "pbkdf2-sha256"},
     EXIT_SUCCESS,
     "pw2",
     "",
     "",
     kPasswdEmpty},
    {"set: explicit kdf, pbkdf2-sha512",
     "WL12503::TS_PW_F1F2_1",
     {"set", kPasswdPlaceholder, "karl", "--kdf", "pbkdf2-sha512"},
     EXIT_SUCCESS,
     "pw2",
     "",
     "",
     kPasswdEmpty},
    {"set: unknown kdf",
     "WL12503::TS_PW_F1F2_5",
     {"set", kPasswdPlaceholder, "karl", "--kdf", "does not work"},
     EXIT_FAILURE,
     "",
     "",
     "unknown kdf: does not work\n",
     kPasswdEmpty},
    {"set: work-factor > 1000. Should be faster",
     "WL12503::TS-1_3",
     {"set", kPasswdPlaceholder, "karl", "--work-factor", "1001"},
     EXIT_SUCCESS,
     "",
     "",
     "",
     kPasswdEmpty},
    {"set: work-factor, not an int",
     "WL12503::TS-1_4",
     {"set", kPasswdPlaceholder, "karl", "--work-factor", "abc"},
     EXIT_FAILURE,
     "",
     "",
     "--work-factor is not an integer",
     kPasswdEmpty},
    {"set: work-factor, out-of-range",
     "WL12503::TS-1_5",
     {"set", kPasswdPlaceholder, "karl", "--work-factor",
      "999999999999999999999999999999999"},
     EXIT_FAILURE,
     "",
     "",
     "--work-factor is larger than",
     kPasswdEmpty},
    {"set: work-factor, negative",
     "WL12503::TS-1_5",
     {"set", kPasswdPlaceholder, "karl", "--work-factor=-1"},
     EXIT_FAILURE,
     "",
     "",
     "--work-factor is negative",
     kPasswdEmpty},
    {"set: work-factor, hex",
     "",
     {"set", kPasswdPlaceholder, "karl", "--work-factor=0xff"},
     EXIT_FAILURE,
     "",
     "",
     "--work-factor is not a positive integer",
     kPasswdEmpty},

    // delete

    {"delete: file doesn't exist",
     "WL12503::TS-1_10",
     {"delete", "does-not-exist", "karl"},
     EXIT_FAILURE,
     "",
     "",
     "can't open file 'does-not-exist'",
     kPasswdEmpty},

    {"delete: no user",
     "WL12503::TS-1_11",
     {"delete", "does-not-exist"},
     EXIT_FAILURE,
     "",
     "",
     "expected <filename> and <username>",
     kPasswdEmpty},

    {"delete: account exists",
     "WL12503::TS-PW_F6_1",
     {"delete", kPasswdPlaceholder, "karl"},
     EXIT_SUCCESS,
     "",
     "",
     "",
     kPasswdUserKarlGoodPw},
    {"delete: account does not exist",
     "WL12503::TS-PW_F6_2",
     {"delete", kPasswdPlaceholder, "karl"},
     EXIT_FAILURE,
     "",
     "",
     "user 'karl' not found",
     kPasswdEmpty},

    // verify

    {"verify: file doesn't exist",
     "WL12503::TS-1_8",
     {"verify", "does-not-exist", "karl"},
     EXIT_FAILURE,
     "",
     "",
     "can't open file 'does-not-exist'",
     kPasswdEmpty},

    {"verify: no user",
     "WL12503::TS-1_9",
     {"verify", "does-not-exist"},
     EXIT_FAILURE,
     "",
     "",
     "expected <filename> and <username>",
     kPasswdEmpty},
    {"verify: account exist, good hash",
     "WL12503::TS_PW_F4F5_1",
     {"verify", kPasswdPlaceholder, "karl"},
     EXIT_SUCCESS,
     "test",
     "",
     "",
     kPasswdUserKarlGoodPw},
    {"verify: account exist, broken hash",
     "",
     {"verify", kPasswdPlaceholder, "karl"},
     EXIT_FAILURE,
     "pw2",
     "",
     "failed to parse file",
     kPasswdUserKarlNoPw},
    {"verify: account exist, wrong password",
     "WL12503::TS_PW_F4F5_2",
     {"verify", kPasswdPlaceholder, "karl"},
     EXIT_FAILURE,
     "pw",
     "",
     "user not found",
     kPasswdEmpty},
    {"verify: account does not exist",
     "",
     {"verify", kPasswdPlaceholder, "karl"},
     EXIT_FAILURE,
     "",
     "",
     "user not found",
     kPasswdEmpty},

    // list

    {"list: file doesn't exist",
     "WL12503::TS_PW_F7_2",
     {"list", "does-not-exist", "karl"},
     EXIT_FAILURE,
     "",
     "",
     "can't open file 'does-not-exist'",
     kPasswdEmpty},

    {"list: no user",
     "WL12503::TS_PW_F8_1",
     {"list", kPasswdPlaceholder},
     EXIT_SUCCESS,
     "",
     kPasswdUserKarlGoodPw,
     "",
     kPasswdUserKarlGoodPw},

    {"list: account exists",
     "WL12503::TS-PW_F8_2",
     {"list", kPasswdPlaceholder, "karl"},
     EXIT_SUCCESS,
     "",
     kPasswdUserKarlGoodPw,
     "",
     kPasswdUserKarlGoodPw},

    {"list: account does not exist",
     "WL12503::TS_PW_F8_3",
     {"list", kPasswdPlaceholder, "karl"},
     EXIT_FAILURE,
     "",
     "",
     "user 'karl' not found",
     kPasswdEmpty},

};

INSTANTIATE_TEST_SUITE_P(
    Spec, PasswdFrontendTest, ::testing::ValuesIn(password_frontend_param),
    [](testing::TestParamInfo<PasswdFrontendTestParam> param_info) {
      return sanitise(param_info.param.test_name +
                      std::string(param_info.param.exit_code == EXIT_SUCCESS
                                      ? " succeeds"
                                      : " fails"));
    });

int main(int argc, char *argv[]) {
  g_origin_path = mysql_harness::Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
