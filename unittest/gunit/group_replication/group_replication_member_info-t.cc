/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "gcs_corosync_view_identifier.h"
#include "gcs_state_exchange.h"

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
    gcs_member_id= new Gcs_member_identifier("stuff");

    Cluster_member_info::Cluster_member_status status=
                                            Cluster_member_info::MEMBER_OFFLINE;

    local_node= new Cluster_member_info((char*)hostname.c_str(), port,
                                        (char*)uuid.c_str(),gcs_member_id,
                                        status);
  }

  virtual void TearDown()
  {
    delete gcs_member_id;
    delete local_node;
  }

  Cluster_member_info* local_node;
  Gcs_member_identifier* gcs_member_id;
};

TEST_F(ClusterMemberInfoTest, EncodeDecodeIdempotencyTest)
{
  vector<uchar>* encoded= new vector<uchar>();
  local_node->encode(encoded);

  Cluster_member_info decoded_local_node(&encoded->front(), encoded->size());

  ASSERT_EQ(local_node->get_port(),
            decoded_local_node.get_port());
  ASSERT_EQ(*local_node->get_hostname(),
            *decoded_local_node.get_hostname());
  ASSERT_EQ(*local_node->get_uuid(),
            *decoded_local_node.get_uuid());
  ASSERT_EQ(*local_node->get_gcs_member_id()->get_member_id(),
            *decoded_local_node.get_gcs_member_id()->get_member_id());
  ASSERT_EQ(local_node->get_recovery_status(),
            decoded_local_node.get_recovery_status());
}

TEST_F(ClusterMemberInfoTest, EncodeDecodeWithStatusTest)
{
  vector<uchar>* encoded= new vector<uchar>();
  local_node->encode(encoded);

  unsigned long int fixed_part= 9999;
  int monotonic_part= 140;
  Gcs_corosync_view_identifier v_id(fixed_part, monotonic_part);

  Member_state ms(&v_id, encoded);

  vector<uchar>* ms_encoded= new vector<uchar>();
  ms.encode(ms_encoded);

  Member_state ms_decoded(&ms_encoded->front(), ms_encoded->size());

  ASSERT_EQ(fixed_part,
            ms_decoded.get_view_id()->get_fixed_part());
  ASSERT_EQ(monotonic_part,
            ms_decoded.get_view_id()->get_monotonic_part());

  vector<uchar>* ms_exchangeable_data= ms_decoded.get_data();

  Cluster_member_info decoded_local_node(&ms_exchangeable_data->front(),
                                         ms_exchangeable_data->size());

  ASSERT_EQ(local_node->get_port(),
            decoded_local_node.get_port());
  ASSERT_EQ(*local_node->get_hostname(),
            *decoded_local_node.get_hostname());
  ASSERT_EQ(*local_node->get_uuid(),
            *decoded_local_node.get_uuid());
  ASSERT_EQ(*local_node->get_gcs_member_id()->get_member_id(),
            *decoded_local_node.get_gcs_member_id()->get_member_id());
  ASSERT_EQ(local_node->get_recovery_status(),
            decoded_local_node.get_recovery_status());
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
    gcs_member_id= new Gcs_member_identifier("stuff");

    Cluster_member_info::Cluster_member_status status=
                                            Cluster_member_info::MEMBER_OFFLINE;

    local_node= new Cluster_member_info((char*)hostname.c_str(), port,
                                        (char*)uuid.c_str(),gcs_member_id,
                                        status);

    cluster_member_mgr= new Cluster_member_info_manager(local_node);
  }

  virtual void TearDown()
  {
    delete cluster_member_mgr;
    delete gcs_member_id;
    delete local_node;
  }

  Cluster_member_info_manager_interface* cluster_member_mgr;
  Cluster_member_info* local_node;
  Gcs_member_identifier* gcs_member_id;
};

TEST_F(ClusterMemberInfoManagerTest, GetLocalInfoByUUIDTest)
{
  //Add another member info in order to make this test more realistic
  string hostname("pc_hostname2");
  string uuid("781f947c-db4a-22e3-99d4-f01faf1a1c44");
  uint port= 4444;
  Gcs_member_identifier gcs_member_id("another_stuff");

  Cluster_member_info::Cluster_member_status status=
                                           Cluster_member_info::MEMBER_OFFLINE;

  Cluster_member_info* new_member= new Cluster_member_info
                                                      ((char*)hostname.c_str(),
                                                       port,
                                                       (char*)uuid.c_str(),
                                                       &gcs_member_id,
                                                       status);

  cluster_member_mgr->add(new_member);

  string uuid_to_get("8d7r947c-dr4a-17i3-59d1-f01faf1kkc44");

  Cluster_member_info* retrieved_local_info=
            cluster_member_mgr->get_cluster_member_info(uuid_to_get);

  ASSERT_TRUE(retrieved_local_info != NULL);
  ASSERT_EQ(*retrieved_local_info->get_uuid(),
            uuid_to_get);

  delete retrieved_local_info;
}

TEST_F(ClusterMemberInfoManagerTest, UpdateStatusOfLocalObjectTest)
{
  cluster_member_mgr->update_member_status
            (*local_node->get_uuid(),
             Cluster_member_info::MEMBER_ONLINE);

  ASSERT_EQ(Cluster_member_info::MEMBER_ONLINE,
            local_node->get_recovery_status());
}

TEST_F(ClusterMemberInfoManagerTest, GetLocalInfoByUUIDAfterEncodingTest)
{
  vector<uchar>* encoded= new vector<uchar>();
  cluster_member_mgr->encode(encoded);

  vector<Cluster_member_info*>* decoded_members=
                                 cluster_member_mgr->decode(&encoded->front());

  cluster_member_mgr->update(decoded_members);

  string uuid_to_get("8d7r947c-dr4a-17i3-59d1-f01faf1kkc44");

  Cluster_member_info* retrieved_local_info=
            cluster_member_mgr->get_cluster_member_info(uuid_to_get);

  ASSERT_TRUE(retrieved_local_info != NULL);

  ASSERT_EQ(local_node->get_port(),
            retrieved_local_info->get_port());
  ASSERT_EQ(*local_node->get_hostname(),
            *retrieved_local_info->get_hostname());
  ASSERT_EQ(*local_node->get_uuid(),
            *retrieved_local_info->get_uuid());
  ASSERT_EQ(*local_node->get_gcs_member_id()->get_member_id(),
            *retrieved_local_info->get_gcs_member_id()->get_member_id());
  ASSERT_EQ(local_node->get_recovery_status(),
            retrieved_local_info->get_recovery_status());

  delete retrieved_local_info;
}

TEST_F(ClusterMemberInfoManagerTest, UpdateStatusOfLocalObjectAfterExchangeTest)
{
  vector<uchar>* encoded= new vector<uchar>();
  cluster_member_mgr->encode(encoded);

  vector<Cluster_member_info*>* decoded_members=
                                 cluster_member_mgr->decode(&encoded->front());

  cluster_member_mgr->update(decoded_members);

  cluster_member_mgr->update_member_status
                   (*local_node->get_uuid(),
                   Cluster_member_info::MEMBER_ONLINE);

  ASSERT_EQ(Cluster_member_info::MEMBER_ONLINE,
            local_node->get_recovery_status());

  Cluster_member_info* retrieved_local_info=
        cluster_member_mgr->get_cluster_member_info(*local_node->get_uuid());

  ASSERT_EQ(Cluster_member_info::MEMBER_ONLINE,
            retrieved_local_info->get_recovery_status());

  delete retrieved_local_info;
}

}
