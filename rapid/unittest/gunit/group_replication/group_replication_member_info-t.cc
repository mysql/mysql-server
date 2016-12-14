/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "member_info.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace gcs_member_info_unittest {

class ClusterMemberInfoTest : public ::testing::Test
{
protected:
  ClusterMemberInfoTest() { };

  virtual void SetUp()
  {
    string hostname("pc_hostname");
    string uuid("781f947c-db4a-11e3-98d1-f01faf1a1c44");
    uint port= 4444;
    uint plugin_version= 0x000400;
    uint write_set_algorithm= 1;
    string executed_gtid("aaaa:1-10");
    string retrieved_gtid("bbbb:1-10");
    ulonglong gtid_assignment_block_size= 9223372036854775807ULL;
    bool in_primary_mode= false;
    bool has_enforces_update_everywhere_checks= false;

    gcs_member_id= new Gcs_member_identifier("stuff");

    Group_member_info::Group_member_status status=
        Group_member_info::MEMBER_OFFLINE;

    Member_version local_member_plugin_version(plugin_version);
    local_node= new Group_member_info((char*)hostname.c_str(), port,
                                      (char*)uuid.c_str(),write_set_algorithm,
                                      gcs_member_id->get_member_id(), status,
                                      local_member_plugin_version,
                                      gtid_assignment_block_size,
                                      Group_member_info::MEMBER_ROLE_PRIMARY,
                                      in_primary_mode,
                                      has_enforces_update_everywhere_checks);
    local_node->update_gtid_sets(executed_gtid,retrieved_gtid);
  }

  virtual void TearDown()
  {
    delete gcs_member_id;
    delete local_node;
  }

  Group_member_info* local_node;
  Gcs_member_identifier* gcs_member_id;
};

TEST_F(ClusterMemberInfoTest, EncodeDecodeIdempotencyTest)
{
  vector<uchar>* encoded= new vector<uchar>();
  local_node->encode(encoded);

  Group_member_info decoded_local_node(&encoded->front(), encoded->size());


  ASSERT_EQ(local_node->get_port(),
            decoded_local_node.get_port());
  ASSERT_EQ(local_node->get_hostname(),
            decoded_local_node.get_hostname());
  ASSERT_EQ(local_node->get_uuid(),
            decoded_local_node.get_uuid());
  ASSERT_EQ(local_node->get_write_set_extraction_algorithm(),
            decoded_local_node.get_write_set_extraction_algorithm());
  ASSERT_EQ(local_node->get_gcs_member_id().get_member_id(),
            decoded_local_node.get_gcs_member_id().get_member_id());
  ASSERT_EQ(local_node->get_recovery_status(),
            decoded_local_node.get_recovery_status());
  ASSERT_EQ(local_node->get_member_version().get_version(),
            decoded_local_node.get_member_version().get_version());
  ASSERT_EQ(local_node->get_gtid_executed(),
            decoded_local_node.get_gtid_executed());
  ASSERT_EQ(local_node->get_gtid_retrieved(),
            decoded_local_node.get_gtid_retrieved());
  ASSERT_EQ(local_node->get_gtid_assignment_block_size(),
            decoded_local_node.get_gtid_assignment_block_size());
  ASSERT_EQ(local_node->get_role(),
            decoded_local_node.get_role());

  delete encoded;
}

class ClusterMemberInfoManagerTest : public ::testing::Test
{
protected:
  ClusterMemberInfoManagerTest() { };

