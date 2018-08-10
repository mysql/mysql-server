/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * Test the metadata cache implementation.
 */

#include "gmock/gmock.h"
#include "gtest/gtest_prod.h"  // must be the first header

#include "cluster_metadata.h"
#include "dim.h"
#include "metadata_cache.h"
#include "metadata_factory.h"
#include "mock_metadata.h"
#include "mysql_session_replayer.h"
#include "tcp_address.h"
#include "test/helpers.h"

using metadata_cache::ManagedInstance;
using mysql_harness::TCPAddress;

class MetadataCacheTest : public ::testing::Test {
 public:
  MockNG mf;
  MetadataCache cache;

  MetadataCacheTest()
      : mf("admin", "admin", 1, 1, 1, std::chrono::seconds(10)),
        cache({TCPAddress("localhost", 32275)},
              get_instance("admin", "admin", 1, 1, 1, std::chrono::seconds(10),
                           mysqlrouter::SSLOptions()),
              std::chrono::seconds(10), mysqlrouter::SSLOptions(),
              "replicaset-1") {}
};

/**
 * Test that the list of servers that are part of a replicaset is accurate.
 */
TEST_F(MetadataCacheTest, ValidReplicasetTest_1) {
  std::vector<ManagedInstance> instance_vector_1;

  instance_vector_1 = cache.replicaset_lookup("replicaset-1");
  ASSERT_EQ(3U, instance_vector_1.size());
  EXPECT_EQ(instance_vector_1[0], mf.ms1);
  EXPECT_EQ(instance_vector_1[1], mf.ms2);
  EXPECT_EQ(instance_vector_1[2], mf.ms3);
}

/**
 * Test that looking up an invalid replicaset returns a empty list.
 */
TEST_F(MetadataCacheTest, InvalidReplicasetTest) {
  std::vector<ManagedInstance> instance_vector;

  instance_vector = cache.replicaset_lookup("InvalidReplicasetTest");

  EXPECT_TRUE(instance_vector.empty());
}

////////////////////////////////////////////////////////////////////////////////
//
// Test Metadata Cache vs metadata server availabilty
//
////////////////////////////////////////////////////////////////////////////////

class MetadataCacheTest2 : public ::testing::Test {
 public:
  // per-test setup
  virtual void SetUp() override {
    session.reset(new MySQLSessionReplayer(true));
    mysql_harness::DIM::instance().set_MySQLSession(
        [this]() { return session.get(); },  // provide pointer to session
        [](mysqlrouter::MySQLSession *) {}   // and don't try deleting it!
    );
    cmeta.reset(new ClusterMetadata("admin", "admin", 1, 1, 1,
                                    std::chrono::seconds(10),
                                    mysqlrouter::SSLOptions()));
  }

  // make queries on metadata schema return a 3 members replicaset
  void expect_sql_metadata() {
    MySQLSessionReplayer &m = *session;

    m.expect_query_one(
        "SELECT * FROM mysql_innodb_cluster_metadata.schema_version");
    m.then_return(3, {
                         {m.string_or_null("1"), m.string_or_null("0"),
                          m.string_or_null("1")},
                     });

    m.expect_query(
        "SELECT R.replicaset_name, I.mysql_server_uuid, I.role, I.weight, "
        "I.version_token, H.location, I.addresses->>'$.mysqlClassic', "
        "I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters "
        "AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON "
        "F.cluster_id = R.cluster_id JOIN "
        "mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id = "
        "I.replicaset_id JOIN mysql_innodb_cluster_metadata.hosts AS H ON "
        "I.host_id = H.host_id WHERE F.cluster_name = 'cluster-1';");
    m.then_return(
        8,
        {// replicaset_name, mysql_server_uuid, role, weight, version_token,
         // location, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX'
         {m.string_or_null("cluster-1"), m.string_or_null("uuid-server1"),
          m.string_or_null("HA"), m.string_or_null(), m.string_or_null(),
          m.string_or_null(""), m.string_or_null("localhost:3000"),
          m.string_or_null("localhost:30000")},
         {m.string_or_null("cluster-1"), m.string_or_null("uuid-server2"),
          m.string_or_null("HA"), m.string_or_null(), m.string_or_null(),
          m.string_or_null(""), m.string_or_null("localhost:3001"),
          m.string_or_null("localhost:30010")},
         {m.string_or_null("cluster-1"), m.string_or_null("uuid-server3"),
          m.string_or_null("HA"), m.string_or_null(), m.string_or_null(),
          m.string_or_null(""), m.string_or_null("localhost:3002"),
          m.string_or_null("localhost:30020")}});
  }

