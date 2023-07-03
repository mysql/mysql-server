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

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest_prod.h>  // FRIEND_TEST, must be before all other local includes
#include <tuple>

#include "cluster_metadata_gr.h"
#include "dim.h"
#include "metadata_cache_gr.h"
#include "mysql_session_replayer.h"
#include "mysqlrouter/metadata_cache.h"
#include "tcp_address.h"
#include "test/helpers.h"

using namespace std::chrono_literals;
using namespace metadata_cache;

static constexpr unsigned kRouterId{1};

static constexpr const char group_uuid[] =
    "3e4338a1-2c5d-49ac-8baa-e5a25ba61e76";

static constexpr const char node_1_uuid[] =
    "3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39";
static constexpr const char node_2_uuid[] =
    "8148cba4-2ad5-456e-a04e-2ba73eb10cc5";
static constexpr const char node_3_uuid[] =
    "f0a2079f-8b90-4324-9eec-a0496c4338e0";

static constexpr const char replicaset_name[] = "default";
static constexpr const char cluster_id[] = "cluster-1-id";
static constexpr const char cluster_name[] = "cluster-1";

class FailoverTest : public ::testing::Test {
 public:
  std::shared_ptr<MySQLSessionReplayer> session;
  std::shared_ptr<ClusterMetadata> cmeta;
  std::shared_ptr<GRMetadataCache> cache;

  // per-test setup
  void SetUp() override {
    session = std::make_shared<MySQLSessionReplayer>(true);

    // setup DI for MySQLSession
    mysql_harness::DIM::instance().set_MySQLSession(
        [this]() { return session.get(); },  // provide pointer to session
        [](mysqlrouter::MySQLSession *) {}   // and don't try deleting it!
    );

    cmeta = std::make_shared<GRClusterMetadata>(
        metadata_cache::MetadataCacheMySQLSessionConfig{
            {"admin", "admin"}, 1, 1, 1},
        mysqlrouter::SSLOptions());
  }

  void init_cache() {
    cache = std::make_shared<GRMetadataCache>(
        kRouterId, group_uuid, "",
        std::vector<mysql_harness::TCPAddress>{
            {"localhost", 32275},
        },
        cmeta, metadata_cache::MetadataCacheTTLConfig{10s, -1s, 20s},
        mysqlrouter::SSLOptions(),
        mysqlrouter::TargetCluster{
            mysqlrouter::TargetCluster::TargetType::ByName, "cluster-1"},
        metadata_cache::RouterAttributes{},
        mysql_harness::kDefaultStackSizeInKiloBytes, false);
  }

  // make queries on metadata schema return a 3 members replicaset
  void expect_metadata_1() {
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
        "AND R.attributes->>'$.group_replication_group_name' = "
        "'3e4338a1-2c5d-49ac-8baa-e5a25ba61e76'");
    m.then_return(
        5,
        {// cluster_id, cluster_name, replicaset_name, mysql_server_uuid,
         // I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX'
         {m.string_or_null(cluster_id), m.string_or_null(cluster_name),
          m.string_or_null(replicaset_name), m.string_or_null(node_1_uuid),
          m.string_or_null("localhost:3000"),
          m.string_or_null("localhost:30000")},
         {m.string_or_null(cluster_id), m.string_or_null(cluster_name),
          m.string_or_null(replicaset_name), m.string_or_null(node_2_uuid),
          m.string_or_null("localhost:3001"),
          m.string_or_null("localhost:30010")},
         {m.string_or_null(cluster_id), m.string_or_null(cluster_name),
          m.string_or_null(replicaset_name), m.string_or_null(node_3_uuid),
          m.string_or_null("localhost:3002"),
          m.string_or_null("localhost:30020")}});

