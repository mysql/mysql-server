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

// must have these first, before #includes that rely on it
#include <gtest/gtest_prod.h>

#include "cluster_metadata.h"
#include "dim.h"
#include "group_replication_metadata.h"
#include "metadata_cache.h"
#include "mysqlrouter/mysql_session.h"
#include "test/helpers.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>

// ignore GMock warnings
#ifdef __clang__
#ifndef __has_warning
#define __has_warning(x) 0
#endif
#pragma clang diagnostic push
#if __has_warning("-Winconsistent-missing-override")
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif
#if __has_warning("-Wsign-conversion")
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
#include "gmock/gmock.h"
#else
#include "gmock/gmock.h"
#endif

using ::testing::Assign;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StartsWith;
using ::testing::Throw;
using ::testing::_;

using metadata_cache::ManagedInstance;
using metadata_cache::ManagedReplicaSet;
using metadata_cache::ServerMode;
using mysqlrouter::MySQLSession;

using State = GroupReplicationMember::State;
using Role = GroupReplicationMember::Role;
using RS = metadata_cache::ReplicasetStatus;

////////////////////////////////////////////////////////////////////////////////
//
// These tests focus on testing functionality implemented in
// metadata_cache.{h,cc}.
//
// Notes:
// - throughout tests we use human-readable UUIDs ("instance-1", "instance-2",
// etc)
//   for clarity, but actual code will deal with proper GUIDs (such as
//   "3acfe4ca-861d-11e6-9e56-08002741aeb6"). At the time of writing, these IDs
//   are treated like any other plain strings in production code (we call
//   empty(), operator==(), etc, on them, but we never parse them), thus
//   allowing us to use human-readable UUIDs in tests.
// - the test groups are arranged in order that they run in production. This
// should
//   help diagnose problems faster, as the stuff tested later depends on the
//   stuff tested earlier.
//
// TODO: At the time of writing, tests don't test multiple replicaset scenarios.
//       The code will probably work as is, but "it doesn't work until it's
//       proven by a unit test".
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @file
 * @brief These tests verify Metadata Cache's _refresh process_ at its different
 *        stages.
 */

const std::string query_schema_version =
    "SELECT * FROM mysql_innodb_cluster_metadata.schema_version";

// query #1 (occurrs first) - fetches expected (configured) topology from
// metadata server
std::string query_metadata =
    "SELECT "
    "R.replicaset_name, I.mysql_server_uuid, I.role, I.weight, "
    "I.version_token, H.location, "
    "I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' "
    "FROM mysql_innodb_cluster_metadata.clusters AS F "
    "JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = "
    "R.cluster_id "
    "JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id = "
    "I.replicaset_id "
    "JOIN mysql_innodb_cluster_metadata.hosts AS H ON I.host_id = H.host_id "
    "WHERE F.cluster_name = " /*'<cluster name>';"*/;

// query #2 (occurs second) - fetches primary member as seen by a particular
// node
std::string query_primary_member =
    "show status like 'group_replication_primary_member'";

// query #3 (occurs last) - fetches current topology as seen by a particular
// node
std::string query_status =
    "SELECT "
    "member_id, member_host, member_port, member_state, "
    "@@group_replication_single_primary_mode "
    "FROM performance_schema.replication_group_members "
    "WHERE channel_name = 'group_replication_applier'";

////////////////////////////////////////////////////////////////////////////////
//
// mock classes
//
////////////////////////////////////////////////////////////////////////////////

class MockMySQLSession : public MySQLSession {
 public:
  MOCK_METHOD2(query,
               void(const std::string &query, const RowProcessor &processor));
  MOCK_METHOD1(query_one, ResultRow *(const std::string &query));
  MOCK_METHOD2(flag_succeed, void(const std::string &, unsigned int));
  MOCK_METHOD2(flag_fail, void(const std::string &, unsigned int));

  void connect(const std::string &host, unsigned int port, const std::string &,
               const std::string &, const std::string &, const std::string &,
               int = kDefaultConnectTimeout,
               int = kDefaultReadTimeout) override {
    connect_cnt_++;

    std::string host_port = host + ':' + std::to_string(port);
    if (good_conns_.count(host_port))
      connect_succeed(host, port);
    else
      connect_fail(host, port);  // throws Error
  }

  void set_good_conns(std::set<std::string> &&conns) {
    good_conns_ = std::move(conns);
  }

  void query_impl(const RowProcessor &processor,
                  const std::vector<Row> &resultset,
                  bool should_succeed = true) const {
    // emulate real MySQLSession::query() error-handling logic
    if (!connected_) throw std::logic_error("Not connected");
    if (!should_succeed) {
      std::string s = "Error executing MySQL query: some error(42)";
      throw Error(s.c_str(), 42);
    }

    for (const Row &row : resultset) {
      if (!processor(row))  // processor is allowed to throw
        break;
    }
  }

 private:
  void connect_succeed(const std::string &host, unsigned int port) {
    flag_succeed(host, port);

    // emulate real MySQLSession::connect() behaviour on success
    connected_ = true;
    connection_address_ = host + ":" + std::to_string(port);
  }

  void connect_fail(const std::string &host, unsigned int port) {
    flag_fail(host, port);

    // emulate real MySQLSession::connect() behaviour on failure
    std::string s = "Error connecting to MySQL server at ";
    s += host + ":" + std::to_string(port) + ": some error(42)";
    throw Error(s.c_str(), 42);
  }

  int connect_cnt_ = 0;
  std::set<std::string> good_conns_;
};

class MockMySQLSessionFactory {
  const int kInstances = 4;

 public:
  MockMySQLSessionFactory() {
    // we pre-allocate instances and then return those in create() and get()
    for (int i = 0; i < kInstances; i++) {
      sessions_.emplace_back(new MockMySQLSession);
    }
  }

  std::shared_ptr<MySQLSession> create() const { return sessions_.at(next_++); }

  MockMySQLSession &get(unsigned i) const { return *sessions_.at(i); }

  int create_cnt() const {
    // without cast, we'd need to type 'u' everywhere, like so:
    // EXPECT_EQ(1u, factory.create_cnt());
    return static_cast<int>(next_);
  }

 private:
  // can't use vector<MockMySQLSession>, because MockMySQLSession is not
  // copyable due to GMock (produces weird linker errors)
  std::vector<std::shared_ptr<MockMySQLSession>> sessions_;

  mutable unsigned next_ = 0;
};

// tiny helper to create a row on the fly
class MockRow : public MySQLSession::ResultRow {
 public:
  explicit MockRow(const MySQLSession::Row &row) { row_ = row; }
};

static bool cmp_mi_FIFMS(const ManagedInstance &lhs,
                         const ManagedInstance &rhs) {
  // This function compares fields set by
  // Metadata::fetch_instances_from_metadata_server(). Ignored fields (they're
  // not being set at the time of writing):
  //   ServerMode mode;

  return lhs.replicaset_name == rhs.replicaset_name &&
         lhs.mysql_server_uuid == rhs.mysql_server_uuid &&
         lhs.role == rhs.role && std::fabs(lhs.weight - rhs.weight) < 0.001 &&
         lhs.version_token == rhs.version_token &&
         lhs.location == rhs.location && lhs.host == rhs.host &&
         lhs.port == rhs.port && lhs.xport == rhs.xport;
}

static bool cmp_mi_FI(const ManagedInstance &lhs, const ManagedInstance &rhs) {
  // This function compares fields set by Metadata::fetch_instances().
  // Ignored fields (they're not being set at the time of writing):
  //   std::string role;
  //   float weight;
  //   unsigned int version_token;
  //   std::string location;

  return lhs.replicaset_name == rhs.replicaset_name &&
         lhs.mysql_server_uuid == rhs.mysql_server_uuid &&
         lhs.mode == rhs.mode && lhs.host == rhs.host && lhs.port == rhs.port &&
         lhs.xport == rhs.xport;
}

////////////////////////////////////////////////////////////////////////////////
//
// test class
//
////////////////////////////////////////////////////////////////////////////////

class MetadataTest : public ::testing::Test {
 public:
  void SetUp() override {
    // redirect cout > nothing (following tests print to cout)
    original_cout = std::cout.rdbuf();
    std::cout.rdbuf(nullptr);

    // setup DI for MySQLSession
    mysql_harness::DIM::instance().set_MySQLSession(
        [this]() {
          return session_factory.create().get();
        },                     // provide raw pointer
        [](MySQLSession *) {}  // and try don't deleting it!
    );
  }