  // make queries on PFS.replication_group_members return all members ONLINE
  void expect_sql_members() {
    MySQLSessionReplayer &m = *session;

    m.expect_query("show status like 'group_replication_primary_member'");
    m.then_return(2, {// Variable_name, Value
                      {m.string_or_null("group_replication_primary_member"),
                       m.string_or_null("uuid-server1")}});

    m.expect_query(
        "SELECT member_id, member_host, member_port, member_state, "
        "@@group_replication_single_primary_mode FROM "
        "performance_schema.replication_group_members WHERE channel_name = "
        "'group_replication_applier'");
    m.then_return(5, {// member_id, member_host, member_port, member_state,
                      // @@group_replication_single_primary_mode
                      {m.string_or_null("uuid-server1"),
                       m.string_or_null("somehost"), m.string_or_null("3000"),
                       m.string_or_null("ONLINE"), m.string_or_null("1")},
                      {m.string_or_null("uuid-server2"),
                       m.string_or_null("somehost"), m.string_or_null("3001"),
                       m.string_or_null("ONLINE"), m.string_or_null("1")},
                      {m.string_or_null("uuid-server3"),
                       m.string_or_null("somehost"), m.string_or_null("3002"),
                       m.string_or_null("ONLINE"), m.string_or_null("1")}});
  }

  std::shared_ptr<MySQLSessionReplayer> session;
  std::shared_ptr<ClusterMetadata> cmeta;
  std::shared_ptr<MetadataCache> cache;

  std::vector<TCPAddress> metadata_servers{
      {"localhost", 3000},
      {"localhost", 3001},
      {"localhost", 3002},
  };
};

void expect_cluster_routable(MetadataCache &mc) {
  std::vector<ManagedInstance> instances = mc.replicaset_lookup("cluster-1");
  ASSERT_EQ(3U, instances.size());
  EXPECT_EQ("uuid-server1", instances[0].mysql_server_uuid);
  EXPECT_EQ(metadata_cache::ServerMode::ReadWrite, instances[0].mode);
  EXPECT_EQ("uuid-server2", instances[1].mysql_server_uuid);
  EXPECT_EQ(metadata_cache::ServerMode::ReadOnly, instances[1].mode);
  EXPECT_EQ("uuid-server3", instances[2].mysql_server_uuid);
  EXPECT_EQ(metadata_cache::ServerMode::ReadOnly, instances[2].mode);
}

void expect_cluster_not_routable(MetadataCache &mc) {
  std::vector<ManagedInstance> instances = mc.replicaset_lookup("cluster-1");
  ASSERT_EQ(0U, instances.size());
}

TEST_F(MetadataCacheTest2, basic_test) {
  // start off with all metadata servers up
  expect_sql_metadata();
  expect_sql_members();

  MetadataCache mc(metadata_servers, cmeta, std::chrono::seconds(10),
                   mysqlrouter::SSLOptions(), "cluster-1");

  // verify that cluster can be seen
  expect_cluster_routable(mc);
  expect_cluster_routable(mc);  // repeated queries should not change anything
  expect_cluster_routable(mc);  // repeated queries should not change anything

  // refresh MC
  expect_sql_metadata();
  expect_sql_members();
  mc.refresh();

  // verify that cluster can be seen
  expect_cluster_routable(mc);
  expect_cluster_routable(mc);  // repeated queries should not change anything
  expect_cluster_routable(mc);  // repeated queries should not change anything
}

TEST_F(MetadataCacheTest2, metadata_server_connection_failures) {
  // Here we test MC behaviour when metadata servers go down and back up again.
  // ATM (2017.01.10, might be changed later) at least one metadata server must
  // be reachable for Router to continue Routing.

  MySQLSessionReplayer &m = *session;

  // start off with all metadata servers up
  expect_sql_metadata();
  expect_sql_members();
  MetadataCache mc(metadata_servers, cmeta, std::chrono::seconds(10),
                   mysqlrouter::SSLOptions(), "cluster-1");
  expect_cluster_routable(mc);

  // refresh: fail connecting to first metadata server
  m.expect_connect("127.0.0.1", 3000, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  expect_sql_metadata();
  expect_sql_members();
  mc.refresh();
  expect_cluster_routable(mc);

  // refresh: fail connecting to all 3 metadata servers
  m.expect_connect("127.0.0.1", 3000, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  m.expect_connect("127.0.0.1", 3001, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  m.expect_connect("127.0.0.1", 3002, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  mc.refresh();
  expect_cluster_not_routable(mc);  // lookup should return nothing (all route
                                    // paths should have been cleared)

  // refresh: fail connecting to first 2 metadata servers
  m.expect_connect("127.0.0.1", 3000, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  m.expect_connect("127.0.0.1", 3001, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  expect_sql_metadata();
  expect_sql_members();
  mc.refresh();
  expect_cluster_routable(mc);  // lookup should see the cluster again
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
