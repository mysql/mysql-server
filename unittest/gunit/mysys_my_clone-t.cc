/* Copyright (c) 2026 Oracle and/or its affiliates.

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

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "my_clone.h"

namespace {
struct NonUpgradeCase {
  const char *ver1;
  const char *ver2;
  bool expected;
};

struct UpgradeCase {
  const char *recipient_version;
  const char *donor_version;
  const bool is_recipient_lts;
  const bool is_donor_lts;
  bool expected;
  std::optional<std::string> recipient_prev_lts = std::nullopt;
};
}  // namespace

TEST(MysysMyVersion, NonUpgradeCase_CloneWithinLTS) {
  const NonUpgradeCase cases[] = {
      /* clone_within_lts_version_match */
      {"9.6.0", "9.6.0", true},
      {"8.4.8", "8.4.8", true},
      {"8.0.45", "8.0.45", true},

      /* clone_within_lts_major_mismatch */
      {"8.0.45", "9.6.0", false},
      {"8.4.8", "9.6.0", false},

      /* clone_within_lts_minor_mismatch */
      {"8.0.45", "8.4.8", false},
      {"8.4.45", "8.3.0", false},
      {"8.1.0", "8.2.0", false},

      /* clone_within_lts_non_8_0_patch_mismatch */
      {"8.4.0", "8.4.8", true},
      {"8.4.8", "8.4.0", true},
      {"9.7.0", "9.7.1", true},

      /* clone_within_lts_8_0_patch_match */
      {"8.0.30", "8.0.30", true},
      {"8.0.25", "8.0.25-debug", true},

      /* clone_within_lts_8_0_before_backport_patch_mismatch */
      {"8.0.6", "8.0.7", false},
      {"8.0.34", "8.0.35", false},
      {"8.0.38", "8.0.35", false},

      /* clone_within_lts_8_0_after_backport_patch_mismatch */
      {"8.0.38", "8.0.37", true},
      {"8.0.37", "8.0.38", true}};

  for (const auto &tc : cases) {
    EXPECT_EQ(tc.expected, are_versions_clone_compatible(std::string{tc.ver1},
                                                         std::string{tc.ver2}))
        << "recipient=" << tc.ver1 << " donor=" << tc.ver2;
  }
}

TEST(MysysMyVersion, NonUpgradeCase_Sanity) {
  const NonUpgradeCase cases[] = {/* Invalid characters in Version => false */
                                  {"8.0.a", "8.0.37", false},
                                  {"8.b.37", "8.0.37", false},
                                  {"c.0.37", "8.0.37", false},

                                  /* Incomplete Version */
                                  {"8.0", "8.0.37", false}};

  for (const auto &tc : cases) {
    EXPECT_EQ(tc.expected, are_versions_clone_compatible(std::string{tc.ver1},
                                                         std::string{tc.ver2}))
        << "ver1=" << tc.ver1 << " ver2=" << tc.ver2;
  }
}