  void TearDown() override {
    // undo cout redirect
    std::cout.rdbuf(original_cout);
  }

  std::streambuf *original_cout;

  //---- helper functions
  //--------------------------------------------------------

  void connect_to_first_metadata_server() {
    std::vector<ManagedInstance> metadata_servers{
        {"replicaset-1", "instance-1", "", ServerMode::ReadWrite, 0, 0, "",
         "localhost", 3310, 33100},
    };
    session_factory.get(0).set_good_conns(
        {"127.0.0.1:3310", "127.0.0.1:3320", "127.0.0.1:3330"});

    EXPECT_CALL(session_factory.get(0), flag_succeed(_, 3310)).Times(1);
    EXPECT_TRUE(metadata.connect(metadata_servers[0]));
  }

  void enable_connection(unsigned session, unsigned port) {
    session_factory.get(session).set_good_conns(
        {std::string("127.0.0.1:") +
         std::to_string(port)});  // \_ new connection
    EXPECT_CALL(session_factory.get(session), flag_succeed(_, port))
        .Times(1);  // /  should succeed
  }

  //----- mock SQL queries
  //-------------------------------------------------------

  std::function<void(const std::string &,
                     const MySQLSession::RowProcessor &processor)>
  query_primary_member_ok(unsigned session) {
    return [this, session](const std::string &,
                           const MySQLSession::RowProcessor &processor) {
      session_factory.get(session).query_impl(
          processor, {{"group_replication_primary_member",
                       "instance-1"}});  // typical response
    };
  }

  std::function<void(const std::string &,
                     const MySQLSession::RowProcessor &processor)>
  query_primary_member_empty(unsigned session) {
    return [this, session](const std::string &,
                           const MySQLSession::RowProcessor &processor) {
      session_factory.get(session).query_impl(
          processor,
          {{"group_replication_primary_member", ""}});  // empty response
    };
  }

  std::function<void(const std::string &,
                     const MySQLSession::RowProcessor &processor)>
  query_primary_member_fail(unsigned session) {
    return [this, session](const std::string &,
                           const MySQLSession::RowProcessor &processor) {
      session_factory.get(session).query_impl(
          processor, {}, false);  // false = induce fail query
    };
  }

  std::function<void(const std::string &,
                     const MySQLSession::RowProcessor &processor)>
  query_status_fail(unsigned session) {
    return [this, session](const std::string &,
                           const MySQLSession::RowProcessor &processor) {
      session_factory.get(session).query_impl(
          processor, {}, false);  // false = induce fail query
    };
  }

  std::function<void(const std::string &,
                     const MySQLSession::RowProcessor &processor)>
  query_status_ok(unsigned session) {
    return [this, session](const std::string &,
                           const MySQLSession::RowProcessor &processor) {
      session_factory.get(session).query_impl(
          processor, {
                         {"instance-1", "ubuntu", "3310", "ONLINE", "1"},  // \.
                         {"instance-2", "ubuntu", "3320", "ONLINE",
                          "1"},  //  > typical response
                         {"instance-3", "ubuntu", "3330", "ONLINE", "1"},  // /
                     });
    };
  }

 private:  // toggling between public and private because we require these vars
           // in this particular order
  std::unique_ptr<MockMySQLSessionFactory> up_session_factory_{
      new MockMySQLSessionFactory()};

 public:
  MockMySQLSessionFactory &session_factory =
      *up_session_factory_;  // hack: we can do this because unique_ptr will
                             // outlive our tests
  ClusterMetadata metadata{"user",
                           "pass",
                           0,
                           0,
                           0,
                           std::chrono::milliseconds(0),
                           mysqlrouter::SSLOptions()};

  // set instances that would be returned by successful
  // metadata.fetch_instances_from_metadata_server() for a healthy 3-node setup.
  // Only some tests need this variable.

  const ManagedReplicaSet typical_replicaset{
      "replicaset-1",
      {
          // will be set ----------------------vvvvvvvvvvvvvvvvvvvvvvv
          // v--v--vv--- ignored at the time of writing
          {"replicaset-1", "instance-1", "HA", ServerMode::Unavailable, 0, 0,
           "", "localhost", 3310, 33100},
          {"replicaset-1", "instance-2", "HA", ServerMode::Unavailable, 0, 0,
           "", "localhost", 3320, 33200},
          {"replicaset-1", "instance-3", "HA", ServerMode::Unavailable, 0, 0,
           "", "localhost", 3330, 33300},
          // ignored at time of writing
          // -^^^^--------------------------------------------------------^^^^^
          // TODO: ok to ignore xport?
      },
      false};
};

////////////////////////////////////////////////////////////////////////////////
//
// test ClusterMetadata::connect()
//
////////////////////////////////////////////////////////////////////////////////

TEST_F(MetadataTest, ConnectToMetadataServer_Succeed) {
  ManagedInstance metadata_server{
      "replicaset-1", "instance-1", "",   ServerMode::ReadWrite, 0, 0, "",
      "localhost",    3310,         33100};
  session_factory.get(0).set_good_conns({"127.0.0.1:3310"});

  // should connect successfully
  EXPECT_CALL(session_factory.get(0), flag_succeed(_, 3310)).Times(1);
  EXPECT_TRUE(metadata.connect(metadata_server));
}

TEST_F(MetadataTest, ConnectToMetadataServer_Failed) {
  ManagedInstance metadata_server{
      "replicaset-1", "instance-1", "",   ServerMode::ReadWrite, 0, 0, "",
      "localhost",    3310,         33100};

  // connetion attempt should fail
  EXPECT_CALL(session_factory.get(0), flag_fail(_, 3310)).Times(1);
  EXPECT_FALSE(metadata.connect(metadata_server));
}

////////////////////////////////////////////////////////////////////////////////
//
// test ClusterMetadata::fetch_instances_from_metadata_server()
// [QUERY #1: query_metadata]
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @test
 * Verify that `ClusterMetadata::fetch_instances_from_metadata_server()` returns
 * correct information that it obtains from MD server via SQL query. Tested
 * result sets:
 *
 *   1. empty
 *   2. many nodes in many replicasets
 *   3. SQL query fails
 */
