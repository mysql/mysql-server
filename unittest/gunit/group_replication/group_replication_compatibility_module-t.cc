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

TEST_F(CompatibilityModuleTest, isLTS) {
  Member_version server_080037(0x080037);  // version: 8.0.37
  Member_version server_080300(0x080300);  // version: 8.3.0
  Member_version server_080400(0x080400);  // version: 8.4.0
  Member_version server_080401(0x080401);  // version: 8.4.1
  Member_version server_080499(0x080499);  // version: 8.4.99
  Member_version server_090000(0x090000);  // version: 9.0.0

  std::set<Member_version> all_versions;
  all_versions.insert(server_080401);
  all_versions.insert(server_080400);
  all_versions.insert(server_080499);
  ASSERT_TRUE(Compatibility_module::do_all_versions_belong_to_the_same_lts(
      all_versions));

  all_versions.clear();
  all_versions.insert(server_080300);
  all_versions.insert(server_080400);
  ASSERT_FALSE(Compatibility_module::do_all_versions_belong_to_the_same_lts(
      all_versions));

  all_versions.clear();
  all_versions.insert(server_080400);
  all_versions.insert(server_090000);
  ASSERT_FALSE(Compatibility_module::do_all_versions_belong_to_the_same_lts(
      all_versions));

  all_versions.clear();
  all_versions.insert(server_080400);
  all_versions.insert(server_080037);
  ASSERT_FALSE(Compatibility_module::do_all_versions_belong_to_the_same_lts(
      all_versions));
}

TEST_F(CompatibilityModuleTest, LTSCompatibility) {
  Member_version server_080300(0x080300);  // version: 8.3.0
  Member_version server_080400(0x080400);  // version: 8.4.0
  Member_version server_080401(0x080401);  // version: 8.4.1
  Member_version server_080410(0x080410);  // version: 8.4.10
  Member_version server_080420(0x080420);  // version: 8.4.20
  Member_version server_080442(0x080442);  // version: 8.4.42
  Member_version server_080499(0x080499);  // version: 8.4.99
  Member_version server_090000(0x090000);  // version: 9.0.0

  /*
    Group with 8.4.1
    Try to add a 8.3.0
  */
  std::set<Member_version> all_versions;
  all_versions.insert(server_080401);
  all_versions.insert(server_080300);
  Compatibility_type ret = module->check_incompatibility(
      server_080300, server_080401, true, all_versions);
  ASSERT_EQ(INCOMPATIBLE_LOWER_VERSION, ret);

  /*
    Group with 8.4.1
    Try to add a 8.4.0
  */
  all_versions.clear();
  all_versions.insert(server_080401);
  all_versions.insert(server_080400);
  ret = module->check_incompatibility(server_080400, server_080401, true,
                                      all_versions);
  ASSERT_EQ(COMPATIBLE, ret);

  /*
    Group with 8.4.20, 8.4.42, 8.4.99
    Try to add a 8.4.10
  */
  all_versions.clear();
  all_versions.insert(server_080420);
  all_versions.insert(server_080442);
  all_versions.insert(server_080499);
  all_versions.insert(server_080410);

  ret = module->check_incompatibility(server_080410, server_080420, true,
                                      all_versions);
  ASSERT_EQ(COMPATIBLE, ret);

  ret = module->check_incompatibility(server_080410, server_080442, true,
                                      all_versions);
  ASSERT_EQ(COMPATIBLE, ret);

  ret = module->check_incompatibility(server_080410, server_080499, true,
                                      all_versions);
  ASSERT_EQ(COMPATIBLE, ret);

  /*
    Group with 8.4.1, 9.0.0
    Try to add a 8.4.0
  */
  all_versions.clear();
  all_versions.insert(server_080401);
  all_versions.insert(server_090000);
  all_versions.insert(server_080400);

  ret = module->check_incompatibility(server_080400, server_080401, true,
                                      all_versions);
  ASSERT_EQ(INCOMPATIBLE_LOWER_VERSION, ret);

  ret = module->check_incompatibility(server_080400, server_090000, true,
                                      all_versions);
  ASSERT_EQ(INCOMPATIBLE_LOWER_VERSION, ret);
}

}  // namespace compatibility_module_unittest