TEST(MysysMyVersion, UpgradeCase_CloneToNextLTS) {
  const UpgradeCase cases[] = {
      /* Recipient in next LTS of Donor */
      {"28.4.0", "9.7.0", true, true, true, "9.7.0"},
      {"28.4.3", "9.7.5", true, true, true, "9.7.0"},

      /* Either one is not LTS */
      {"28.1.0", "9.7.0", false, true, false, "9.7.0"},
      {"28.4.0", "9.6.0", true, false, false, "9.7.0"},

      /* Either one in LTS before backport */
      {"9.7.0", "8.4.0", true, true, false},
      {"8.4.0", "8.0.0", true, true, false},
      {"9.7.0", "8.0.0", true, true, false},

      /* Recipient in later LTS */
      {"28.4.0", "8.4.0", true, true, false, "9.7.0"},
      {"30.4.0", "9.7.0", true, true, false, "28.4.0"},

      /* Downgrade to previous LTS */
      {"9.7.0", "28.4.0", true, true, false, "9.7.0"},
      {"28.4.0", "30.4.0", true, true, false, "9.7.0"},

      /* All sanity cases */
      {"28.7.0", "28.7.0", false, false, true, "28.4.0"},
      {"28.1.25", "28.1.25-debug", false, false, true, "9.7.0"},
      {"26.7.5", "26.7.0", false, false, true, "9.7.0"},
      {"26.7.2", "26.7.4", false, false, true, "9.7.0"},
      {"26.10.0", "26.7.0", false, false, false, "9.7.0"},
      {"28.4.0", "9.7.5", true, true, true, "9.7.0"},
      {"30.4.1", "28.4.4", true, true, true, "28.4.0"},
      {"30.4.4", "28.4.1", true, true, true, "28.4.0"},
      {"9.7.1", "28.4.4", true, true, false},
      {"28.4.1", "8.4.4", true, true, false, "9.7.0"},
      {"28.4.1", "30.4.4", true, true, false, "9.7.0"},
      {"9.7.0", "30.4.4", true, true, false},
      {"30.4.1", "9.7.4", true, true, false, "28.4.0"},
      {"32.4.1", "28.4.4", true, true, false, "30.4.0"},
      {"32.4.1", "9.7.4", true, true, false, "30.4.0"},
      {"28.10.2", "28.7.1", false, false, false, "28.4.0"},
      {"28.7.1", "28.10.2", false, false, false, "28.4.0"},
      {"28.4.2", "28.1.1", true, false, false, "9.7.0"},
      {"28.1.1", "28.4.2", false, true, false, "9.7.0"},
      {"28.4.0", "28.4.0", true, true, true, "9.7.0"},
      {"28.4.0", "9.7.0", true, true, false},
      {"9.7.0", "28.4.0", true, true, false, "9.7.0"},
      {"invalid", "invalid", false, false, true},
      {"28.4.0", "28.4.x", true, true, false, "9.7.0"},
      {"invalid", "28.4.0", false, true, false},
      {"28.4.0", "9.7.0", true, true, false},
      {"8.0.37", "8.0.36", false, false, false},
      {"8.0.36", "8.0.35", false, false, false},
      {"8.0.38", "8.0.37", false, false, true},
      {"8.0.25", "8.0.25-debug", false, false, true},

      {"4294967297.4294967296.4294967291", "4294967292.4294967293.4294967294",
       true, true, false, "4294967292.4294967293.4294967294"},
      {"4294967324.4294967324.4294967323", "4294967324.4294967324.4294967324",
       true, true, false, "4294967324.4294967324.4294967324"},
      {"99.99.99", "99.98.99", true, true, false, "99.99.99"},
      {"99.99.99", "99.98.99", true, true, true, "99.98.99"}};

  for (const auto &tc : cases) {
    EXPECT_EQ(
        tc.expected,
        are_versions_clone_compatible(
            std::string{tc.recipient_version}, std::string{tc.donor_version},
            tc.is_recipient_lts, tc.is_donor_lts, tc.recipient_prev_lts))
        << "recipient=" << tc.recipient_version << " donor=" << tc.donor_version
        << " recipient_lts=" << tc.is_recipient_lts
        << " donor_lts=" << tc.is_donor_lts << " recipient_prev_lts="
        << (tc.recipient_prev_lts ? *tc.recipient_prev_lts : "<missing>");
  }
}

TEST(MysysMyVersion, UpgradeCase_Sanity) {
  const UpgradeCase cases[] = {
      /* Invalid version */
      {"9.7", "8.0.99", false, false, false},
      {"9.7.0", "8.0", false, false, false},
      {"9.7.a", "8.0.99", false, false, false},
      {"9.7.0", "8.0.a", false, false, false},
  };

  for (const auto &tc : cases) {
    EXPECT_EQ(
        tc.expected,
        are_versions_clone_compatible(
            std::string{tc.recipient_version}, std::string{tc.donor_version},
            tc.is_recipient_lts, tc.is_donor_lts, tc.recipient_prev_lts))
        << "recipient=" << tc.recipient_version << " donor=" << tc.donor_version
        << " recipient_lts=" << tc.is_recipient_lts
        << " donor_lts=" << tc.is_donor_lts << " recipient_prev_lts="
        << (tc.recipient_prev_lts ? *tc.recipient_prev_lts : "<missing>");
  }
}
