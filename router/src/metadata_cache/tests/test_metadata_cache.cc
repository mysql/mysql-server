/*
  Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include <chrono>
#include <memory>
#include <vector>

#include <gmock/gmock.h>

#include "cluster_metadata.h"
#include "dim.h"
#include "metadata_cache_gr.h"
#include "mock_metadata.h"
#include "mock_metadata_factory.h"
#include "mysql_session_replayer.h"
#include "tcp_address.h"
#include "test/helpers.h"

using metadata_cache::ManagedInstance;
using mysql_harness::TCPAddress;
using namespace std::chrono_literals;

constexpr unsigned kRouterId = 2;

class MetadataCacheTest : public ::testing::Test {
 public:
  MockNG mf;
  GRMetadataCache cache;

  MetadataCacheTest()
      : mf(metadata_cache::MetadataCacheMySQLSessionConfig{
            {"admin", "admin"}, 1, 1, 1}),
        cache(kRouterId, "0000-0001", "", {TCPAddress("localhost", 32275)},
              mock_metadata_factory_get_instance(
                  mysqlrouter::ClusterType::GR_V1,
                  metadata_cache::MetadataCacheMySQLSessionConfig{
                      {"admin", "admin"}, 1, 1, 1},
                  mysqlrouter::SSLOptions(), false, 0),
              metadata_cache::MetadataCacheTTLConfig{10s, -1s, 20s},
              mysqlrouter::SSLOptions(),
              {mysqlrouter::TargetCluster::TargetType::ByName, "cluster-1"},
              metadata_cache::RouterAttributes{}) {
    cache.refresh(true);
  }
};

/**
 * Test that the list of servers that are part of a replicaset is accurate.
 */
TEST_F(MetadataCacheTest, ValidReplicasetTest_1) {
  std::vector<ManagedInstance> instance_vector_1;

  instance_vector_1 = cache.get_cluster_nodes();
  ASSERT_EQ(3U, instance_vector_1.size());
  EXPECT_EQ(instance_vector_1[0], mf.ms1);
  EXPECT_EQ(instance_vector_1[1], mf.ms2);
  EXPECT_EQ(instance_vector_1[2], mf.ms3);
}

////////////////////////////////////////////////////////////////////////////////
//
// Test Metadata Cache vs metadata server availability
//
////////////////////////////////////////////////////////////////////////////////

class MetadataCacheTest2 : public ::testing::Test {
 public:
  // per-test setup
  void SetUp() override {
    session.reset(new MySQLSessionReplayer(true));
    mysql_harness::DIM::instance().set_MySQLSession(
        [this]() { return session.get(); },  // provide pointer to session
        [](mysqlrouter::MySQLSession *) {}   // and don't try deleting it!
    );
    cmeta.reset(new GRClusterMetadata(
        metadata_cache::MetadataCacheMySQLSessionConfig{
            {"admin", "admin"}, 1, 1, 1},
        mysqlrouter::SSLOptions()));
  }