    m.expect_execute("COMMIT");
    m.then_ok();
  }

  // make queries on PFS.replication_group_members return all members ONLINE
  void expect_group_members_1() {
    MySQLSessionReplayer &m = *session;

    m.expect_query("show status like 'group_replication_primary_member'");
    m.then_return(2,
                  {// Variable_name, Value
                   {m.string_or_null("group_replication_primary_member"),
                    m.string_or_null("3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39")}});

    m.expect_query(
        "SELECT member_id, member_host, member_port, member_state, "
        "@@group_replication_single_primary_mode FROM "
        "performance_schema.replication_group_members WHERE channel_name = "
        "'group_replication_applier'");
    m.then_return(5, {// member_id, member_host, member_port, member_state,
                      // @@group_replication_single_primary_mode
                      {m.string_or_null(node_1_uuid),
                       m.string_or_null("somehost"), m.string_or_null("3000"),
                       m.string_or_null("ONLINE"), m.string_or_null("1")},
                      {m.string_or_null(node_2_uuid),
                       m.string_or_null("somehost"), m.string_or_null("3001"),
                       m.string_or_null("ONLINE"), m.string_or_null("1")},
                      {m.string_or_null(node_3_uuid),
                       m.string_or_null("somehost"), m.string_or_null("3002"),
                       m.string_or_null("ONLINE"), m.string_or_null("1")}});
  }

  // make queries on PFS.replication_group_members return primary in the given
  // state
  void expect_group_members_1_primary_fail(
      const char *state, const char *primary_override = node_1_uuid) {
    MySQLSessionReplayer &m = *session;

    m.expect_query("show status like 'group_replication_primary_member'");
    m.then_return(2, {// Variable_name, Value
                      {m.string_or_null("group_replication_primary_member"),
                       m.string_or_null(primary_override)}});

    m.expect_query(
        "SELECT member_id, member_host, member_port, member_state, "
        "@@group_replication_single_primary_mode FROM "
        "performance_schema.replication_group_members WHERE channel_name = "
        "'group_replication_applier'");
    if (!state) {
      // primary not listed at all
      m.then_return(5,
                    {// member_id, member_host, member_port, member_state,
                     // @@group_replication_single_primary_mode
                     {m.string_or_null(node_2_uuid),
                      m.string_or_null("somehost"), m.string_or_null("3001"),
                      m.string_or_null("ONLINE"), m.string_or_null("1")},
                     {m.string_or_null(node_3_uuid),
                      m.string_or_null("somehost"), m.string_or_null("3002"),
                      m.string_or_null("ONLINE"), m.string_or_null("1")}});
    } else {
      m.then_return(5,
                    {// member_id, member_host, member_port, member_state,
                     // @@group_replication_single_primary_mode
                     {m.string_or_null(node_1_uuid),
                      m.string_or_null("somehost"), m.string_or_null("3000"),
                      m.string_or_null(state), m.string_or_null("1")},
                     {m.string_or_null(node_2_uuid),
                      m.string_or_null("somehost"), m.string_or_null("3001"),
                      m.string_or_null("ONLINE"), m.string_or_null("1")},
                     {m.string_or_null(node_3_uuid),
                      m.string_or_null("somehost"), m.string_or_null("3002"),
                      m.string_or_null("ONLINE"), m.string_or_null("1")}});
    }
  }
};

class DelayCheck {
 public:
  DelayCheck() { start_time_ = time(nullptr); }

  long time_elapsed() { return time(nullptr) - start_time_; }

 private:
  time_t start_time_;
};

namespace std {

std::ostream &operator<<(std::ostream &os, const ServerMode &v) {
  switch (v) {
    case ServerMode::ReadOnly:
      os << "RO";
      break;
    case ServerMode::ReadWrite:
      os << "RW";
      break;
    case ServerMode::Unavailable:
      os << "N/A";
      break;
  };
  return os;
}

std::ostream &operator<<(std::ostream &os, const ManagedInstance &v) {
  os << "{";
  os << "disconnect_when_hidden: " << v.disconnect_existing_sessions_when_hidden
     << ", ";
  os << "hidden: " << v.hidden << ", ";
  os << "host: " << v.host << ", ";
  os << "port: " << v.port << ", ";
  os << "xport: " << v.xport << ", ";
  os << "mode: " << v.mode << ", ";
  os << "mysql_server_uuid: " << v.mysql_server_uuid;
  os << "}";

  return os;
}

}  // namespace std

