/* Copyright (c) 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <cstring>

#include <gtest/gtest.h>

#include "my_inttypes.h"
#include "sql/dd/impl/bootstrap/server_version_transition.h"

namespace dd_server_version_transition_unittest {

using Transition = dd::bootstrap::Server_version_transition;
using Result = Transition::Result;

constexpr uint version(uint major, uint minor, uint patch) {
  return major * 10000 + minor * 100 + patch;
}

constexpr uint k_no_previous_lts = 0;
constexpr uint k_lts_9_7 = version(9, 7, 0);
constexpr uint k_lts_28_4 = version(28, 4, 0);
constexpr uint k_lts_30_4 = version(30, 4, 0);

My_server_version source(uint version, const char *maturity,
                         uint previous_lts = 0, uint upgrade_threshold = 0,
                         uint downgrade_threshold = 0) {
  return {.version = version,
          .is_lts = std::strcmp(maturity, "LTS") == 0,
          .previous_lts = previous_lts,
          .upgrade_threshold = upgrade_threshold,
          .downgrade_threshold = downgrade_threshold};
}

Result evaluate(uint source_version, const char *maturity, uint target_version,
                uint source_previous_lts = 0, uint target_previous_lts = 0,
                uint upgrade_threshold = 0, uint downgrade_threshold = 0) {
  const Transition transition{
      source(source_version, maturity, source_previous_lts, upgrade_threshold,
             downgrade_threshold),
      {.version = target_version, .previous_lts = target_previous_lts}};
  return transition.evaluate();
}

struct Transition_case {
  const char *m_name;
  uint m_source_version;
  const char *m_source_maturity;
  uint m_target_version;
  uint m_source_previous_lts;
  uint m_target_previous_lts;
  uint m_upgrade_threshold;
  uint m_downgrade_threshold;
  Result m_expected_result;
};

void expect_result(const Transition_case &test_case) {
  SCOPED_TRACE(test_case.m_name);
  EXPECT_EQ(
      test_case.m_expected_result,
      evaluate(test_case.m_source_version, test_case.m_source_maturity,
               test_case.m_target_version, test_case.m_source_previous_lts,
               test_case.m_target_previous_lts, test_case.m_upgrade_threshold,
               test_case.m_downgrade_threshold));
}

TEST(ServerVersionTransitionTest, UpgradeTransitions) {
  const Transition_case test_cases[] = {
      {.m_name = "9.7 LTS can enter first calendar Innovation release",
       .m_source_version = version(9, 7, 5),
       .m_source_maturity = "LTS",
       .m_target_version = version(26, 7, 0),
       .m_source_previous_lts = k_no_previous_lts,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "9.7 LTS can enter first calendar Innovation patch release",
       .m_source_version = version(9, 7, 5),
       .m_source_maturity = "LTS",
       .m_target_version = version(26, 7, 1),
       .m_source_previous_lts = k_no_previous_lts,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "9.7 LTS can move to later release in first calendar lineage",
       .m_source_version = version(9, 7, 5),
       .m_source_maturity = "LTS",
       .m_target_version = version(26, 10, 0),
       .m_source_previous_lts = k_no_previous_lts,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "9.7 LTS can move to first calendar LTS",
       .m_source_version = version(9, 7, 5),
       .m_source_maturity = "LTS",
       .m_target_version = version(28, 4, 0),
       .m_source_previous_lts = k_no_previous_lts,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "8.4 LTS cannot enter the first calendar lineage",
       .m_source_version = version(8, 4, 5),
       .m_source_maturity = "LTS",
       .m_target_version = version(26, 7, 0),
       .m_source_previous_lts = k_no_previous_lts,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::INVALID_SERVER_UPGRADE_NOT_LTS},
      {.m_name = "9.7 LTS cannot skip the first calendar lineage",
       .m_source_version = version(9, 7, 5),
       .m_source_maturity = "LTS",
       .m_target_version = version(28, 10, 0),
       .m_source_previous_lts = k_no_previous_lts,
       .m_target_previous_lts = k_lts_28_4,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::INVALID_SERVER_UPGRADE_SKIPS_LTS_LINEAGE},
      {.m_name = "9.7 LTS cannot jump to a later calendar LTS",
       .m_source_version = version(9, 7, 5),
       .m_source_maturity = "LTS",
       .m_target_version = version(30, 4, 0),
       .m_source_previous_lts = k_no_previous_lts,
       .m_target_previous_lts = k_lts_28_4,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::INVALID_SERVER_UPGRADE_SKIPS_LTS_LINEAGE},

      {.m_name = "Upgrade threshold accepts the matching target",
       .m_source_version = version(9, 7, 5),
       .m_source_maturity = "LTS",
       .m_target_version = version(26, 10, 0),
       .m_source_previous_lts = k_no_previous_lts,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = version(26, 10, 0),
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "Upgrade threshold rejects an older target",
       .m_source_version = version(9, 7, 5),
       .m_source_maturity = "LTS",
       .m_target_version = version(26, 7, 0),
       .m_source_previous_lts = k_no_previous_lts,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = version(26, 10, 0),
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::BEYOND_SERVER_UPGRADE_THRESHOLD},

      {.m_name = "Innovation release can upgrade within the same lineage",
       .m_source_version = version(26, 7, 0),
       .m_source_maturity = "INNOVATION",
       .m_target_version = version(27, 1, 0),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "Innovation release can upgrade to LTS in the same lineage",
       .m_source_version = version(26, 7, 0),
       .m_source_maturity = "INNOVATION",
       .m_target_version = version(28, 4, 0),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "Innovation release can upgrade to a patch in same lineage",
       .m_source_version = version(26, 7, 0),
       .m_source_maturity = "INNOVATION",
       .m_target_version = version(27, 1, 3),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "Innovation release cannot cross an LTS boundary",
       .m_source_version = version(28, 1, 0),
       .m_source_maturity = "INNOVATION",
       .m_target_version = version(28, 10, 0),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_28_4,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::INVALID_SERVER_UPGRADE_NOT_LTS},

      {.m_name = "LTS release can upgrade within the same YY.M release base",
       .m_source_version = version(28, 4, 0),
       .m_source_maturity = "LTS",
       .m_target_version = version(28, 4, 4),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "LTS release can upgrade to next lineage Innovation release",
       .m_source_version = version(28, 4, 0),
       .m_source_maturity = "LTS",
       .m_target_version = version(28, 10, 0),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_28_4,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "LTS patch can upgrade to next lineage Innovation release",
       .m_source_version = version(28, 4, 4),
       .m_source_maturity = "LTS",
       .m_target_version = version(28, 10, 0),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_28_4,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "LTS release can upgrade to next lineage LTS",
       .m_source_version = version(28, 4, 0),
       .m_source_maturity = "LTS",
       .m_target_version = version(30, 4, 0),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_28_4,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "LTS release cannot skip beyond the next lineage",
       .m_source_version = version(28, 4, 0),
       .m_source_maturity = "LTS",
       .m_target_version = version(32, 4, 0),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_30_4,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::INVALID_SERVER_UPGRADE_NOT_LTS},

      {.m_name = "Upgrade threshold can allow a target later than threshold",
       .m_source_version = version(28, 4, 0),
       .m_source_maturity = "LTS",
       .m_target_version = version(29, 1, 0),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_28_4,
       .m_upgrade_threshold = version(28, 10, 0),
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "Upgrade threshold can reject a next-lineage target",
       .m_source_version = version(28, 4, 0),
       .m_source_maturity = "LTS",
       .m_target_version = version(28, 10, 0),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_28_4,
       .m_upgrade_threshold = version(29, 1, 0),
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::BEYOND_SERVER_UPGRADE_THRESHOLD},
  };

  for (const auto &test_case : test_cases) expect_result(test_case);
}

TEST(ServerVersionTransitionTest, DowngradeTransitions) {
  const Transition_case test_cases[] = {
      {.m_name =
           "LTS patch downgrade is accepted without a restrictive threshold",
       .m_source_version = version(28, 4, 4),
       .m_source_maturity = "LTS",
       .m_target_version = version(28, 4, 2),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = 0,
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "LTS patch downgrade is accepted at the downgrade threshold",
       .m_source_version = version(28, 4, 4),
       .m_source_maturity = "LTS",
       .m_target_version = version(28, 4, 2),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = version(28, 4, 2),
       .m_expected_result = Result::ACCEPTED},
      {.m_name = "LTS patch downgrade below the threshold is rejected",
       .m_source_version = version(28, 4, 4),
       .m_source_maturity = "LTS",
       .m_target_version = version(28, 4, 1),
       .m_source_previous_lts = k_lts_9_7,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = version(28, 4, 2),
       .m_expected_result = Result::BEYOND_SERVER_DOWNGRADE_THRESHOLD},
      {.m_name = "Innovation downgrade across YY.M release bases is rejected",
       .m_source_version = version(28, 10, 4),
       .m_source_maturity = "INNOVATION",
       .m_target_version = version(28, 4, 0),
       .m_source_previous_lts = k_lts_28_4,
       .m_target_previous_lts = k_lts_9_7,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = version(28, 10, 4),
       .m_expected_result = Result::INVALID_SERVER_DOWNGRADE_NOT_PATCH},
      {.m_name = "Innovation patch downgrade is rejected",
       .m_source_version = version(28, 10, 4),
       .m_source_maturity = "INNOVATION",
       .m_target_version = version(28, 10, 3),
       .m_source_previous_lts = k_lts_28_4,
       .m_target_previous_lts = k_lts_28_4,
       .m_upgrade_threshold = 0,
       .m_downgrade_threshold = version(28, 10, 4),
       .m_expected_result = Result::NO_PATCH_DOWNGRADE_FOR_INNOVATION_RELEASES},
  };

  for (const auto &test_case : test_cases) expect_result(test_case);
}

TEST(ServerVersionTransitionTest, NonCalendarSourceCannotDefinePreviousLts) {
#ifndef NDEBUG
  My_server_version target;
  target.version = version(26, 7, 0);
  target.previous_lts = k_lts_9_7;

  const My_server_version legacy_source =
      source(version(9, 7, 5), "LTS", k_lts_9_7);

  EXPECT_DEATH(
      {
        const Transition transition(legacy_source, target);
        (void)transition;
      },
      "is_valid_release_version");
#else
  GTEST_SKIP() << "Assertions are disabled in this build.";
#endif
}

}  // namespace dd_server_version_transition_unittest
