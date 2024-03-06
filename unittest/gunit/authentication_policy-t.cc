/*
   Copyright (c) 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <gtest/gtest.h>
#include <sys/types.h>

#include <assert.h>
#include <utility>

#include "sql/auth/authentication_policy.h"

using namespace authentication_policy;

/**
  Namespace for authentication_policy unit tests
*/
namespace authentication_policy_unittest {

/**
  Helper class to public the parser functionality
*/
class Policy_test : public Policy {
 public:
  /**
    Parse @@authentication_policy variable value.

    @param new_policy_value [in] new value of the variable
    @param parsed_factors [out]  parsed factors

    @retval  false    OK
    @retval  true     Error
  */
  static bool parse(const std::string &new_policy_value,
                    Factors &parsed_factors) {
    return Policy::parse(new_policy_value, parsed_factors);
  }
};

/**
  Type of factor test function -checking if the factor has expected properties
*/
using factor_test = bool (*)(const Factor &);

/**
  Check if the factor is optional.

   @param factor the factor
   @return result of the check
*/
bool is_optional(const Factor &factor) { return factor.is_optional(); }

/**
  Check if the factor may be any plugin and has no default.

   @param factor the factor
   @return result of the check
*/
bool is_whichever_no_default(const Factor &factor) {
  return factor.is_whichever() && factor.get_default_plugin().empty();
}

/**
  Check if the factor may be any plugin and has default.

   @param factor the factor
   @return result of the check
*/
bool is_whichever_has_default(const Factor &factor) {
  return factor.is_whichever() && factor.get_default_plugin() == "plugin";
}

/**
  Check if the factor is a mandatory plugin.

   @param factor the factor
   @return result of the check
*/
bool is_mandatory(const Factor &factor) {
  return factor.get_mandatory_plugin() == "plugin";
}

/**
  Test parser with single factor, correct values
 */
TEST(AuthPolicyParser, Correct1Factor) {
  const std::vector<std::string> policies{("*"), ("*:plugin"), ("plugin")};
  Factors factors;
  for (auto &policy : policies) {
    ASSERT_FALSE(Policy_test::parse(policy, factors))
        << "Parsing " << policy << " failed.";
    EXPECT_EQ(factors.size(), 1)
        << "Policy " << policy << " has unexpected number of factors.";
  }
}
/**
  Test parser with 2 factors, correct values.
  Additionally check if parsed factors have expected proprties.
 */
TEST(AuthPolicyParser, Correct2Factors) {
  const std::vector<std::tuple<std::string, factor_test, factor_test>> tests{
      {("*,"), is_whichever_no_default, is_optional},
      {("*,*"), is_whichever_no_default, is_whichever_no_default},
      {("*,*:plugin"), is_whichever_no_default, is_whichever_has_default},
      {("*,plugin"), is_whichever_no_default, is_mandatory},
      {("*:plugin,"), is_whichever_has_default, is_optional},
      {("*:plugin,*"), is_whichever_has_default, is_whichever_no_default},
      {("*:plugin,*:plugin"), is_whichever_has_default,
       is_whichever_has_default},
      {("*:plugin,plugin"), is_whichever_has_default, is_mandatory},
      {("plugin,"), is_mandatory, is_optional},
      {("plugin,*"), is_mandatory, is_whichever_no_default},
      {("plugin,*:plugin"), is_mandatory, is_whichever_has_default},
      {("plugin,plugin"), is_mandatory, is_mandatory}};
  Factors factors;
  for (auto &test : tests) {
    ASSERT_FALSE(Policy_test::parse(std::get<0>(test), factors))
        << "Parsing " << std::get<0>(test) << " failed.";
    ASSERT_EQ(factors.size(), 2) << "Policy " << std::get<0>(test)
                                 << " has unexpected number of factors.";
    EXPECT_TRUE(std::get<1>(test)(factors[0]))
        << "First factor " << std::get<0>(test) << " is incorrect.";
    EXPECT_TRUE(std::get<2>(test)(factors[1]))
        << "Second factor " << std::get<0>(test) << " is incorrect.";
  }
}
/**
  Test parser with 3 factors, correct values.
 */
TEST(AuthPolicyParser, Correct3Factors) {
  const std::vector<std::string> policies{("*:plugin,,"),
                                          ("*,,"),
                                          ("plugin,,"),
                                          ("*,*,"),
                                          ("*,*:plugin,"),
                                          ("*,plugin,"),
                                          ("*:plugin,*,"),
                                          ("*:plugin,*:plugin,"),
                                          ("*:plugin,plugin,"),
                                          ("plugin,*,"),
                                          ("plugin,*:plugin,"),
                                          ("plugin,plugin,"),
                                          ("*,*,*"),
                                          ("*,*,*:plugin"),
                                          ("*,*,plugin"),
                                          ("*,*:plugin,*"),
                                          ("*,*:plugin,*:plugin"),
                                          ("*,*:plugin,plugin"),
                                          ("*,plugin,*"),
                                          ("*,plugin,*:plugin"),
                                          ("*,plugin,plugin"),
                                          ("*:plugin,*,*"),
                                          ("*:plugin,*,*:plugin"),
                                          ("*:plugin,*,plugin"),
                                          ("*:plugin,*:plugin,*"),
                                          ("*:plugin,*:plugin,*:plugin"),
                                          ("*:plugin,*:plugin,plugin"),
                                          ("*:plugin,plugin,*"),
                                          ("*:plugin,plugin,*:plugin"),
                                          ("*:plugin,plugin,plugin"),
                                          ("plugin,*,*"),
                                          ("plugin,*,*:plugin"),
                                          ("plugin,*,plugin"),
                                          ("plugin,*:plugin,*"),
                                          ("plugin,*:plugin,*:plugin"),
                                          ("plugin,*:plugin,plugin"),
                                          ("plugin,plugin,*"),
                                          ("plugin,plugin,*:plugin"),
                                          ("plugin,plugin,plugin")};
  Factors factors;

  for (auto &policy : policies) {
    EXPECT_FALSE(Policy_test::parse(policy, factors))
        << "Parsing " << policy << " failed.";
    EXPECT_EQ(factors.size(), 3)
        << "Policy " << policy << " has unexpected number of factors.";
  }
}

/**
  Test parser with 3 factors, correct values, some spaces at correct positions
  to be trimmed.
 */
TEST(AuthPolicyParser, Correct3FactorsWithSpaces) {
  const std::vector<std::string> policies{(" *: plugin, ,"),
                                          ("*,,"),
                                          (" plugin, , "),
                                          ("* , * , "),
                                          ("  *:plugin, plugin\t,"),
                                          ("\t\tplugin,*,"),
                                          ("*, plugin , *: plugin"),
                                          ("*,plugin,         plugin"),
                                          (" plugin , plugin , * : plugin"),
                                          (" plugin,plugin,plugin ")};
  Factors factors;

  for (auto &policy : policies) {
    EXPECT_FALSE(Policy_test::parse(policy, factors))
        << "Parsing " << policy << " failed.";
    EXPECT_EQ(factors.size(), 3)
        << "Policy " << policy << " has unexpected number of factors.";
    for (auto &factor : factors) {
      EXPECT_TRUE(factor.is_optional() ||
                  factor.get_mandatory_or_default_plugin().find_first_of(
                      " \t") == std::string::npos);
    }
  }
}

/**
  Test parser with incorrect number of factors (0 or >3)
 */
TEST(AuthPolicyParser, IncorrectNoOfFactors) {
  const std::vector<std::string> policies{
      (""),         (",,,,"), ("*,*,,"), ("*,*,*,policy"), ("*,*,*,policy,"),
      ("*,*,*,*,*")};
  Factors factors;
  for (auto &policy : policies)
    EXPECT_TRUE(Policy_test::parse(policy, factors))
        << "Parsing " << policy << " was successful.";
}
/**
  Test parser with optional first factor
 */
TEST(AuthPolicyParser, FirstCannotBeOptional) {
  const std::vector<std::string> policies{(","), (",,")};
  Factors factors;
  for (auto &policy : policies)
    EXPECT_TRUE(Policy_test::parse(policy, factors))
        << "Parsing " << policy << " was successful.";
}
/**
  Test parser with non optional following optional factor
 */
TEST(AuthPolicyParser, OptionalCannotFollowNonOptional) {
  const std::vector<std::string> policies{(",*"),         (",*:policy"),
                                          (",policy"),    (",,*"),
                                          (",,*:policy"), (",,policy")};
  Factors factors;
  for (auto &policy : policies)
    EXPECT_TRUE(Policy_test::parse(policy, factors))
        << "Parsing " << policy << " was successful.";
}
/**
  Test parser with incorrect syntax, especially misplaced '*' or ':'
 */
TEST(AuthPolicyParser, IncorrectSyntax) {
  const std::vector<std::string> policies{
      ("*:p:"),  ("p:*,"),  (":,,"),    ("*x,,"),   ("*:*x,,"), ("*x,,"),
      ("x,*:,"), ("*:*,,"), ("*,*:p:"), ("p,p:*,"), ("*,:,,"),  ("*,*:*,,")};
  Factors factors;
  for (auto &policy : policies)
    EXPECT_TRUE(Policy_test::parse(policy, factors))
        << "Parsing " << policy << " was successful.";
}

/**
  Test parser with 3 factors, correct values, some spaces at incorrect positions
  to be trimmed.
 */
TEST(AuthPolicyParser, Incorrect3FactorsWithSpaces) {
  const std::vector<std::string> policies{("*:pl ugin, ,"),
                                          (" pl    ugin, , "),
                                          ("*:p lugin, plugi\tn,"),
                                          ("\t\tplu\tgin,*,"),
                                          ("*,plugin,*:p lugin"),
                                          ("*,plugin,         plu gin"),
                                          (" plugin,plugin,*:p lugin"),
                                          ("p lugin,plugi n,pl ugin")};
  Factors factors;

  for (auto &policy : policies) {
    bool is_factor_with_space(false);
    EXPECT_FALSE(Policy_test::parse(policy, factors))
        << "Parsing " << policy << " failed.";
    EXPECT_EQ(factors.size(), 3)
        << "Policy " << policy << " has unexpected number of factors.";
    for (auto &factor : factors) {
      if (!factor.is_optional() &&
          factor.get_mandatory_or_default_plugin().find_first_of(" \t") !=
              std::string::npos)
        is_factor_with_space = true;
    }
    EXPECT_TRUE(is_factor_with_space);
  }
}

}  // namespace authentication_policy_unittest