TEST_F(MetadataTest, FetchInstancesFromMetadataServer) {
  connect_to_first_metadata_server();

  // test automatic conversions
  {
    EXPECT_CALL(session_factory.get(0),
                query_one(StartsWith(query_schema_version)))
        .Times(1)
        .WillOnce(Return(new MockRow({"1", "0", "1"})));

    auto resultset_metadata = [this](
                                  const std::string &,
                                  const MySQLSession::RowProcessor &processor) {
      session_factory.get(0).query_impl(
          processor,
          {
              {"replicaset-1", "instance-1", "HA", "0.2", "0", "location1",
               "localhost:3310", "localhost:33100"},
              {"replicaset-1", "instance-2", "arbitrary_string", "1.5", "1",
               "s.o_loc", "localhost:3320", NULL},
              {"replicaset-1", "instance-3", "", "0.0", "99", "", "localhost",
               NULL},
              {"replicaset-1", "instance-4", "", NULL, NULL, "", NULL, NULL},
          });
    };
    EXPECT_CALL(session_factory.get(0), query(StartsWith(query_metadata), _))
        .Times(1)
        .WillOnce(Invoke(resultset_metadata));

    ASSERT_NO_THROW({
      ClusterMetadata::ReplicaSetsByName rs =
          metadata.fetch_instances_from_metadata_server("replicaset-1");

      EXPECT_EQ(1u, rs.size());
      EXPECT_EQ(4u, rs.at("replicaset-1").members.size());  // not set/checked
      // -------------------vvvvvvvvvvvvvvvvvvvvvvv
      EXPECT_TRUE(
          cmp_mi_FIFMS(ManagedInstance{"replicaset-1", "instance-1", "HA",
                                       ServerMode::Unavailable, 0.2f, 0,
                                       "location1", "localhost", 3310, 33100},
                       rs.at("replicaset-1").members.at(0)));
      EXPECT_TRUE(cmp_mi_FIFMS(
          ManagedInstance{"replicaset-1", "instance-2", "arbitrary_string",
                          ServerMode::Unavailable, 1.5f, 1, "s.o_loc",
                          "localhost", 3320, 33200},
          rs.at("replicaset-1").members.at(1)));
      EXPECT_TRUE(
          cmp_mi_FIFMS(ManagedInstance{"replicaset-1", "instance-3", "",
                                       ServerMode::Unavailable, 0.0f, 99, "",
                                       "localhost", 3306, 33060},
                       rs.at("replicaset-1").members.at(2)));
      EXPECT_TRUE(cmp_mi_FIFMS(ManagedInstance{"replicaset-1", "instance-4", "",
                                               ServerMode::Unavailable, 0.0f, 0,
                                               "", "", 3306, 33060},
                               rs.at("replicaset-1").members.at(3)));
      // TODO is this really right behavior?
      // ---------------------------------------------------------------------------------------------------^^
    });
  }

  // empty result
  {
    EXPECT_CALL(session_factory.get(0),
                query_one(StartsWith(query_schema_version)))
        .Times(1)
        .WillOnce(Return(new MockRow({"1", "0", "1"})));
    auto resultset_metadata = [this](
                                  const std::string &,
                                  const MySQLSession::RowProcessor &processor) {
      session_factory.get(0).query_impl(processor, {});
    };
    EXPECT_CALL(session_factory.get(0), query(StartsWith(query_metadata), _))
        .Times(1)
        .WillOnce(Invoke(resultset_metadata));

    ASSERT_NO_THROW({
      ClusterMetadata::ReplicaSetsByName rs =
          metadata.fetch_instances_from_metadata_server("replicaset-1");

      EXPECT_EQ(0u, rs.size());
    });
  }

  // multiple replicasets
  {
    EXPECT_CALL(session_factory.get(0),
                query_one(StartsWith(query_schema_version)))
        .Times(1)
        .WillOnce(Return(new MockRow({"1", "0", "1"})));
    auto resultset_metadata = [this](
                                  const std::string &,
                                  const MySQLSession::RowProcessor &processor) {
      session_factory.get(0).query_impl(
          processor, {
                         {"replicaset-2", "instance-4", "HA", NULL, NULL, "",
                          "localhost2:3333", NULL},
                         {"replicaset-1", "instance-1", "HA", NULL, NULL, "",
                          "localhost1:1111", NULL},
                         {"replicaset-1", "instance-2", "HA", NULL, NULL, "",
                          "localhost1:2222", NULL},
                         {"replicaset-1", "instance-3", "HA", NULL, NULL, "",
                          "localhost1:3333", NULL},
                         {"replicaset-3", "instance-5", "HA", NULL, NULL, "",
                          "localhost3:3333", NULL},
                         {"replicaset-3", "instance-6", "HA", NULL, NULL, "",
                          "localhost3:3333", NULL},
                     });
    };
    EXPECT_CALL(session_factory.get(0), query(StartsWith(query_metadata), _))
        .Times(1)
        .WillOnce(Invoke(resultset_metadata));

    ASSERT_NO_THROW({
      ClusterMetadata::ReplicaSetsByName rs =
          metadata.fetch_instances_from_metadata_server("replicaset-1");

      EXPECT_EQ(3u, rs.size());
      EXPECT_EQ(3u, rs.at("replicaset-1").members.size());
      EXPECT_TRUE(
          cmp_mi_FIFMS(ManagedInstance{"replicaset-1", "instance-1", "HA",
                                       ServerMode::Unavailable, 0, 0, "",
                                       "localhost1", 1111, 11110},
                       rs.at("replicaset-1").members.at(0)));
      EXPECT_TRUE(
          cmp_mi_FIFMS(ManagedInstance{"replicaset-1", "instance-2", "HA",
                                       ServerMode::Unavailable, 0, 0, "",
                                       "localhost1", 2222, 22220},
                       rs.at("replicaset-1").members.at(1)));
      EXPECT_TRUE(
          cmp_mi_FIFMS(ManagedInstance{"replicaset-1", "instance-3", "HA",
                                       ServerMode::Unavailable, 0, 0, "",
                                       "localhost1", 3333, 33330},
                       rs.at("replicaset-1").members.at(2)));
      EXPECT_EQ(1u, rs.at("replicaset-2").members.size());
      EXPECT_TRUE(
          cmp_mi_FIFMS(ManagedInstance{"replicaset-2", "instance-4", "HA",
                                       ServerMode::Unavailable, 0, 0, "",
                                       "localhost2", 3333, 33330},
                       rs.at("replicaset-2").members.at(0)));
      EXPECT_EQ(2u, rs.at("replicaset-3").members.size());
      EXPECT_TRUE(
          cmp_mi_FIFMS(ManagedInstance{"replicaset-3", "instance-5", "HA",
                                       ServerMode::Unavailable, 0, 0, "",
                                       "localhost3", 3333, 33330},
                       rs.at("replicaset-3").members.at(0)));
      EXPECT_TRUE(
          cmp_mi_FIFMS(ManagedInstance{"replicaset-3", "instance-6", "HA",
                                       ServerMode::Unavailable, 0, 0, "",
                                       "localhost3", 3333, 33330},
                       rs.at("replicaset-3").members.at(1)));
    });
  }

  // query fails
  {
    EXPECT_CALL(session_factory.get(0),
                query_one(StartsWith(query_schema_version)))
        .Times(1)
        .WillOnce(Return(new MockRow({"1", "0", "1"})));
    auto resultset_metadata = [this](
                                  const std::string &,
                                  const MySQLSession::RowProcessor &processor) {
      session_factory.get(0).query_impl(processor, {}, false);
    };
    EXPECT_CALL(session_factory.get(0), query(StartsWith(query_metadata), _))
        .Times(1)
        .WillOnce(Invoke(resultset_metadata));

    // exception thrown by MySQLSession::query() should get repackaged in
    // metadata_cache::metadata_error
    ClusterMetadata::ReplicaSetsByName rs;
    try {
      rs = metadata.fetch_instances_from_metadata_server("replicaset-1");
      FAIL() << "Expected metadata_cache::metadata_error to be thrown";
    } catch (const metadata_cache::metadata_error &e) {
      EXPECT_STREQ("Error executing MySQL query: some error(42)", e.what());
      EXPECT_EQ(0u, rs.size());
    } catch (...) {
      FAIL() << "Expected metadata_cache::metadata_error to be thrown";
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// test ClusterMetadata::check_replicaset_status()
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @test
 * Verify that `ClusterMetadata::check_replicaset_status()` returns proper
 * status for each node (instance) that it received from MD server, and
 * calculates proper replicaset availability.
 *
 * The tested function has two inputs: MD (cluster topology from MD server) and
 * GR (health status from GR tables). All tested scenarios in this test keep the
 * MD constant (3 nodes) and while varying the GR.
 */
TEST_F(MetadataTest, CheckReplicasetStatus_3NodeSetup) {
  std::vector<ManagedInstance> servers_in_metadata{
      // ServerMode doesn't matter ------vvvvvvvvvvv
      {"", "instance-1", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "instance-2", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "instance-3", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
  };

  // typical
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Primary}},
        {"instance-2", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);
  }

  // less typical
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-2", {"", "", 0, State::Online, Role::Primary}},
        {"instance-3", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);

    auto r = {ServerMode::ReadOnly, ServerMode::ReadWrite,
              ServerMode::ReadOnly};
    EXPECT_TRUE(std::equal(
        r.begin(), r.end(), servers_in_metadata.begin(),
        [](ServerMode mode, ManagedInstance mi) { return mode == mi.mode; }));
  }

  // less typical
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-2", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Online, Role::Primary}},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(2).mode);

    auto r = {ServerMode::ReadOnly, ServerMode::ReadOnly,
              ServerMode::ReadWrite};
    EXPECT_TRUE(std::equal(
        r.begin(), r.end(), servers_in_metadata.begin(),
        [](ServerMode mode, ManagedInstance mi) { return mode == mi.mode; }));
  }

  // no primary
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-2", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableReadOnly, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);

    auto r = {ServerMode::ReadOnly, ServerMode::ReadOnly, ServerMode::ReadOnly};
    EXPECT_TRUE(std::equal(
        r.begin(), r.end(), servers_in_metadata.begin(),
        [](ServerMode mode, ManagedInstance mi) { return mode == mi.mode; }));
  }

  // multi-primary (currently unsupported, but treat as single-primary)
  // TODO: this behaviour should change, probably turn all Primary ->
  // Unavailable but leave Secondary alone
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Primary}},
        {"instance-2", {"", "", 0, State::Online, Role::Primary}},
        {"instance-3", {"", "", 0, State::Online, Role::Secondary}},
    };