  virtual void SetUp()
  {
    string hostname("pc_hostname");
    string uuid("8d7r947c-dr4a-17i3-59d1-f01faf1kkc44");
    uint port= 4444;
    uint write_set_algorithm= 1;
    uint plugin_version= 0x000400;
    gcs_member_id= new Gcs_member_identifier("stuff");
    ulonglong gtid_assignment_block_size= 9223372036854775807ULL;
    bool in_primary_mode= false;
    bool has_enforces_update_everywhere_checks= false;

    Group_member_info::Group_member_status status=
        Group_member_info::MEMBER_OFFLINE;

    Member_version local_member_plugin_version(plugin_version);
    local_node= new Group_member_info((char*)hostname.c_str(), port,
                                      (char*)uuid.c_str(), write_set_algorithm,
                                      gcs_member_id->get_member_id(), status,
                                      local_member_plugin_version,
                                      gtid_assignment_block_size,
                                      Group_member_info::MEMBER_ROLE_SECONDARY,
                                      in_primary_mode,
                                      has_enforces_update_everywhere_checks);

    cluster_member_mgr= new Group_member_info_manager(local_node);
  }

  virtual void TearDown()
  {
    delete cluster_member_mgr;
    delete gcs_member_id;
    delete local_node;
  }

  Group_member_info_manager_interface* cluster_member_mgr;
  Group_member_info* local_node;
  Gcs_member_identifier* gcs_member_id;
};

TEST_F(ClusterMemberInfoManagerTest, GetLocalInfoByUUIDTest)
{
  //Add another member info in order to make this test more realistic
  string hostname("pc_hostname2");
  string uuid("781f947c-db4a-22e3-99d4-f01faf1a1c44");
  uint port= 4444;
  uint write_set_algorithm= 1;
  uint plugin_version= 0x000400;
  Gcs_member_identifier gcs_member_id("another_stuff");
  string executed_gtid("aaaa:1-11");
  string retrieved_gtid("bbbb:1-11");
  ulonglong gtid_assignment_block_size= 9223372036854775807ULL;
  bool in_primary_mode= false;
  bool has_enforces_update_everywhere_checks= false;

  Group_member_info::Group_member_status status=
      Group_member_info::MEMBER_OFFLINE;

  Member_version local_member_plugin_version(plugin_version);
  Group_member_info* new_member= new Group_member_info((char*)hostname.c_str(),
                                                       port,
                                                       (char*)uuid.c_str(),
                                                       write_set_algorithm,
                                                       gcs_member_id.get_member_id(),
                                                       status,
                                                       local_member_plugin_version,
                                                       gtid_assignment_block_size,
                                                       Group_member_info::MEMBER_ROLE_PRIMARY,
                                                       in_primary_mode,
                                                       has_enforces_update_everywhere_checks);
  new_member->update_gtid_sets(executed_gtid,retrieved_gtid);

  cluster_member_mgr->add(new_member);

  string uuid_to_get("8d7r947c-dr4a-17i3-59d1-f01faf1kkc44");

  Group_member_info* retrieved_local_info=
      cluster_member_mgr->get_group_member_info(uuid_to_get);

  ASSERT_TRUE(retrieved_local_info != NULL);
  ASSERT_EQ(retrieved_local_info->get_uuid(),
            uuid_to_get);

  delete retrieved_local_info;
}

TEST_F(ClusterMemberInfoManagerTest, UpdateStatusOfLocalObjectTest)
{
  cluster_member_mgr->update_member_status(local_node->get_uuid(),
                                           Group_member_info::MEMBER_ONLINE);

  ASSERT_EQ(Group_member_info::MEMBER_ONLINE,
            local_node->get_recovery_status());
}

TEST_F(ClusterMemberInfoManagerTest, UpdateGtidSetsOfLocalObjectTest)
{
  string executed_gtid("aaaa:1-10");
  string retrieved_gtid("bbbb:1-10");

  cluster_member_mgr->update_gtid_sets(local_node->get_uuid(),
                                       executed_gtid,
                                       retrieved_gtid);

  ASSERT_EQ(executed_gtid,
            local_node->get_gtid_executed());
  ASSERT_EQ(retrieved_gtid,
            local_node->get_gtid_retrieved());
}

