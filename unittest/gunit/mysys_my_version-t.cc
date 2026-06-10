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

#include <string>

#include "my_sys.h"

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
  const UpgradeCase cases[] = {/* Recipient in next LTS of Donor */
                               {"10.7.0", "9.7.0", true, true, true},
                               {"10.7.3", "9.7.5", true, true, true},

                               /* Either one is not LTS */
                               {"10.3.0", "9.7.0", false, true, false},
                               {"10.7.0", "9.6.0", true, false, false},

                               /* Either one in LTS before backport */
                               {"9.7.0", "8.4.0", true, true, false},
                               {"8.4.0", "8.0.0", true, true, false},
                               {"9.7.0", "8.0.0", true, true, false},

                               /* Recipient in later LTS */
                               {"10.7.0", "8.4.0", true, true, false},
                               {"11.7.0", "9.7.0", true, true, false},

                               /* Downgrade to previous LTS */
                               {"9.7.0", "10.7.0", true, true, false},
                               {"10.7.0", "11.7.0", true, true, false}};

  for (const auto &tc : cases) {
    EXPECT_EQ(tc.expected, are_versions_clone_compatible(
                               std::string{tc.recipient_version},
                               std::string{tc.donor_version},
                               tc.is_recipient_lts, tc.is_donor_lts))
        << "recipient=" << tc.recipient_version << " donor=" << tc.donor_version
        << " recipient_lts=" << tc.is_recipient_lts
        << " donor_lts=" << tc.is_donor_lts;
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
    EXPECT_EQ(tc.expected, are_versions_clone_compatible(
                               std::string{tc.recipient_version},
                               std::string{tc.donor_version},
                               tc.is_recipient_lts, tc.is_donor_lts))
        << "recipient=" << tc.recipient_version << " donor=" << tc.donor_version
        << " recipient_lts=" << tc.is_recipient_lts
        << " donor_lts=" << tc.is_donor_lts;
  }
}