#ifdef NDEBUG  // guardian assert() should fail in Debug
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);

    auto r = {ServerMode::ReadWrite, ServerMode::ReadWrite,
              ServerMode::ReadOnly};
    EXPECT_TRUE(std::equal(
        r.begin(), r.end(), servers_in_metadata.begin(),
        [](ServerMode mode, ManagedInstance mi) { return mode == mi.mode; }));
#endif
  }

  // 1 node missing
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Primary}},
        {"instance-3", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);
    // should log warning "Member <host>:<port> (instance-2) defined in metadata
    // not found in actual replicaset"
  }

  // 1 node missing, no primary
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-2", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableReadOnly, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);
    // should log warning "Member <host>:<port> (instance-1) defined in metadata
    // not found in actual replicaset"
  }

  // 2 nodes missing
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Primary}},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
    // should log warning "Member <host>:<port> (instance-2) defined in metadata
    // not found in actual replicaset" should log warning "Member <host>:<port>
    // (instance-3) defined in metadata not found in actual replicaset"
  }

  // 2 nodes missing, no primary
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-3", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableReadOnly, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);
    // should log warning "Member <host>:<port> (instance-1) defined in metadata
    // not found in actual replicaset" should log warning "Member <host>:<port>
    // (instance-2) defined in metadata not found in actual replicaset"
  }

  // all nodes missing
  {
    std::map<std::string, GroupReplicationMember> server_status{};
    EXPECT_EQ(RS::Unavailable, metadata.check_replicaset_status(
                                   servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
    // should log warning "Member <host>:<port> (instance-1) defined in metadata
    // not found in actual replicaset" should log warning "Member <host>:<port>
    // (instance-2) defined in metadata not found in actual replicaset" should
    // log warning "Member <host>:<port> (instance-3) defined in metadata not
    // found in actual replicaset"
  }

  // 1 unknown id
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-4",
         {"instance-4", "host4", 4444, State::Online, Role::Secondary}},
        {"instance-2", {"", "", 0, State::Online, Role::Primary}},
        {"instance-3", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);
    // should log warning "Member <host>:<port> (instance-1) defined in metadata
    // not found in actual replicaset" should log error "Member <host>:<port>
    // (instance-4) found in replicaset, yet is not defined in metadata!"
  }

  // 2 unknown ids
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-4", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-2", {"", "", 0, State::Online, Role::Primary}},
        {"instance-5", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::Unavailable, metadata.check_replicaset_status(
                                   servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
    // should log warning "Member <host>:<port> (instance-1) defined in metadata
    // not found in actual replicaset" should log warning "Member <host>:<port>
    // (instance-3) defined in metadata not found in actual replicaset" should
    // log error "Member <host>:<port> (instance-4) found in replicaset, yet is
    // not defined in metadata!" should log error "Member <host>:<port>
    // (instance-5) found in replicaset, yet is not defined in metadata!"
  }

  // more nodes than expected
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Primary}},
        {"instance-2", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-4", {"", "", 0, State::Online, Role::Primary}},
        {"instance-5", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);
    // should log error "Member <host>:<port> (instance-4) found in replicaset,
    // yet is not defined in metadata!" should log error "Member <host>:<port>
    // (instance-5) found in replicaset, yet is not defined in metadata!"
  }
}

/**
 * @test
 * Verify that `ClusterMetadata::check_replicaset_status()` returns proper
 * status for each node (instance) that it received from MD server, and
 * calculates proper replicaset availability.
 *
 * This test is similar to `CheckReplicasetStatus_3NodeSetup`, but here we the
 * inputs flip: MD is variable, GR is always 3 nodes.
 */