TEST_F(ClusterMemberInfoManagerTest, GetLocalInfoByUUIDAfterEncodingTest)
{
  vector<uchar>* encoded= new vector<uchar>();
  cluster_member_mgr->encode(encoded);

  vector<Group_member_info*>* decoded_members=
      cluster_member_mgr->decode(&encoded->front(), encoded->size());

  cluster_member_mgr->update(decoded_members);

  delete decoded_members;
  delete encoded;

  string uuid_to_get("8d7r947c-dr4a-17i3-59d1-f01faf1kkc44");

  Group_member_info* retrieved_local_info=
      cluster_member_mgr->get_group_member_info(uuid_to_get);

  ASSERT_TRUE(retrieved_local_info != NULL);

  ASSERT_EQ(local_node->get_port(),
            retrieved_local_info->get_port());
  ASSERT_EQ(local_node->get_hostname(),
            retrieved_local_info->get_hostname());
  ASSERT_EQ(local_node->get_uuid(),
            retrieved_local_info->get_uuid());
  ASSERT_EQ(local_node->get_gcs_member_id().get_member_id(),
            retrieved_local_info->get_gcs_member_id().get_member_id());
  ASSERT_EQ(local_node->get_recovery_status(),
            retrieved_local_info->get_recovery_status());
  ASSERT_EQ(local_node->get_write_set_extraction_algorithm(),
            retrieved_local_info->get_write_set_extraction_algorithm());
  ASSERT_EQ(local_node->get_gtid_executed(),
            retrieved_local_info->get_gtid_executed());
  ASSERT_EQ(local_node->get_gtid_retrieved(),
            retrieved_local_info->get_gtid_retrieved());
  ASSERT_EQ(local_node->get_gtid_assignment_block_size(),
            retrieved_local_info->get_gtid_assignment_block_size());
  ASSERT_EQ(local_node->get_role(),
            retrieved_local_info->get_role());

  delete retrieved_local_info;
}

TEST_F(ClusterMemberInfoManagerTest, UpdateStatusOfLocalObjectAfterExchangeTest)
{
  vector<uchar>* encoded= new vector<uchar>();
  cluster_member_mgr->encode(encoded);

  vector<Group_member_info*>* decoded_members=
      cluster_member_mgr->decode(&encoded->front(), encoded->size());

  cluster_member_mgr->update(decoded_members);

  delete decoded_members;
  delete encoded;

  cluster_member_mgr->update_member_status(local_node->get_uuid(),
                                           Group_member_info::MEMBER_ONLINE);

  ASSERT_EQ(Group_member_info::MEMBER_ONLINE,
            local_node->get_recovery_status());

  string executed_gtid("cccc:1-11");
  string retrieved_gtid("dddd:1-11");

  cluster_member_mgr->update_gtid_sets(local_node->get_uuid(),
                                       executed_gtid,
                                       retrieved_gtid);

  ASSERT_EQ(executed_gtid,
            local_node->get_gtid_executed());
  ASSERT_EQ(retrieved_gtid,
            local_node->get_gtid_retrieved());

  Group_member_info* retrieved_local_info=
      cluster_member_mgr->get_group_member_info(local_node->get_uuid());

  ASSERT_EQ(Group_member_info::MEMBER_ONLINE,
            retrieved_local_info->get_recovery_status());

  ASSERT_EQ(executed_gtid,
            retrieved_local_info->get_gtid_executed());
  ASSERT_EQ(retrieved_gtid,
            retrieved_local_info->get_gtid_retrieved());

  delete retrieved_local_info;
}


