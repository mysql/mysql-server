/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "compatibility_module.h"

namespace compatibility_module_unittest {

class CompatibilityModuleTest : public ::testing::Test
{
protected:
  CompatibilityModuleTest() {};

  virtual void SetUp()
  {
    local_version= new Member_version(0x010203); //version: 1.2.3

    module= new Compatibility_module(*local_version);
  }

  virtual void TearDown()
  {
    delete local_version;
    delete module;
  }

  Compatibility_module* module;
  Member_version *local_version;
};

TEST_F(CompatibilityModuleTest, CheckCompatibleBySameVersion)
{
  Member_version member1(0x010203); //version: 1.2.3

  //Both members have the same version
  Compatibility_type ret= module->check_incompatibility(*local_version, member1);

  ASSERT_EQ(COMPATIBLE, ret);
}

TEST_F(CompatibilityModuleTest, AddIncompatibility)
{
  Member_version member1(0x010203); //version: 1.2.3
  Member_version member2(0x010204); //version: 1.2.4

  module->add_incompatibility(member1, member2);
}

TEST_F(CompatibilityModuleTest, AddIncompatibilityAndFailByIt)
{
  Member_version member1(0x010203); //version: 1.2.3
  Member_version member2(0x010204); //version: 1.2.4

  module->add_incompatibility(member1, member2);

  //The rule forces the members to be incompatible
  Compatibility_type ret= module->check_incompatibility(member1, member2);

  ASSERT_EQ(INCOMPATIBLE, ret);
}

TEST_F(CompatibilityModuleTest, AddIncompatibilityRangeAndFailByIt)
{
  Member_version member1(0x010205); //version: 1.2.5
  Member_version min_incomp_version(0x010201); //version: 1.2.1
  Member_version max_incomp_version(0x010204); //version: 1.2.4

  module->add_incompatibility(member1, min_incomp_version, max_incomp_version);

  Member_version member2(0x010204);

  //The rule forces the members to be incompatible
  Compatibility_type ret= module->check_incompatibility(member1, member2);

  ASSERT_EQ(INCOMPATIBLE, ret);

  Member_version member3(0x010201);

  //The rule forces the members to be incompatible
  ret= module->check_incompatibility(member1, member3);

  ASSERT_EQ(INCOMPATIBLE, ret);

  Member_version member4(0x010202);

  //The rule forces the members to be incompatible
  ret= module->check_incompatibility(member1, member4);

  ASSERT_EQ(INCOMPATIBLE, ret);

  Member_version member5(0x010200);

  //The rule does not forces the members to be incompatible
  ret= module->check_incompatibility(member1, member5);

  ASSERT_EQ(COMPATIBLE, ret);

  Member_version member6(0x010206);

  //The rule does not forces the members to be incompatible
  ret= module->check_incompatibility(member1, member6);

  ASSERT_EQ(COMPATIBLE, ret);
}

TEST_F(CompatibilityModuleTest, ReadCompatibility)
{
  Member_version member1(0x010203); //version: 1.2.3
  Member_version member2(0x020204); //version: 2.2.4

  //Member 2 has a higher major version so it is read compatible
  Compatibility_type ret= module->check_incompatibility(member2, member1);

  ASSERT_EQ(READ_COMPATIBLE, ret);
}

TEST_F(CompatibilityModuleTest, Incompatibility)
{
  Member_version member1(0x010203); //version: 1.2.3
  Member_version member2(0x010204); //version: 1.2.4

  //Both members have the same major/minor version, so they are compatible
  Compatibility_type ret= module->check_incompatibility(member1, member2);

  ASSERT_EQ(COMPATIBLE, ret);
}

}