TEST_F(MetadataTest, CheckReplicasetStatus_VariableNodeSetup) {
  std::map<std::string, GroupReplicationMember> server_status{
      {"instance-1", {"", "", 0, State::Online, Role::Primary}},
      {"instance-2", {"", "", 0, State::Online, Role::Secondary}},
      {"instance-3", {"", "", 0, State::Online, Role::Secondary}},
  };

  // Next 2 scenarios test situation in which the status report (view) contains
  // only a subset of servers provided by metadata server. At the time of
  // writing, this longer list of servers is essentially irrelevant, and the
  // "view" is king. See notes in ClusterMetadata::check_replicaset_status() for
  // more info.

  // 7-node setup according to metadata
  {
    std::vector<ManagedInstance> servers_in_metadata{
        // ServerMode doesn't matter ------vvvvvvvvvvv
        {"", "instance-1", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
        {"", "instance-2", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
        {"", "instance-3", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
        {"", "instance-4", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
        {"", "instance-5", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
        {"", "instance-6", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
        {"", "instance-7", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);
    // should log warning "Member <host>:<port> (instance-*) defined in metadata
    // not found in actual replicaset" for instanes 4-7
  }

  // 4-node setup according to metadata
  {
    std::vector<ManagedInstance> servers_in_metadata{
        {"", "instance-1", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
        {"", "instance-2", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
        {"", "instance-3", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
        {"", "instance-4", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);
    // should log warning "Member <host>:<port> (instance-4) defined in metadata
    // not found in actual replicaset"
  }

  // This time, the status report (view) contains some servers not defined by
  // metadata server. Here the situation is a little different: the "view" is
  // still what matters, but subject to one restriction: nodes not defined in
  // metadata don't count, they're ignored.
  // NOTE that these scenarios should never happen, and if they do, the DBA
  // is at fault (the setup is messed up). Here we only test how our system will
  // handle such bad setup, and it should handle it defensively, err on the safe
  // side. Indeed, in case of undefined nodes, they will be not be counted
  // towards reaching quorum, making attaining quorum more difficult.

  // 2-node setup according to metadata -> quorum requires 3 nodes, 2 nodes
  // count
  {
    std::vector<ManagedInstance> servers_in_metadata{
        {"", "instance-1", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
        {"", "instance-2", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
    // should log error "Member <host>:<port> (instance-3) found in replicaset,
    // yet is not defined in metadata!"
  }

  // 1-node setup according to metadata -> quorum requires 3 nodes, 1 node
  // counts
  {
    std::vector<ManagedInstance> servers_in_metadata{
        {"", "instance-1", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
    };
    EXPECT_EQ(RS::Unavailable, metadata.check_replicaset_status(
                                   servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    // should log error "Member <host>:<port> (instance-2) found in replicaset,
    // yet is not defined in metadata!" should log error "Member <host>:<port>
    // (instance-3) found in replicaset, yet is not defined in metadata!"
  }

  // 0-node setup according to metadata -> quorum requires 3 nodes, 0 node count
  {
    std::vector<ManagedInstance> servers_in_metadata{};
    EXPECT_EQ(RS::Unavailable, metadata.check_replicaset_status(
                                   servers_in_metadata, server_status));
    // should log error "Member <host>:<port> (instance-1) found in replicaset,
    // yet is not defined in metadata!" should log error "Member <host>:<port>
    // (instance-2) found in replicaset, yet is not defined in metadata!" should
    // log error "Member <host>:<port> (instance-3) found in replicaset, yet is
    // not defined in metadata!"
  }
}

/**
 * @test
 * Verify that `ClusterMetadata::check_replicaset_status()` returns proper
 * status for each node (instance) that it received from MD server, and
 * calculates proper replicaset availability.
 *
 * This test focuses on scenarios where 1 and 2 nodes (out of 3-node setup) are
 * in one of unavailable states (offline, error, unreachable, other).
 */
TEST_F(MetadataTest, CheckReplicasetStatus_VariousStatuses) {
  std::vector<ManagedInstance> servers_in_metadata{
      // ServerMode doesn't matter ------vvvvvvvvvvv
      {"", "instance-1", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "instance-2", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "instance-3", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
  };

  for (State state :
       {State::Offline, State::Error, State::Unreachable, State::Other}) {
    // should keep quorum
    {
      std::map<std::string, GroupReplicationMember> server_status{
          {"instance-1", {"", "", 0, State::Online, Role::Primary}},
          {"instance-2", {"", "", 0, State::Online, Role::Secondary}},
          {"instance-3", {"", "", 0, state, Role::Secondary}},
      };
      EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                           servers_in_metadata, server_status));
      EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
      EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
      EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
    }

    // should keep quorum
    {
      std::map<std::string, GroupReplicationMember> server_status{
          {"instance-1", {"", "", 0, State::Online, Role::Secondary}},
          {"instance-2", {"", "", 0, State::Online, Role::Secondary}},
          {"instance-3", {"", "", 0, state, Role::Secondary}},
      };
      EXPECT_EQ(RS::AvailableReadOnly, metadata.check_replicaset_status(
                                           servers_in_metadata, server_status));
      EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(0).mode);
      EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
      EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
    }

    // should lose quorum
    {
      std::map<std::string, GroupReplicationMember> server_status{
          {"instance-1", {"", "", 0, State::Online, Role::Primary}},
          {"instance-2", {"", "", 0, state, Role::Secondary}},
          {"instance-3", {"", "", 0, state, Role::Secondary}},
      };
      EXPECT_EQ(RS::Unavailable, metadata.check_replicaset_status(
                                     servers_in_metadata, server_status));
      EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
      EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
      EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
    }
  }
}

/**
 * @test
 * Verify that `ClusterMetadata::check_replicaset_status()` returns proper
 * status for each node (instance) that it received from MD server, and
 * calculates proper replicaset availability.
 *
 * Here we test various scenarios with RECOVERING nodes. RECOVERING nodes
 * should be treated as valid quorum members just like ONLINE nodes, but they
 * cannot be routed to. RS::Recovering should be returned in a (corner) case
 * when all nodes in quorum are recovering.
 */
TEST_F(MetadataTest, CheckReplicasetStatus_Recovering) {
  std::vector<ManagedInstance> servers_in_metadata{
      // ServerMode doesn't matter ------vvvvvvvvvvv
      {"", "instance-1", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "instance-2", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "instance-3", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
  };

  // 1 node recovering, 1 RW, 1 RO
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Primary}},
        {"instance-2", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
  }

  // 1 node recovering, 1 offline, 1 RW
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Primary}},
        {"instance-2", {"", "", 0, State::Error, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
  }

  // 1 node recovering, 1 offline, 1 RO
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-2", {"", "", 0, State::Error, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableReadOnly, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
  }

  // 1 node recovering, 2 offline
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Error, Role::Secondary}},
        {"instance-2", {"", "", 0, State::Error, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(RS::Unavailable, metadata.check_replicaset_status(
                                   servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
  }

  // 1 node recovering, 1 offline, 1 left replicaset
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-2", {"", "", 0, State::Error, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(RS::Unavailable, metadata.check_replicaset_status(
                                   servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
  }

  // 1 node recovering, 2 left replicaset
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(
        RS::UnavailableRecovering,
        metadata.check_replicaset_status(servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
  }

  // 2 nodes recovering, 1 RW
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Primary}},
        {"instance-2", {"", "", 0, State::Recovering, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableWritable, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
  }

  // 2 nodes recovering, 1 RO
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Online, Role::Secondary}},
        {"instance-2", {"", "", 0, State::Recovering, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(RS::AvailableReadOnly, metadata.check_replicaset_status(
                                         servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
  }

  // 2 nodes recovering, 1 offline
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Error, Role::Secondary}},
        {"instance-2", {"", "", 0, State::Recovering, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(
        RS::UnavailableRecovering,
        metadata.check_replicaset_status(servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
  }

  // 2 nodes recovering, 1 left replicaset
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-2", {"", "", 0, State::Recovering, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(
        RS::UnavailableRecovering,
        metadata.check_replicaset_status(servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
  }

  // 3 nodes recovering
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"instance-1", {"", "", 0, State::Recovering, Role::Secondary}},
        {"instance-2", {"", "", 0, State::Recovering, Role::Secondary}},
        {"instance-3", {"", "", 0, State::Recovering, Role::Secondary}},
    };
    EXPECT_EQ(
        RS::UnavailableRecovering,
        metadata.check_replicaset_status(servers_in_metadata, server_status));
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
  }
}

/**
 * @test
 * Verify that `ClusterMetadata::check_replicaset_status()` returns proper
 * status for each node (instance) that it received from MD server, and
 * calculates proper replicaset availability.
 *
 * Here we test an interesting cornercase:
 *
 *     MD defines nodes A, B, C
 *     GR defines nodes A, B, C, D, E
 *     A, B are alive; C, D, E are dead
 *
 * Availability calculation should deem replicaset to be unavailable, because
 * only 2 of 5 nodes are alive, even though looking purely from MD
 * point-of-view, 2 of its 3 nodes are still alive, thus could be considered a
 * quorum.
 */
TEST_F(MetadataTest, CheckReplicasetStatus_Cornercase2of5Alive) {
  // MD defines 3 nodes
  std::vector<ManagedInstance> servers_in_metadata{
      {"", "node-A", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "node-B", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "node-C", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
  };

  // GR reports 5 nodes, of which only 2 are alive (no qourum), BUT from
  // perspective of MD-defined nodes, 2 of its 3 are alive (have quorum).
  // We choose to be pessimistic (no quorum)
  for (State dead_state :
       {State::Offline, State::Error, State::Unreachable, State::Other}) {
    std::map<std::string, GroupReplicationMember> server_status{
        {"node-A", {"", "", 0, State::Online, Role::Primary}},
        {"node-B", {"", "", 0, State::Online, Role::Secondary}},
        {"node-C", {"", "", 0, dead_state, Role::Secondary}},
        {"node-D", {"", "", 0, dead_state, Role::Secondary}},
        {"node-E", {"", "", 0, dead_state, Role::Secondary}},
    };
    EXPECT_EQ(RS::Unavailable, metadata.check_replicaset_status(
                                   servers_in_metadata, server_status));
    // should log error "Member <host>:<port> (node-D) found in replicaset, yet
    // is not defined in metadata!" should log error "Member <host>:<port>
    // (node-E) found in replicaset, yet is not defined in metadata!"

    // meeting these is not strictly required, because when the cluster is
    // unavailable, ATTOW these results will be ignored. But OTOH, there's no
    // reason why these computations should fail - so we use the opportunity to
    // check if they still compute correctly despite replicaset being
    // unavailable. If one day we need these results to compute differently,
    // please feel free to erase these tests.
    EXPECT_EQ(3u, servers_in_metadata
                      .size());  // new nodes reported by GR will not be added
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(2).mode);
  }
}

/**
 * @test
 * Verify that `ClusterMetadata::check_replicaset_status()` returns proper
 * status for each node (instance) that it received from MD server, and
 * calculates proper replicaset availability.
 *
 * Here we test an interesting cornercase:
 *
 *     MD defines nodes A, B, C
 *     GR defines nodes A, B, C, D, E
 *     A, B are dead, C, D, E are alive
 *
 * Availability calculation, if fully GR-aware, could deem replicaset as
 * available, because looking from purely GR perspective, 3 of 5 nodes form
 * quorum.
 *
 * However, our availability calculation in
 * `ClusterMetadata::check_replicaset_status()` always assumes that MD is in
 * sync with GR (which it always should be), but just in case it violates this
 * assumption, it prefers to err on the side of caution. This erring on the side
 * of caution is demonstrated in this test, where the availability is judged as
 * not available, even though it could be. But that's the price we pay in
 * exchange for the safety the algorithm provides which is demonstrated in the
 * CheckReplicasetStatus_Cornercase2of5Alive testcase.
 */
TEST_F(MetadataTest, CheckReplicasetStatus_Cornercase3of5Alive) {
  // NOTE: If this test starts failing one day because check_replicaset_status()
  //       starts returning that the replicaset is available, it might be a good
  //       thing, BUT ONLY AS LONG as CheckReplicasetStatus_Cornercase2of5Alive
  //       is also passing. Please read the description of that test, and this
  //       one, before drawing conclusions.

  // MD defines 3 nodes
  std::vector<ManagedInstance> servers_in_metadata{
      {"", "node-A", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "node-B", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "node-C", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
  };

  // GR reports 5 nodes, of which 3 are alive (have qourum), BUT from
  // the perspective of MD-defined nodes, only 1 of its 3 is alive (no quorum).
  // We choose to be pessimistic (no quorum)
  for (State dead_state :
       {State::Offline, State::Error, State::Unreachable, State::Other}) {
    std::map<std::string, GroupReplicationMember> server_status{
        {"node-A", {"", "", 0, dead_state, Role::Primary}},
        {"node-B", {"", "", 0, dead_state, Role::Secondary}},
        {"node-C", {"", "", 0, State::Online, Role::Secondary}},
        {"node-D", {"", "", 0, State::Online, Role::Secondary}},
        {"node-E", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::Unavailable, metadata.check_replicaset_status(
                                   servers_in_metadata, server_status));
    // should log error "Member <host>:<port> (node-D) found in replicaset, yet
    // is not defined in metadata!" should log error "Member <host>:<port>
    // (node-E) found in replicaset, yet is not defined in metadata!"

    // meeting these is not strictly required, because when the cluster is
    // unavailable, ATTOW these results will be ignored. But OTOH, there's no
    // reason why these computations should fail - so we use the opportunity to
    // check if they still compute correctly despite replicaset being
    // unavailable. If one day we need these results to compute differently,
    // please feel free to erase these tests.
    EXPECT_EQ(3u, servers_in_metadata
                      .size());  // new nodes reported by GR will not be added
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadOnly, servers_in_metadata.at(2).mode);
  }
}

/**
 * @test
 * Verify that `ClusterMetadata::check_replicaset_status()` returns proper
 * status for each node (instance) that it received from MD server, and
 * calculates proper replicaset availability.
 *
 * Here we test an interesting cornercase:
 *
 *     MD defines nodes A, B, C
 *     GR defines nodes       C, D, E
 *     A, B are not reported by GR, C, D, E are alive
 *
 * According to GR, there's a quorum between nodes C, D and E. However, from MD
 * point-of-view, A, B went missing and only C is known to be alive.
 *
 * Our availability calculation in `ClusterMetadata::check_replicaset_status()`
 * always assumes that MD is in sync with GR (which it always should be), but
 * just in case it violates this assumption, it prefers to err on the side of
 * caution. This erring on the side of caution is demonstrated in this test,
 * where the availability is judged as not available, even though it could be.
 * But that's the price we pay in exchange for the safety the algorithm provides
 * which is demonstrated in the CheckReplicasetStatus_Cornercase2of5Alive
 * testcase.
 */
TEST_F(MetadataTest, CheckReplicasetStatus_Cornercase1Common) {
  // NOTE: If this test starts failing one day because check_replicaset_status()
  //       starts returning that the replicaset is available, it might be a good
  //       thing, BUT ONLY AS LONG as CheckReplicasetStatus_Cornercase2of5Alive
  //       is also passing. Please read the description of that test, and this
  //       one, before drawing conclusions.

  // MD defines 3 nodes
  std::vector<ManagedInstance> servers_in_metadata{
      {"", "node-A", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "node-B", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
      {"", "node-C", "", ServerMode::Unavailable, 0, 0, "", "", 0, 0},
  };

  // GR reports 3 nodes, of which 3 are alive (have qourum), BUT from
  // the perspective of MD-defined nodes, only 1 of its 3 is alive (no quorum).
  // We choose to be pessimistic (no quorum)
  {
    std::map<std::string, GroupReplicationMember> server_status{
        {"node-C", {"", "", 0, State::Online, Role::Primary}},
        {"node-D", {"", "", 0, State::Online, Role::Secondary}},
        {"node-E", {"", "", 0, State::Online, Role::Secondary}},
    };
    EXPECT_EQ(RS::Unavailable, metadata.check_replicaset_status(
                                   servers_in_metadata, server_status));
    // should log warning "Member <host>:<port> (node-A) defined in metadata not
    // found in actual replicaset" should log warning "Member <host>:<port>
    // (node-B) defined in metadata not found in actual replicaset" should log
    // error "Member <host>:<port> (node-D) found in replicaset, yet is not
    // defined in metadata!" should log error "Member <host>:<port> (node-E)
    // found in replicaset, yet is not defined in metadata!"

    // meeting these is not strictly required, because when the cluster is
    // unavailable, ATTOW these results will be ignored. But OTOH, there's no
    // reason why these computations should fail - so we use the opportunity to
    // check if they still compute correctly despite replicaset being
    // unavailable. If one day we need these results to compute differently,
    // please feel free to erase these tests.
    EXPECT_EQ(3u,
              servers_in_metadata.size());  // new nodes reported by GR will not
                                            // be added, nor old ones removed
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(0).mode);
    EXPECT_EQ(ServerMode::Unavailable, servers_in_metadata.at(1).mode);
    EXPECT_EQ(ServerMode::ReadWrite, servers_in_metadata.at(2).mode);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// test ClusterMetadata::update_replicaset_status() - connection failures
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @test
 * Verify `ClusterMetadata::update_replicaset_status()` will correctly update
 * routing table, even despite having to failover on connection errors.
 *
 *     Scenario details:
 *     iteration 1 (instance-1): query_primary_member FAILS
 *     iteration 2 (instance-2): CAN'T CONNECT
 *     iteration 3 (instance-3): query_primary_member OK, query_status OK
 */
TEST_F(MetadataTest, UpdateReplicasetStatus_PrimaryMember_FailConnectOnNode2) {
  connect_to_first_metadata_server();

  // update_replicaset_status() first iteration: requests start with existing
  // connection to instance-1 (shared with metadata server)
  unsigned session = 0;

  // 1st query_primary_member should go to existing connection (shared with
  // metadata server) -> make the query fail
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_fail(session)));

  // since 1st query_primary_member failed, update_replicaset_status() should
  // try to connect to instance-2. Let's make that new connections fail by NOT
  // using enable_connection(session)
  // enable_connection(++session, 3320); // we don't call this on purpose
  EXPECT_CALL(session_factory.get(++session), flag_fail(_, 3320)).Times(1);

  // Next
  // instance-2. Let's make that new connections fail by NOT using
  // enable_connection(session)
  // enable_connection(++session, 3320); // we don't call this on purpose

  // since 2nd connection failed, update_replicaset_status() should try to
  // connect to instance-3. Let's allow this.
  enable_connection(++session, 3330);

  // 3rd query_primary_member: let's return "instance-1"
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_ok(session)));

  // 3rd query_status: let's return good data
  EXPECT_CALL(session_factory.get(session), query(StartsWith(query_status), _))
      .Times(1)
      .WillOnce(Invoke(query_status_ok(session)));

  EXPECT_EQ(1,
            session_factory
                .create_cnt());  // caused by connect_to_first_metadata_server()

  ManagedReplicaSet replicaset = typical_replicaset;
  metadata.update_replicaset_status("replicaset-1", replicaset);

  EXPECT_EQ(3u, replicaset.members.size());
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-1", "", ServerMode::ReadWrite,
                      0, 0, "", "localhost", 3310, 33100},
      replicaset.members.at(0)));
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-2", "", ServerMode::ReadOnly, 0,
                      0, "", "localhost", 3320, 33200},
      replicaset.members.at(1)));
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-3", "", ServerMode::ReadOnly, 0,
                      0, "", "localhost", 3330, 33300},
      replicaset.members.at(2)));

  EXPECT_EQ(3, session_factory.create_cnt());  // +2 from new connections to
                                               // localhost:3320 and :3330
}

/**
 * @test
 * Verify `ClusterMetadata::update_replicaset_status()` will handle correctly
 * when all connect attempts fail. Finally, it should clear the routing table
 * since it's unable to connect to any server.
 *
 *     Scenario details:
 *     iteration 1 (instance-1): query_primary_member FAILS
 *     iteration 2 (instance-2): CAN'T CONNECT
 *     iteration 3 (instance-3): CAN'T CONNECT
 */
TEST_F(MetadataTest,
       UpdateReplicasetStatus_PrimaryMember_FailConnectOnAllNodes) {
  connect_to_first_metadata_server();

  // update_replicaset_status() first iteration: requests start with existing
  // connection to instance-1 (shared with metadata server)
  unsigned session = 0;

  // 1st query_primary_member should go to existing connection (shared with
  // metadata server) -> make the query fail
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_fail(session)));

  // since 1st query_primary_member failed, update_replicaset_status() should
  // try to connect to instance-2, then instance-3. Let's make those new
  // connections fail by NOT using enable_connection(session)
  EXPECT_CALL(session_factory.get(++session), flag_fail(_, 3320)).Times(1);
  EXPECT_CALL(session_factory.get(++session), flag_fail(_, 3330)).Times(1);

  EXPECT_EQ(1,
            session_factory
                .create_cnt());  // caused by connect_to_first_metadata_server()

  // if update_replicaset_status() can't connect to a quorum, it should clear
  // replicaset.members
  ManagedReplicaSet replicaset = typical_replicaset;
  metadata.update_replicaset_status("replicaset-1", replicaset);
  EXPECT_TRUE(replicaset.members.empty());

  EXPECT_EQ(3, session_factory.create_cnt());  // +2 from new connections to
                                               // localhost:3320 and :3330
}

