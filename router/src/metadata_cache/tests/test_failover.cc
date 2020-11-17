/*
  Copyright (c) 2016, 2020, Oracle and/or its affiliates.

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

#include "cluster_metadata_gr.h"
#include "dim.h"
#include "gtest/gtest_prod.h"
#include "metadata_cache_gr.h"
#include "mysql_session_replayer.h"
#include "tcp_address.h"
#include "test/helpers.h"

#include "gmock/gmock.h"

using namespace std::chrono_literals;
using namespace metadata_cache;

constexpr unsigned kRouterId = 1;

class FailoverTest : public ::testing::Test {
 public:
  std::shared_ptr<MySQLSessionReplayer> session;
  std::shared_ptr<ClusterMetadata> cmeta;
  std::shared_ptr<GRMetadataCache> cache;

  FailoverTest() {}

  // per-test setup
  void SetUp() override {
    session.reset(new MySQLSessionReplayer(true));

    // setup DI for MySQLSession
    mysql_harness::DIM::instance().set_MySQLSession(
        [this]() { return session.get(); },  // provide pointer to session
        [](mysqlrouter::MySQLSession *) {}   // and don't try deleting it!
    );

    cmeta.reset(new GRClusterMetadata("admin", "admin", 1, 1, 1,
                                      mysqlrouter::SSLOptions()));
  }

  void init_cache() {
    cache.reset(
        new GRMetadataCache(kRouterId, "3e4338a1-2c5d-49ac-8baa-e5a25ba61e76",
                            {mysql_harness::TCPAddress("localhost", 32275)},
                            cmeta, std::chrono::seconds(10),
                            std::chrono::seconds(-1), std::chrono::seconds(20),

                            mysqlrouter::SSLOptions(), "cluster-1"));
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
        "SELECT R.replicaset_name, I.mysql_server_uuid, "
        "I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM "
        "mysql_innodb_cluster_metadata.clusters "
        "AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON "
        "F.cluster_id = R.cluster_id "
        "JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id "
        "= I.replicaset_id WHERE F.cluster_name = 'cluster-1' "
        "AND R.attributes->>'$.group_replication_group_name' = "
        "'3e4338a1-2c5d-49ac-8baa-e5a25ba61e76'");
    m.then_return(
        7,
        {// replicaset_name, mysql_server_uuid,
         // location, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX'
         {m.string_or_null("default"),
          m.string_or_null("3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39"),
          m.string_or_null("localhost:3000"),
          m.string_or_null("localhost:30000")},
         {m.string_or_null("default"),
          m.string_or_null("8148cba4-2ad5-456e-a04e-2ba73eb10cc5"),
          m.string_or_null("localhost:3001"),
          m.string_or_null("localhost:30010")},
         {m.string_or_null("default"),
          m.string_or_null("f0a2079f-8b90-4324-9eec-a0496c4338e0"),
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
                      {m.string_or_null("3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39"),
                       m.string_or_null("somehost"), m.string_or_null("3000"),
                       m.string_or_null("ONLINE"), m.string_or_null("1")},
                      {m.string_or_null("8148cba4-2ad5-456e-a04e-2ba73eb10cc5"),
                       m.string_or_null("somehost"), m.string_or_null("3001"),
                       m.string_or_null("ONLINE"), m.string_or_null("1")},
                      {m.string_or_null("f0a2079f-8b90-4324-9eec-a0496c4338e0"),
                       m.string_or_null("somehost"), m.string_or_null("3002"),
                       m.string_or_null("ONLINE"), m.string_or_null("1")}});
  }

  // make queries on PFS.replication_group_members return primary in the given
  // state
  void expect_group_members_1_primary_fail(
      const char *state,
      const char *primary_override = "3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39") {
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
                     {m.string_or_null("8148cba4-2ad5-456e-a04e-2ba73eb10cc5"),
                      m.string_or_null("somehost"), m.string_or_null("3001"),
                      m.string_or_null("ONLINE"), m.string_or_null("1")},
                     {m.string_or_null("f0a2079f-8b90-4324-9eec-a0496c4338e0"),
                      m.string_or_null("somehost"), m.string_or_null("3002"),
                      m.string_or_null("ONLINE"), m.string_or_null("1")}});
    } else {
      m.then_return(5,
                    {// member_id, member_host, member_port, member_state,
                     // @@group_replication_single_primary_mode
                     {m.string_or_null("3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39"),
                      m.string_or_null("somehost"), m.string_or_null("3000"),
                      m.string_or_null(state), m.string_or_null("1")},
                     {m.string_or_null("8148cba4-2ad5-456e-a04e-2ba73eb10cc5"),
                      m.string_or_null("somehost"), m.string_or_null("3001"),
                      m.string_or_null("ONLINE"), m.string_or_null("1")},
                     {m.string_or_null("f0a2079f-8b90-4324-9eec-a0496c4338e0"),
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

TEST_F(FailoverTest, basics) {
  ASSERT_NO_THROW(init_cache());

  expect_metadata_1();
  expect_group_members_1();
  cache->refresh();

  // ensure that the instance list returned by a lookup is the expected one
  // in the case everything's online and well
  auto instances = cache->replicaset_lookup("default");

  ASSERT_EQ(3U, instances.size());
  EXPECT_EQ("3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39",
            instances[0].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadWrite, instances[0].mode);
  EXPECT_EQ("8148cba4-2ad5-456e-a04e-2ba73eb10cc5",
            instances[1].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadOnly, instances[1].mode);
  EXPECT_EQ("f0a2079f-8b90-4324-9eec-a0496c4338e0",
            instances[2].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadOnly, instances[2].mode);

  // this should succeed right away
  DelayCheck t;
  EXPECT_TRUE(cache->wait_primary_failover("default", 2s));
  EXPECT_LE(t.time_elapsed(), 1);

  // ensure no expected queries leftover
  ASSERT_FALSE(session->print_expected());
}

TEST_F(FailoverTest, primary_failover) {
  // normal operation
  // ----------------

  ASSERT_NO_THROW(init_cache());
  expect_metadata_1();
  expect_group_members_1();
  cache->refresh();

  // ensure that the instance list returned by a lookup is the expected one
  // in the case everything's online and well
  auto instances = cache->replicaset_lookup("default");

  ASSERT_EQ(3U, instances.size());
  EXPECT_EQ("3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39",
            instances[0].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadWrite, instances[0].mode);
  EXPECT_EQ("8148cba4-2ad5-456e-a04e-2ba73eb10cc5",
            instances[1].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadOnly, instances[1].mode);
  EXPECT_EQ("f0a2079f-8b90-4324-9eec-a0496c4338e0",
            instances[2].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadOnly, instances[2].mode);

  // this should succeed right away
  {
    DelayCheck t;
    EXPECT_TRUE(cache->wait_primary_failover("default", 2s));
    EXPECT_LE(t.time_elapsed(), 1);
  }

  // ensure no expected queries leftover
  ASSERT_FALSE(session->print_expected());

  // now the primary goes down (but group view not updated yet by GR)
  // ----------------------------------------------------------------
  expect_metadata_1();
  expect_group_members_1();
  ASSERT_NO_THROW(cache->refresh());

  cache->mark_instance_reachability(
      "3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39",
      metadata_cache::InstanceStatus::Unreachable);
  // this should fail with timeout b/c no primary yet
  {
    DelayCheck t;
    EXPECT_FALSE(cache->wait_primary_failover("default", 1s));
    EXPECT_GE(t.time_elapsed(), 1);
  }

  instances = cache->replicaset_lookup("default");

  ASSERT_EQ(3U, instances.size());
  // primary is still visible, even tho it's dead.. that's because we pretend
  // we're getting updates from an instance that hasn't noticed that yet
  EXPECT_EQ("3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39",
            instances[0].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadWrite, instances[0].mode);
  EXPECT_EQ("8148cba4-2ad5-456e-a04e-2ba73eb10cc5",
            instances[1].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadOnly, instances[1].mode);
  EXPECT_EQ("f0a2079f-8b90-4324-9eec-a0496c4338e0",
            instances[2].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadOnly, instances[2].mode);

  // GR notices the server went down, new primary picked
  // ---------------------------------------------------
  expect_metadata_1();
  expect_group_members_1_primary_fail(nullptr,
                                      "8148cba4-2ad5-456e-a04e-2ba73eb10cc5");
  cache->refresh();

  // this should succeed
  {
    DelayCheck t;
    EXPECT_TRUE(cache->wait_primary_failover("default", 2s));
    EXPECT_LE(t.time_elapsed(), 1);
  }

  instances = cache->replicaset_lookup("default");

  ASSERT_EQ(3U, instances.size());
  EXPECT_EQ("3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39",
            instances[0].mysql_server_uuid);
  EXPECT_EQ(ServerMode::Unavailable, instances[0].mode);
  EXPECT_EQ("8148cba4-2ad5-456e-a04e-2ba73eb10cc5",
            instances[1].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadWrite, instances[1].mode);
  EXPECT_EQ("f0a2079f-8b90-4324-9eec-a0496c4338e0",
            instances[2].mysql_server_uuid);
  EXPECT_EQ(ServerMode::ReadOnly, instances[2].mode);
}

TEST_F(FailoverTest, primary_failover_shutdown) {
  ASSERT_NO_THROW(init_cache());
  expect_metadata_1();
  expect_group_members_1();
  ASSERT_NO_THROW(cache->refresh());

  cache->mark_instance_reachability(
      "3c85a47b-7cc1-4fa8-bb4c-8f2dbf1c3c39",
      metadata_cache::InstanceStatus::Unreachable);

  auto wait_failover_thread = std::thread([&] {
    DelayCheck t;
    // even though we wait for 10s for the primary failover the function should
    // return promptly when the catche->stop() gets called (mimicking terminate
    // request)
    EXPECT_FALSE(cache->wait_primary_failover("default", 10s));
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
