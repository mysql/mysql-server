/*
 * Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <gmock/gmock.h>

#include "plugin/x/src/xpl_regex.h"

namespace xpl {
namespace test {

const bool PASS = true;
const bool FAIL = false;

struct Param_regex_match {
  const std::string pattern;
  bool expect;
  std::string value;
};

class Regex_match_test : public testing::TestWithParam<Param_regex_match> {
 public:
  const Regex m_re{GetParam().pattern.c_str()};
};

TEST_P(Regex_match_test, regex_match) {
  const auto &param = GetParam();
  EXPECT_EQ(param.expect, m_re.match(param.value.c_str()));
}

#define DECIMAL_PATTERN "DECIMAL(?:\\([0-9]+(?:,[0-9]+)?\\))?"
#define ALTER_USER_PATTERN "ALTER USER .+ IDENTIFIED BY .+"

Param_regex_match regex_match_param[] = {
    {DECIMAL_PATTERN, PASS, "DECIMAL"},
    {DECIMAL_PATTERN, PASS, "decimal"},
    {DECIMAL_PATTERN, PASS, "decimal(10)"},
    {DECIMAL_PATTERN, PASS, "decimal(10,5)"},
    {DECIMAL_PATTERN, FAIL, "DEC"},
    {DECIMAL_PATTERN, FAIL, "decimal()"},
    {DECIMAL_PATTERN, FAIL, "decimal(abc)"},
    {DECIMAL_PATTERN, FAIL, "decimal(a,c)"},
    {DECIMAL_PATTERN, FAIL, "decimal(10,5,1)"},
    {DECIMAL_PATTERN, FAIL, "decimal(10)(5)"},
    {DECIMAL_PATTERN, FAIL, "decimal(10.5)"},
    {DECIMAL_PATTERN, FAIL, "(10,5)"},
    {DECIMAL_PATTERN, FAIL, " decimal(10,5)"},
    {DECIMAL_PATTERN, FAIL, "decimal (10,5)"},
    {DECIMAL_PATTERN, FAIL, "decimal( 10,5)"},
    {DECIMAL_PATTERN, FAIL, "decimal(10 ,5)"},
    {DECIMAL_PATTERN, FAIL, "decimal(10, 5)"},
    {DECIMAL_PATTERN, FAIL, "decimal(10,5 )"},
    {DECIMAL_PATTERN, FAIL, "decimal(10,5) "},
    {ALTER_USER_PATTERN, PASS, "ALTER USER foo@localhost IDENTIFIED BY 'foo'"},
    {ALTER_USER_PATTERN, PASS, "alter user foo@localhost identified by 'foo'"},
    {ALTER_USER_PATTERN, FAIL, "ALTER USER foo@localhost ACCOUNT UNLOCK"},
};

INSTANTIATE_TEST_CASE_P(regex_match_test, Regex_match_test,
                        testing::ValuesIn(regex_match_param));

struct Param_regex_match_groups {
  const std::string pattern;
  bool expect;
  const bool skip_empty;
  std::vector<std::string> expect_groups;
  std::string value;
};

#define TYPE_GROUPS_PATTERN                                             \
  "(INT)|"                                                              \
  "(CHAR|TEXT)(?:\\(([0-9]+)\\))?(?: CHARSET \\w+)?(?: COLLATE \\w+)?|" \
  "(DECIMAL)(?:\\(([0-9]+)(?:,([0-9]+))?\\))?|"                         \
  "\\w+(?:\\(([0-9]+)(?:,([0-9]+))?\\))?( UNSIGNED)?"

#define ALTER_USER_GROUPS_PATTERN  \
  "ALTER USER '(\\w+)'@'(\\w*)'.+" \
  "(FAILED_LOGIN_ATTEMPTS|PASSWORD_LOCK_TIME|ACCOUNT UNLOCK).*"

const bool SKIP_EMPTY = true;

class Regex_match_groups_test
    : public testing::TestWithParam<Param_regex_match_groups> {
 public:
  const Regex m_re{GetParam().pattern.c_str()};
};

TEST_P(Regex_match_groups_test, regex_match_groups) {
  const auto &param = GetParam();
  Regex::Group_list groups;
  EXPECT_EQ(param.expect,
            m_re.match_groups(param.value.c_str(), &groups, param.skip_empty));
  EXPECT_EQ(param.expect_groups, groups);
}

Param_regex_match_groups regex_match_groups_param[] = {
    {TYPE_GROUPS_PATTERN, PASS, SKIP_EMPTY, {"int", "int"}, "int"},
    {TYPE_GROUPS_PATTERN,
     PASS,
     SKIP_EMPTY,
     {"char(5)", "char", "5"},
     "char(5)"},
    {TYPE_GROUPS_PATTERN, PASS, SKIP_EMPTY, {"char", "char"}, "char"},
    {TYPE_GROUPS_PATTERN,
     PASS,
     SKIP_EMPTY,
     {"text(64)", "text", "64"},
     "text(64)"},
    {TYPE_GROUPS_PATTERN, PASS, SKIP_EMPTY, {"text", "text"}, "text"},
    {TYPE_GROUPS_PATTERN,
     PASS,
     SKIP_EMPTY,
     {"decimal(10,5)", "decimal", "10", "5"},
     "decimal(10,5)"},
    {TYPE_GROUPS_PATTERN,
     PASS,
     SKIP_EMPTY,
     {"decimal(10)", "decimal", "10"},
     "decimal(10)"},
    {TYPE_GROUPS_PATTERN, PASS, SKIP_EMPTY, {"decimal", "decimal"}, "decimal"},
    {TYPE_GROUPS_PATTERN,
     PASS,
     SKIP_EMPTY,
     {"qwe(10,5) unsigned", "10", "5", " unsigned"},
     "qwe(10,5) unsigned"},
    {TYPE_GROUPS_PATTERN,
     PASS,
     SKIP_EMPTY,
     {"qwe(10) unsigned", "10", " unsigned"},
     "qwe(10) unsigned"},
    {TYPE_GROUPS_PATTERN,
     PASS,
     SKIP_EMPTY,
     {"char(20) charset latin1", "char", "20"},
     "char(20) charset latin1"},
    {TYPE_GROUPS_PATTERN,
     PASS,
     SKIP_EMPTY,
     {"text(30) charset latin1 collate latin1_bin", "text", "30"},
     "text(30) charset latin1 collate latin1_bin"},
    {ALTER_USER_GROUPS_PATTERN,
     PASS,
     !SKIP_EMPTY,
     {"ALTER USER 'foo'@'localhost' ACCOUNT UNLOCK", "foo", "localhost",
      "ACCOUNT UNLOCK"},
     "ALTER USER 'foo'@'localhost' ACCOUNT UNLOCK"},
    {ALTER_USER_GROUPS_PATTERN,
     PASS,
     !SKIP_EMPTY,
     {"ALTER USER 'foo'@'localhost' FAILED_LOGIN_ATTEMPTS 0", "foo",
      "localhost", "FAILED_LOGIN_ATTEMPTS"},
     "ALTER USER 'foo'@'localhost' FAILED_LOGIN_ATTEMPTS 0"},
    {ALTER_USER_GROUPS_PATTERN,
     PASS,
     !SKIP_EMPTY,
     {"ALTER USER 'foo'@'localhost' PASSWORD_LOCK_TIME 0", "foo", "localhost",
      "PASSWORD_LOCK_TIME"},
     "ALTER USER 'foo'@'localhost' PASSWORD_LOCK_TIME 0"},
    {ALTER_USER_GROUPS_PATTERN,
     PASS,
     !SKIP_EMPTY,
     {"ALTER USER 'foo'@'localhost' "
      "PASSWORD_LOCK_TIME 0 FAILED_LOGIN_ATTEMPTS 1",
      "foo", "localhost", "FAILED_LOGIN_ATTEMPTS"},
     "ALTER USER 'foo'@'localhost' "
     "PASSWORD_LOCK_TIME 0 FAILED_LOGIN_ATTEMPTS 1"},
    {ALTER_USER_GROUPS_PATTERN,
     PASS,
     !SKIP_EMPTY,
     {"ALTER USER 'foo'@'localhost' "
      "FAILED_LOGIN_ATTEMPTS 1 PASSWORD_LOCK_TIME 0",
      "foo", "localhost", "PASSWORD_LOCK_TIME"},
     "ALTER USER 'foo'@'localhost' "
     "FAILED_LOGIN_ATTEMPTS 1 PASSWORD_LOCK_TIME 0"},
    {ALTER_USER_GROUPS_PATTERN,
     PASS,
     !SKIP_EMPTY,
     {"ALTER USER 'foo'@'localhost' "
      "ACCOUNT LOCK FAILED_LOGIN_ATTEMPTS 1",
      "foo", "localhost", "FAILED_LOGIN_ATTEMPTS"},
     "ALTER USER 'foo'@'localhost' "
     "ACCOUNT LOCK FAILED_LOGIN_ATTEMPTS 1"},
    {ALTER_USER_GROUPS_PATTERN,
     FAIL,
     !SKIP_EMPTY,
     {},
     "ALTER USER 'foo'@'localhost' IDENTIFIED BY 'secret'"},
    {ALTER_USER_GROUPS_PATTERN,
     FAIL,
     !SKIP_EMPTY,
     {},
     "ALTER USER 'foo'@'localhost' ACCOUNT LOCK"},
    {ALTER_USER_GROUPS_PATTERN,
     PASS,
     !SKIP_EMPTY,
     {"ALTER USER 'foo'@'' ACCOUNT UNLOCK", "foo", "", "ACCOUNT UNLOCK"},
     "ALTER USER 'foo'@'' ACCOUNT UNLOCK"},
    {ALTER_USER_GROUPS_PATTERN,
     PASS,
     !SKIP_EMPTY,
     {"ALTER USER 'foo'@'localhost' IDENTIFIED BY 'secret' ACCOUNT UNLOCK",
      "foo", "localhost", "ACCOUNT UNLOCK"},
     "ALTER USER 'foo'@'localhost' IDENTIFIED BY 'secret' ACCOUNT UNLOCK"},
    {ALTER_USER_GROUPS_PATTERN,
     PASS,
     !SKIP_EMPTY,
     {"ALTER USER 'foo'@'localhost' ACCOUNT UNLOCK IDENTIFIED BY 'secret'",
      "foo", "localhost", "ACCOUNT UNLOCK"},
     "ALTER USER 'foo'@'localhost' ACCOUNT UNLOCK IDENTIFIED BY 'secret'"},
};

INSTANTIATE_TEST_CASE_P(regex_match_groups_test, Regex_match_groups_test,
                        testing::ValuesIn(regex_match_groups_param));

}  // namespace test
}  // namespace xpl