  // make queries on metadata schema return a 3 members replicaset
  void expect_sql_metadata() {
    MySQLSessionReplayer &m = *session;

    m.expect_execute(
        "SET @@SESSION.autocommit=1, @@SESSION.character_set_client=utf8, "
        "@@SESSION.character_set_results=utf8, "
        "@@SESSION.character_set_connection=utf8, "
        "@@SESSION.sql_mode='ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_"
        "DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION', "
        "@@SESSION.optimizer_switch='derived_merge=on'");
    m.then_ok();
    m.expect_execute("SET @@SESSION.group_replication_consistency='EVENTUAL'");
    m.then_ok();
    m.expect_execute("START TRANSACTION");
    m.then_ok();

    m.expect_query_one(
        "SELECT * FROM mysql_innodb_cluster_metadata.schema_version");
    m.then_return(3, {
                         {m.string_or_null("1"), m.string_or_null("0"),
                          m.string_or_null("1")},
                     });

    m.expect_query(
        "SELECT F.cluster_id, F.cluster_name, R.replicaset_name, "
        "I.mysql_server_uuid, "
        "I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM "
        "mysql_innodb_cluster_metadata.clusters "
        "AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON "
        "F.cluster_id = R.cluster_id "
        "JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id "
        "= I.replicaset_id WHERE F.cluster_name = 'cluster-1' "
        "AND R.attributes->>'$.group_replication_group_name' = '0000-0001'");
    m.then_return(
        5,
        {// cluster_id, cluster_name, replicaset_name, mysql_server_uuid,
         // I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX'
         {m.string_or_null("cluster-id-1"), m.string_or_null("cluster-1"),
          m.string_or_null("default"), m.string_or_null("uuid-server1"),
          m.string_or_null("localhost:3000"),
          m.string_or_null("localhost:30000")},
         {m.string_or_null("cluster-id-1"), m.string_or_null("cluster-1"),
          m.string_or_null("default"), m.string_or_null("uuid-server2"),
          m.string_or_null("localhost:3001"),
          m.string_or_null("localhost:30010")},
         {m.string_or_null("cluster-id-1"), m.string_or_null("cluster-1"),
          m.string_or_null("default"), m.string_or_null("uuid-server3"),
          m.string_or_null("localhost:3002"),
          m.string_or_null("localhost:30020")}});

    m.expect_execute("COMMIT");
    m.then_ok();
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

  const std::string gr_id = "0000-0001";
};

void expect_cluster_routable(MetadataCache &mc) {
  std::vector<ManagedInstance> instances = mc.get_cluster_nodes();
  ASSERT_EQ(3U, instances.size());
  EXPECT_EQ("uuid-server1", instances[0].mysql_server_uuid);
  EXPECT_EQ(metadata_cache::ServerMode::ReadWrite, instances[0].mode);
  EXPECT_EQ("uuid-server2", instances[1].mysql_server_uuid);
  EXPECT_EQ(metadata_cache::ServerMode::ReadOnly, instances[1].mode);
  EXPECT_EQ("uuid-server3", instances[2].mysql_server_uuid);
  EXPECT_EQ(metadata_cache::ServerMode::ReadOnly, instances[2].mode);
}

void expect_cluster_not_routable(GRMetadataCache &mc) {
  std::vector<ManagedInstance> instances = mc.get_cluster_nodes();
  ASSERT_EQ(0U, instances.size());
}

TEST_F(MetadataCacheTest2, basic_test) {
  // start off with all metadata servers up
  expect_sql_metadata();
  expect_sql_members();

  GRMetadataCache mc(
      kRouterId, gr_id, "", metadata_servers, cmeta,
      metadata_cache::MetadataCacheTTLConfig{10s, -1s, 20s},
      mysqlrouter::SSLOptions(),
      {mysqlrouter::TargetCluster::TargetType::ByName, "cluster-1"},
      metadata_cache::RouterAttributes{});
  mc.refresh(true);

  // verify that cluster can be seen
  expect_cluster_routable(mc);
  expect_cluster_routable(mc);  // repeated queries should not change anything
  expect_cluster_routable(mc);  // repeated queries should not change anything

  // refresh MC
  expect_sql_metadata();
  expect_sql_members();
  mc.refresh(true);

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
  GRMetadataCache mc(
      kRouterId, gr_id, "", metadata_servers, cmeta,
      metadata_cache::MetadataCacheTTLConfig{10s, -1s, 20s},
      mysqlrouter::SSLOptions(),
      {mysqlrouter::TargetCluster::TargetType::ByName, "cluster-1"},
      metadata_cache::RouterAttributes{});
  mc.refresh(true);
  expect_cluster_routable(mc);

  // refresh: fail connecting to first metadata server
  m.expect_connect("127.0.0.1", 3000, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  expect_sql_metadata();
  expect_sql_members();
  mc.refresh(true);
  expect_cluster_routable(mc);

  // refresh: fail connecting to all 3 metadata servers
  m.expect_connect("127.0.0.1", 3000, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  m.expect_connect("127.0.0.1", 3001, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  m.expect_connect("127.0.0.1", 3002, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  mc.refresh(true);
  expect_cluster_not_routable(mc);  // lookup should return nothing (all route
                                    // paths should have been cleared)

  // refresh: fail connecting to first 2 metadata servers
  m.expect_connect("127.0.0.1", 3000, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  m.expect_connect("127.0.0.1", 3001, "admin", "admin", "")
      .then_error("some fake bad connection message", 66);
  expect_sql_metadata();
  expect_sql_members();
  mc.refresh(true);
  expect_cluster_routable(mc);  // lookup should see the cluster again
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