////////////////////////////////////////////////////////////////////////////////
//
// test ClusterMetadata::update_replicaset_status() - query_primary_member
// failures [QUERY #2: query_primary_member]
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @test
 * Verify `ClusterMetadata::update_replicaset_status()` will correctly update
 * routing table, even despite having to failover on fetching primary member
 * failing.
 *
 *     Scenario details:
 *     iteration 1 (instance-1): query_primary_member FAILS
 *     iteration 2 (instance-2): query_primary_member OK, query_status OK
 */
TEST_F(MetadataTest, UpdateReplicasetStatus_PrimaryMember_FailQueryOnNode1) {
  connect_to_first_metadata_server();

  // update_replicaset_status() first iteration: requests start with existing
  // connection to instance-1 (shared with metadata server)
  unsigned session = 0;

  // 1st query_primary_member should go to existing connection (shared with
  // metadata server) -> make the query fail
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_fail(session)));

  // since 1st query_primary_member failed, 2nd should to to instance-2 -> make
  // it succeed. Note that the connection to instance-2 has to be created first
  enable_connection(++session, 3320);

  // 2nd query_primary_member: let's return "instance-1"
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_ok(session)));

  // 2nd query_status: let's return good data
  EXPECT_CALL(session_factory.get(session), query(StartsWith(query_status), _))
      .Times(1)
      .WillOnce(Invoke(query_status_ok(session)));

  EXPECT_EQ(1,
            session_factory
                .create_cnt());  // caused by connect_to_first_metadata_server()

  ManagedReplicaSet replicaset = typical_replicaset;
  metadata.update_replicaset_status("replicaset-1", replicaset);

  EXPECT_EQ(2, session_factory.create_cnt());  // +1 from new connection to
                                               // localhost:3320 (instance-2)

  // query_status reported back from instance-2
  EXPECT_EQ(3u, replicaset.members.size());
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-1", "", ServerMode::ReadWrite,
                      0, 0, "", "localhost", 3310, 33100},
      replicaset.members.at(0)));
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-2", "", ServerMode::ReadOnly, 0,
                      0, "", "localhost", 3320, 33200},
      replicaset.members.at(1)));
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-3", "", ServerMode::ReadOnly, 0,
                      0, "", "localhost", 3330, 33300},
      replicaset.members.at(2)));
}