TEST_F(ClusterMemberInfoManagerTest, EncodeDecodeLargeSets)
{
  //Add another member info in order to make this test more realistic
  string hostname("pc_hostname2");
  string uuid("781f947c-db4a-22e3-99d4-f01faf1a1c44");
  uint port= 4444;
  uint write_set_algorithm= 1;
  uint plugin_version= 0x000400;
  Gcs_member_identifier gcs_member_id("another_stuff");
  string executed_gtid("aaaa:1-11:12-14:16-20:22-30");
  //Add an huge gtid string (bigger then 16 bits )
  string retrieved_gtid(70000, 'a');
  ulonglong gtid_assignment_block_size= 9223372036854775807ULL;
  bool in_primary_mode= false;
  bool has_enforces_update_everywhere_checks= false;

  Group_member_info::Group_member_status status=
      Group_member_info::MEMBER_OFFLINE;

  Member_version local_member_plugin_version(plugin_version);
  Group_member_info* new_member= new Group_member_info((char*)hostname.c_str(),
                                                       port,
                                                       (char*)uuid.c_str(),
                                                       write_set_algorithm,
                                                       gcs_member_id.get_member_id(),
                                                       status,
                                                       local_member_plugin_version,
                                                       gtid_assignment_block_size,
                                                       Group_member_info::MEMBER_ROLE_PRIMARY,
                                                       in_primary_mode,
                                                       has_enforces_update_everywhere_checks);
  new_member->update_gtid_sets(executed_gtid,retrieved_gtid);

  cluster_member_mgr->add(new_member);

  string uuid_to_get("8d7r947c-dr4a-17i3-59d1-f01faf1kkc44");

  Group_member_info* retrieved_local_info=
      cluster_member_mgr->get_group_member_info(uuid_to_get);

  ASSERT_TRUE(retrieved_local_info != NULL);
  ASSERT_EQ(retrieved_local_info->get_uuid(),
            uuid_to_get);

  vector<uchar>* encoded= new vector<uchar>();
  cluster_member_mgr->encode(encoded);

  vector<Group_member_info*>* decoded_members=
      cluster_member_mgr->decode(&encoded->front(), encoded->size());
  delete encoded;

  cluster_member_mgr->update(decoded_members);

  delete decoded_members;

  ASSERT_EQ(2,
            cluster_member_mgr->get_number_of_members());

  delete retrieved_local_info;
  retrieved_local_info=
      cluster_member_mgr->get_group_member_info(uuid);

  ASSERT_TRUE(retrieved_local_info != NULL);

  ASSERT_EQ(port,
            retrieved_local_info->get_port());
  ASSERT_EQ(hostname,
            retrieved_local_info->get_hostname());
  ASSERT_EQ(executed_gtid,
            retrieved_local_info->get_gtid_executed());
  ASSERT_EQ(retrieved_gtid,
            retrieved_local_info->get_gtid_retrieved());
  ASSERT_EQ(write_set_algorithm,
            retrieved_local_info->get_write_set_extraction_algorithm());

  delete retrieved_local_info;
  retrieved_local_info=
      cluster_member_mgr->get_group_member_info(uuid_to_get);

  ASSERT_TRUE(retrieved_local_info != NULL);

  ASSERT_EQ(local_node->get_port(),
            retrieved_local_info->get_port());
  ASSERT_EQ(local_node->get_hostname(),
            retrieved_local_info->get_hostname());
  ASSERT_EQ(local_node->get_uuid(),
            retrieved_local_info->get_uuid());
  ASSERT_EQ(local_node->get_gcs_member_id().get_member_id(),
            retrieved_local_info->get_gcs_member_id().get_member_id());
  ASSERT_EQ(local_node->get_recovery_status(),
            retrieved_local_info->get_recovery_status());
  ASSERT_EQ(local_node->get_write_set_extraction_algorithm(),
            retrieved_local_info->get_write_set_extraction_algorithm());
  ASSERT_EQ(local_node->get_gtid_executed(),
            retrieved_local_info->get_gtid_executed());
  ASSERT_EQ(local_node->get_gtid_retrieved(),
            retrieved_local_info->get_gtid_retrieved());
  ASSERT_EQ(local_node->get_gtid_assignment_block_size(),
            retrieved_local_info->get_gtid_assignment_block_size());
  ASSERT_EQ(local_node->get_role(),
            retrieved_local_info->get_role());

  delete retrieved_local_info;
}

}
