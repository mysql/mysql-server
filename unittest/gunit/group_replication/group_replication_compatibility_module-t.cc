/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/compatibility_module.h"

namespace compatibility_module_unittest {

class CompatibilityModuleTest : public ::testing::Test {
 protected:
  CompatibilityModuleTest() = default;

  void SetUp() override {
    local_version = new Member_version(0x010203);  // version: 1.2.3

    module = new Compatibility_module(*local_version);
  }

  void TearDown() override {
    delete local_version;
    delete module;
  }

  Compatibility_module *module;
  Member_version *local_version;
};

TEST_F(CompatibilityModuleTest, CheckCompatibleBySameVersion) {
  Member_version member1(0x010203);  // version: 1.2.3
  std::set<Member_version> all_versions;
  all_versions.insert(*local_version);
  all_versions.insert(member1);

  // Both members have the same version
  Compatibility_type ret = module->check_incompatibility(
      *local_version, member1, true, all_versions);

  ASSERT_EQ(COMPATIBLE, ret);
}

TEST_F(CompatibilityModuleTest, AddIncompatibility) {
  Member_version member1(0x010203);  // version: 1.2.3
  Member_version member2(0x010204);  // version: 1.2.4

  module->add_incompatibility(member1, member2);
}

TEST_F(CompatibilityModuleTest, AddIncompatibilityAndFailByIt) {
  Member_version member1(0x010203);  // version: 1.2.3
  Member_version member2(0x010204);  // version: 1.2.4
  std::set<Member_version> all_versions;
  all_versions.insert(member1);
  all_versions.insert(member2);

  module->add_incompatibility(member1, member2);

  // The rule forces the members to be incompatible
  Compatibility_type ret =
      module->check_incompatibility(member1, member2, true, all_versions);

  ASSERT_EQ(INCOMPATIBLE, ret);

  Member_version member3(0x020203);  // version: 2.2.3
  all_versions.insert(member3);
  Member_version min_range(0x020200);  // version: 2.2.0
  Member_version max_range(0x020205);  // version: 2.2.5
  // Add rule 1.2.3 is incompatible with version range 2.2.0 - 2.2.5
  module->add_incompatibility(member1, min_range, max_range);

  // The rule forces the members to be incompatible
  // Member 1 is also INCOMPATIBLE_LOWER_VERSION with Member 3.
  // INCOMPATIBLE is returned due to rule, version is not checked.
  // Rule takes precedence over version.
  ret = module->check_incompatibility(member1, member3, true, all_versions);

  ASSERT_EQ(INCOMPATIBLE, ret);
}

TEST_F(CompatibilityModuleTest, AddIncompatibilityRangeAndFailByIt) {
  Member_version member1(0x010205);             // version: 1.2.5
  Member_version min_incomp_version(0x010201);  // version: 1.2.1
  Member_version max_incomp_version(0x010204);  // version: 1.2.4

  module->add_incompatibility(member1, min_incomp_version, max_incomp_version);

  Member_version member2(0x010204);
  std::set<Member_version> all_versions;
  all_versions.insert(member1);
  all_versions.insert(member2);

  // The rule forces the members to be incompatible
  Compatibility_type ret =
      module->check_incompatibility(member1, member2, true, all_versions);

  ASSERT_EQ(INCOMPATIBLE, ret);

  Member_version member3(0x010201);
  all_versions.insert(member3);

  // The rule forces the members to be incompatible
  ret = module->check_incompatibility(member1, member3, true, all_versions);

  ASSERT_EQ(INCOMPATIBLE, ret);

  Member_version member4(0x010202);
  all_versions.insert(member4);

  // The rule forces the members to be incompatible
  ret = module->check_incompatibility(member1, member4, true, all_versions);

  ASSERT_EQ(INCOMPATIBLE, ret);

  Member_version member5(0x010200);
  all_versions.insert(member5);

  // Patch version 3 is higher then patch version 0, its read compatible
  ret = module->check_incompatibility(member1, member5, true, all_versions);

  ASSERT_EQ(READ_COMPATIBLE, ret);

  Member_version member6(0x010206);
  all_versions.insert(member6);

  // Patch version 3 is lower then patch version 6, its incompatible lower
  // version
  ret = module->check_incompatibility(member1, member6, true, all_versions);

  ASSERT_EQ(INCOMPATIBLE_LOWER_VERSION, ret);
}

TEST_F(CompatibilityModuleTest, ReadCompatibility) {
  Member_version member1(0x010203);  // version: 1.2.3
  Member_version member2(0x020204);  // version: 2.2.4
  std::set<Member_version> all_versions;
  all_versions.insert(member1);
  all_versions.insert(member2);

  // Member 2 has a higher major version so it is read compatible
  Compatibility_type ret =
      module->check_incompatibility(member2, member1, true, all_versions);

  ASSERT_EQ(READ_COMPATIBLE, ret);
}

TEST_F(CompatibilityModuleTest, Incompatibility) {
  Member_version member1(0x010203);  // version: 1.2.3
  Member_version member2(0x010204);  // version: 1.2.4
  std::set<Member_version> all_versions;
  all_versions.insert(member1);
  all_versions.insert(member2);

  // Member 1 has lower patch version, so its incompatible lower version
  Compatibility_type ret =
      module->check_incompatibility(member1, member2, true, all_versions);

  ASSERT_EQ(INCOMPATIBLE_LOWER_VERSION, ret);

  Member_version member3(0x020203);  // version: 2.2.3
  all_versions.insert(member3);

  // Member1 has lower major version then Member3 so it INCOMPATIBLE
  ret = module->check_incompatibility(member1, member3, true, all_versions);

  ASSERT_EQ(INCOMPATIBLE_LOWER_VERSION, ret);

  // Member1 has lower major version then Member3.
  // Check is not done since do_version_check is false. COMPATIBLE is returned
  ret = module->check_incompatibility(member1, member3, false, all_versions);

  ASSERT_EQ(COMPATIBLE, ret);
}

TEST_F(CompatibilityModuleTest, LTS80) {
  Member_version server_8034(0x080034);  // version: 8.0.34
  Member_version server_8035(0x080035);  // version: 8.0.35
  Member_version server_8036(0x080036);  // version: 8.0.36
  Member_version server_8037(0x080037);  // version: 8.0.37
  Member_version server_8038(0x080038);  // version: 8.0.38
  Member_version server_8100(0x080100);  // version: 8.1.0

  /*
    Group with 8.0.36
    Try to add a 8.0.34
  */
  std::set<Member_version> all_versions;
  all_versions.insert(server_8036);
  all_versions.insert(server_8034);
  Compatibility_type ret = module->check_incompatibility(
      server_8034, server_8036, true, all_versions);
  ASSERT_EQ(INCOMPATIBLE_LOWER_VERSION, ret);

  /*
    Group with 8.0.36
    Try to add a 8.0.35
  */
  all_versions.clear();
  all_versions.insert(server_8036);
  all_versions.insert(server_8035);
  ret = module->check_incompatibility(server_8035, server_8036, true,
                                      all_versions);
  ASSERT_EQ(COMPATIBLE, ret);

  /*
    Group with 8.0.36, 8.0.37, 8.0.38
    Try to add a 8.0.35
  */
  all_versions.clear();
  all_versions.insert(server_8036);
  all_versions.insert(server_8037);
  all_versions.insert(server_8038);
  all_versions.insert(server_8035);

  ret = module->check_incompatibility(server_8035, server_8036, true,
                                      all_versions);
  ASSERT_EQ(COMPATIBLE, ret);

  ret = module->check_incompatibility(server_8035, server_8037, true,
                                      all_versions);
  ASSERT_EQ(COMPATIBLE, ret);

  ret = module->check_incompatibility(server_8035, server_8038, true,
                                      all_versions);
  ASSERT_EQ(COMPATIBLE, ret);

  /*
    Group with 8.0.36, 8.1.0
    Try to add a 8.0.35
  */
  all_versions.clear();
  all_versions.insert(server_8036);
  all_versions.insert(server_8100);
  all_versions.insert(server_8035);

  ret = module->check_incompatibility(server_8035, server_8036, true,
                                      all_versions);
  ASSERT_EQ(INCOMPATIBLE_LOWER_VERSION, ret);

  ret = module->check_incompatibility(server_8035, server_8100, true,
                                      all_versions);
  ASSERT_EQ(INCOMPATIBLE_LOWER_VERSION, ret);
}

}  // namespace compatibility_module_unittest