/**
 * @test
 * Verify `ClusterMetadata::update_replicaset_status()` will handle correctly
 * when all primary member query attempts fail. Finally, it should clear the
 * routing table since it was unable to complete its operation successfully.
 *
 *     Scenario details:
 *     iteration 1 (instance-1): query_primary_member FAILS
 *     iteration 2 (instance-2): query_primary_member FAILS
 *     iteration 3 (instance-3): query_primary_member FAILS
 */
TEST_F(MetadataTest, UpdateReplicasetStatus_PrimaryMember_FailQueryOnAllNodes) {
  connect_to_first_metadata_server();

  // update_replicaset_status() first iteration: requests start with existing
  // connection to instance-1 (shared with metadata server)
  unsigned session = 0;

  // 1st query_primary_member should go to existing connection (shared with
  // metadata server) -> make the query fail
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_fail(session)));

  // since 1st query_primary_member failed, should issue 2nd query to instance-2
  // -> also make the query fail Note that the connection to instance-2 has to
  // be created first
  enable_connection(++session, 3320);

  // 2nd query_primary_member: let's fail again
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_fail(session)));

  // since 2nd query_primary_member failed, should issue 3rd query to instance-3
  // -> also make the query fail Note that the connection to instance-3 has to
  // be created first
  enable_connection(++session, 3330);

  // 3rd query_primary_member: let's fail again
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_fail(session)));

  EXPECT_EQ(1,
            session_factory
                .create_cnt());  // caused by connect_to_first_metadata_server()

  // if update_replicaset_status() can't connect to a quorum, it should clear
  // replicaset.members
  ManagedReplicaSet replicaset = typical_replicaset;
  metadata.update_replicaset_status("replicaset-1", replicaset);
  EXPECT_TRUE(replicaset.members.empty());

  EXPECT_EQ(3, session_factory.create_cnt());  // +2 from new connections to
                                               // localhost:3320 and :3330
}

////////////////////////////////////////////////////////////////////////////////
//
// test ClusterMetadata::update_replicaset_status() - query_status failures
// [QUERY #3: query_status]
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @test
 * Verify `ClusterMetadata::update_replicaset_status()` will correctly update
 * routing table, even despite having to failover on fetching healh status
 * failing.
 *
 *     Scenario details:
 *     iteration 1 (instance-1): query_primary_member OK, query_status FAILS
 *     iteration 2 (instance-2): query_primary_member OK, query_status OK
 */
TEST_F(MetadataTest, UpdateReplicasetStatus_Status_FailQueryOnNode1) {
  connect_to_first_metadata_server();

  // update_replicaset_status() first iteration: requests start with existing
  // connection to instance-1 (shared with metadata server)
  unsigned session = 0;

  // 1st query_primary_member: let's return "instance-1"
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_ok(session)));

  // 1st query_status: let's fail the query
  EXPECT_CALL(session_factory.get(session), query(StartsWith(query_status), _))
      .Times(1)
      .WillOnce(Invoke(query_status_fail(session)));

  // since 1st query_status failed, update_replicaset_status() should start
  // another iteration, but on instance-2 this time. Note that the connection to
  // instance-2 has to be created first
  enable_connection(++session, 3320);

  // 2nd query_primary_member: let's again return "instance-1"
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_ok(session)));

  // 2nd query_status: let's return good data
  EXPECT_CALL(session_factory.get(session), query(StartsWith(query_status), _))
      .Times(1)
      .WillOnce(Invoke(query_status_ok(session)));

  EXPECT_EQ(1,
            session_factory
                .create_cnt());  // caused by connect_to_first_metadata_server()

  ManagedReplicaSet replicaset = typical_replicaset;
  metadata.update_replicaset_status("replicaset-1", replicaset);

  EXPECT_EQ(2, session_factory.create_cnt());  // +1 from new connection to
                                               // localhost:3320 (instance-2)

  // query_status reported back from instance-1
  EXPECT_EQ(3u, replicaset.members.size());
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-1", "", ServerMode::ReadWrite,
                      0, 0, "", "localhost", 3310, 33100},
      replicaset.members.at(0)));
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-2", "", ServerMode::ReadOnly, 0,
                      0, "", "localhost", 3320, 33200},
      replicaset.members.at(1)));
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-3", "", ServerMode::ReadOnly, 0,
                      0, "", "localhost", 3330, 33300},
      replicaset.members.at(2)));
}

/**
 * @test
 * Verify `ClusterMetadata::update_replicaset_status()` will handle correctly
 * when all health status query attempts fail. Finally, it should clear the
 * routing table since it was unable to complete its operation successfully.
 *
 *     Scenario details:
 *     iteration 1 (instance-1): query_primary_member OK, query_status FAILS
 *     iteration 2 (instance-2): query_primary_member OK, query_status FAILS
 *     iteration 3 (instance-2): query_primary_member OK, query_status FAILS
 */