MATCHER(PartialInstanceMatcher, "" /* defaults to 'uuid matcher' */) {
  using namespace ::testing;

  const auto &lhs = std::get<0>(arg);  // ManagedInstance
  const auto &rhs = std::get<1>(arg);  // std::tuple<const char *, ServerMode>

  return ExplainMatchResult(
      AllOf(Field("mysql_server_uuid", &ManagedInstance::mysql_server_uuid,
                  Eq(std::get<0>(rhs))),
            Field("mode", &ManagedInstance::mode, Eq(std::get<1>(rhs)))),
      lhs, result_listener);
}

TEST_F(FailoverTest, primary_failover_router_member_network_loss) {
  // normal operation
  // ----------------

  ASSERT_NO_THROW(init_cache());
  expect_metadata_1();
  expect_group_members_1();
  cache->refresh(true);

  // ensure no expected queries leftover
  ASSERT_FALSE(session->print_expected());

  // ensure that the instance list returned by a lookup is the expected one
  // in the case everything's online and well

  ASSERT_THAT(cache->get_cluster_nodes(),
              ::testing::Pointwise(
                  PartialInstanceMatcher(),
                  std::initializer_list<std::tuple<const char *, ServerMode>>{
                      std::make_tuple(node_1_uuid, ServerMode::ReadWrite),
                      std::make_tuple(node_2_uuid, ServerMode::ReadOnly),
                      std::make_tuple(node_3_uuid, ServerMode::ReadOnly)}));

  // now the primary goes down (but group view not updated yet by GR)
  // ----------------------------------------------------------------
  expect_metadata_1();
  expect_group_members_1();
  ASSERT_NO_THROW(cache->refresh(true));

  // this should fail with timeout b/c no primary yet
  {
    DelayCheck t;
    EXPECT_FALSE(cache->wait_primary_failover(node_1_uuid, 1s));
    EXPECT_GE(t.time_elapsed(), 1);
  }
}

TEST_F(FailoverTest, primary_failover_reelection) {
  ASSERT_NO_THROW(init_cache());
  expect_metadata_1();
  expect_group_members_1();
  cache->refresh(true);
  // ensure no expected queries leftover
  ASSERT_FALSE(session->print_expected());

  // primary is still visible, even tho it's dead.. that's because we pretend
  // we're getting updates from an instance that hasn't noticed that yet
  ASSERT_THAT(cache->get_cluster_nodes(),
              ::testing::Pointwise(
                  PartialInstanceMatcher(),
                  std::initializer_list<std::tuple<const char *, ServerMode>>{
                      std::make_tuple(node_1_uuid, ServerMode::ReadWrite),
                      std::make_tuple(node_2_uuid, ServerMode::ReadOnly),
                      std::make_tuple(node_3_uuid, ServerMode::ReadOnly)}));

  // GR notices the server went down, new primary picked
  // ---------------------------------------------------
  expect_metadata_1();
  expect_group_members_1_primary_fail(nullptr, node_2_uuid);
  ASSERT_NO_THROW(cache->refresh(true));

  ASSERT_THAT(cache->get_cluster_nodes(),
              ::testing::Pointwise(
                  PartialInstanceMatcher(),
                  std::initializer_list<std::tuple<const char *, ServerMode>>{
                      std::make_tuple(node_1_uuid, ServerMode::Unavailable),
                      std::make_tuple(node_2_uuid, ServerMode::ReadWrite),
                      std::make_tuple(node_3_uuid, ServerMode::ReadOnly)}));

  // this should succeed
  {
    DelayCheck t;
    EXPECT_TRUE(cache->wait_primary_failover(node_1_uuid, 2s));
    EXPECT_LE(t.time_elapsed(), 1);
  }
}

TEST_F(FailoverTest, primary_failover_shutdown) {
  ASSERT_NO_THROW(init_cache());
  expect_metadata_1();
  expect_group_members_1();
  ASSERT_NO_THROW(cache->refresh(true));

  auto wait_failover_thread = std::thread([&] {
    DelayCheck t;
    // even though we wait for 10s for the primary failover the function should
    // return promptly when the catche->stop() gets called (mimicking terminate
    // request)
    EXPECT_FALSE(cache->wait_primary_failover(node_1_uuid, 10s));
    EXPECT_LE(t.time_elapsed(), 1);
  });

  cache->stop();

  wait_failover_thread.join();
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