TEST_F(MetadataTest, UpdateReplicasetStatus_Status_FailQueryOnAllNodes) {
  connect_to_first_metadata_server();

  // update_replicaset_status() first iteration: requests start with existing
  // connection to instance-1 (shared with metadata server)
  unsigned session = 0;

  // 1st query_primary_member: let's return "instance-1"
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_ok(session)));

  // 1st query_status: let's fail the query
  EXPECT_CALL(session_factory.get(session), query(StartsWith(query_status), _))
      .Times(1)
      .WillOnce(Invoke(query_status_fail(session)));

  // since 1st query_status failed, update_replicaset_status() should start
  // another iteration, but on instance-2 this time. Note that the connection to
  // instance-2 has to be created first
  enable_connection(++session, 3320);

  // 2nd query_primary_member: let's again return "instance-1"
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_ok(session)));

  // 2nd query_status: let's fail the query
  EXPECT_CALL(session_factory.get(session), query(StartsWith(query_status), _))
      .Times(1)
      .WillOnce(Invoke(query_status_fail(session)));

  // since 2st query_status failed, update_replicaset_status() should start
  // another iteration, but on instance-3 this time. Note that the connection to
  // instance-3 has to be created first
  enable_connection(++session, 3330);

  // 3rd query_primary_member: let's again return "instance-1"
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_ok(session)));

  // 3rd query_status: let's fail the query
  EXPECT_CALL(session_factory.get(session), query(StartsWith(query_status), _))
      .Times(1)
      .WillOnce(Invoke(query_status_fail(session)));

  EXPECT_EQ(1,
            session_factory
                .create_cnt());  // caused by connect_to_first_metadata_server()

  // if update_replicaset_status() can't connect to a quorum, it should clear
  // replicaset.members
  ManagedReplicaSet replicaset = typical_replicaset;
  metadata.update_replicaset_status("replicaset-1", replicaset);
  EXPECT_TRUE(replicaset.members.empty());

  EXPECT_EQ(3, session_factory.create_cnt());  // +2 from new connections to
                                               // localhost:3320 and :3330
}

////////////////////////////////////////////////////////////////////////////////
//
// test ClusterMetadata::update_replicaset_status() - success scenarios
// [QUERY #2 + #3]
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @test
 * Verify `ClusterMetadata::update_replicaset_status()` will return correct
 * results in a sunny-day scenario.
 *
 *     Scenario details:
 *     iteration 1 (instance-1): query_primary_member OK, query_status OK
 */
TEST_F(MetadataTest, UpdateReplicasetStatus_SimpleSunnyDayScenario) {
  connect_to_first_metadata_server();

  // update_replicaset_status() first iteration: all requests go to existing
  // connection to instance-1 (shared with metadata server)
  unsigned session = 0;

  // 1st query_primary_member: let's return "instance-1"
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_ok(session)));

  // 1st query_status as seen from instance-1
  EXPECT_CALL(session_factory.get(session), query(StartsWith(query_status), _))
      .Times(1)
      .WillOnce(Invoke(query_status_ok(session)));

  EXPECT_EQ(1,
            session_factory
                .create_cnt());  // caused by connect_to_first_metadata_server()

  ManagedReplicaSet replicaset = typical_replicaset;
  metadata.update_replicaset_status("replicaset-1", replicaset);

  EXPECT_EQ(1,
            session_factory
                .create_cnt());  // should resuse localhost:3310 connection,

  // query_status reported back from instance-1
  EXPECT_EQ(3u, replicaset.members.size());
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-1", "", ServerMode::ReadWrite,
                      0, 0, "", "localhost", 3310, 33100},
      replicaset.members.at(0)));
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-2", "", ServerMode::ReadOnly, 0,
                      0, "", "localhost", 3320, 33200},
      replicaset.members.at(1)));
  EXPECT_TRUE(cmp_mi_FI(
      ManagedInstance{"replicaset-1", "instance-3", "", ServerMode::ReadOnly, 0,
                      0, "", "localhost", 3330, 33300},
      replicaset.members.at(2)));
}

////////////////////////////////////////////////////////////////////////////////
//
// test ClusterMetadata::fetch_instances()
// (this is the highest-level function, it calls everything tested above
// except connect() (which is a separate step))
//
// TODO add tests for multiple replicasets here, when we begin supporting them
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @test
 * Verify `ClusterMetadata::fetch_instances()` will return correct results in a
 * sunny-day scenario.
 */
TEST_F(MetadataTest, FetchInstances_1Replicaset_ok) {
  connect_to_first_metadata_server();

  // update_replicaset_status() first iteration: all requests go to existing
  // connection to instance-1 (shared with metadata server)
  unsigned session = 0;

  EXPECT_CALL(session_factory.get(session),
              query_one(StartsWith(query_schema_version)))
      .Times(1)
      .WillOnce(Return(new MockRow({"1", "0", "1"})));

  auto resultset_metadata =
      [this](const std::string &, const MySQLSession::RowProcessor &processor) {
        session_factory.get(0).query_impl(
            processor, {
                           {"replicaset-1", "instance-1", "HA", NULL, NULL,
                            "blabla", "localhost:3310", NULL},
                           {"replicaset-1", "instance-2", "HA", NULL, NULL,
                            "blabla", "localhost:3320", NULL},
                           {"replicaset-1", "instance-3", "HA", NULL, NULL,
                            "blabla", "localhost:3330", NULL},
                       });
      };
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_metadata), _))
      .Times(1)
      .WillOnce(Invoke(resultset_metadata));
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_ok(session)));
  EXPECT_CALL(session_factory.get(session), query(StartsWith(query_status), _))
      .Times(1)
      .WillOnce(Invoke(query_status_ok(session)));

  ASSERT_NO_THROW({
    ClusterMetadata::ReplicaSetsByName rs =
        metadata.fetch_instances("replicaset-1");

    EXPECT_EQ(1u, rs.size());
    EXPECT_EQ(3u, rs.at("replicaset-1").members.size());
    EXPECT_TRUE(cmp_mi_FI(
        ManagedInstance{"replicaset-1", "instance-1", "", ServerMode::ReadWrite,
                        0, 0, "", "localhost", 3310, 33100},
        rs.at("replicaset-1").members.at(0)));
    EXPECT_TRUE(cmp_mi_FI(
        ManagedInstance{"replicaset-1", "instance-2", "", ServerMode::ReadOnly,
                        0, 0, "", "localhost", 3320, 33200},
        rs.at("replicaset-1").members.at(1)));
    EXPECT_TRUE(cmp_mi_FI(
        ManagedInstance{"replicaset-1", "instance-3", "", ServerMode::ReadOnly,
                        0, 0, "", "localhost", 3330, 33300},
        rs.at("replicaset-1").members.at(2)));
  });
}

/**
 * @test
 * Verify `ClusterMetadata::fetch_instances()` will handle correctly when
 * retreiving information from all servers fail. It should return an empty
 * routing table since it's unable to complete its operation successfully.
 */
TEST_F(MetadataTest, FetchInstances_1Replicaset_fail) {
  connect_to_first_metadata_server();
  // update_replicaset_status() first iteration: requests start with existing
  // connection to instance-1 (shared with metadata server)
  unsigned session = 0;

  EXPECT_CALL(session_factory.get(session),
              query_one(StartsWith(query_schema_version)))
      .Times(1)
      .WillOnce(Return(new MockRow({"1", "0", "1"})));

  auto resultset_metadata =
      [this](const std::string &, const MySQLSession::RowProcessor &processor) {
        session_factory.get(0).query_impl(
            processor, {
                           {"replicaset-1", "instance-1", "HA", NULL, NULL,
                            "blabla", "localhost:3310", NULL},
                           {"replicaset-1", "instance-2", "HA", NULL, NULL,
                            "blabla", "localhost:3320", NULL},
                           {"replicaset-1", "instance-3", "HA", NULL, NULL,
                            "blabla", "localhost:3330", NULL},
                       });
      };
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_metadata), _))
      .Times(1)
      .WillOnce(Invoke(resultset_metadata));

  // fail query_primary_member, then further connections
  EXPECT_CALL(session_factory.get(session),
              query(StartsWith(query_primary_member), _))
      .Times(1)
      .WillOnce(Invoke(query_primary_member_fail(session)));
  EXPECT_CALL(session_factory.get(++session), flag_fail(_, 3320)).Times(1);
  EXPECT_CALL(session_factory.get(++session), flag_fail(_, 3330)).Times(1);

  // if fetch_instances() can't connect to a quorum for a particular replicaset,
  // it should clear its replicaset.members
  ASSERT_NO_THROW({
    ClusterMetadata::ReplicaSetsByName rs =
        metadata.fetch_instances("replicaset-1");
    EXPECT_EQ(1u, rs.size());
    EXPECT_EQ(0u, rs.at("replicaset-1").members.size());
  });
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
