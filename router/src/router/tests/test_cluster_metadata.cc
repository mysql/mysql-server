/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysqlrouter/utils.h"
#include "router_test_helpers.h"
#include "test/helpers.h"

#include <cstring>
#include <stdexcept>

// ignore GMock warnings
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif

#include "gmock/gmock.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "cluster_metadata.h"
#include "mysql_session_replayer.h"
#include "mysqlrouter/mysql_session.h"

using ::testing::Return;
using namespace testing;
using mysqlrouter::MySQLInnoDBClusterMetadata;

class MockSocketOperations : public mysql_harness::SocketOperationsBase {
 public:
  // this is what we test
  MOCK_METHOD0(get_local_hostname, std::string());

  // we don't call these, but we need to provide an implementation (they're pure
  // virtual)
  MOCK_METHOD3(read, ssize_t(int, void *, size_t));
  MOCK_METHOD3(write, ssize_t(int, void *, size_t));
  MOCK_METHOD1(close, void(int));
  MOCK_METHOD1(shutdown, void(int));
  MOCK_METHOD1(freeaddrinfo, void(addrinfo *ai));
  MOCK_METHOD4(getaddrinfo,
               int(const char *, const char *, const addrinfo *, addrinfo **));
  MOCK_METHOD3(bind, int(int, const struct sockaddr *, socklen_t));
  MOCK_METHOD3(socket, int(int, int, int));
  MOCK_METHOD5(setsockopt, int(int, int, int, const void *, socklen_t));
  MOCK_METHOD2(listen, int(int fd, int n));
  MOCK_METHOD3(poll, int(struct pollfd *, nfds_t, std::chrono::milliseconds));
  MOCK_METHOD4(inetntop, const char *(int af, void *, char *, socklen_t));
  MOCK_METHOD3(getpeername, int(int, struct sockaddr *, socklen_t *));
  MOCK_METHOD2(connect_non_blocking_wait,
               int(socket_t sock, std::chrono::milliseconds timeout));
  MOCK_METHOD2(connect_non_blocking_status, int(int sock, int &so_error));
  MOCK_METHOD1(set_errno, void(int err));
  MOCK_METHOD0(get_errno, int());
};

class ClusterMetadataTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}

  MySQLSessionReplayer session_replayer;
  MockSocketOperations hostname_operations;
};

const std::string kQueryGetHostname =
    "SELECT h.host_id, h.host_name"
    " FROM mysql_innodb_cluster_metadata.routers r"
    " JOIN mysql_innodb_cluster_metadata.hosts h"
    "    ON r.host_id = h.host_id"
    " WHERE r.router_id =";

const std::string kCheckHostExists =
    "SELECT host_id, host_name, ip_address"
    " FROM mysql_innodb_cluster_metadata.hosts"
    " WHERE host_name =";

const std::string kRegisterRouter =
    "INSERT INTO mysql_innodb_cluster_metadata.routers"
    "        (host_id, router_name) VALUES";

TEST_F(ClusterMetadataTest, check_router_id_ok) {
  const std::string kHostId = "2";
  const std::string kHostname = "hostname";
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer,
                                              &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname)
      .then_return(2, {{kHostId.c_str(), kHostname.c_str()}});
  EXPECT_CALL(hostname_operations, get_local_hostname())
      .Times(1)
      .WillOnce(Return(kHostname));

  EXPECT_NO_THROW(cluster_metadata.check_router_id(1));
}

ACTION_P(ThrowLocalHostnameResolutionError, msg) {
  throw mysql_harness::SocketOperationsBase::LocalHostnameResolutionError(msg);
}

/**
 * @test verify that check_router_id() will throw if get_local_hostname() fails
 */
TEST_F(ClusterMetadataTest, check_router_id_get_hostname_throws) {
  const std::string kHostId = "2";
  const std::string kHostname = "";
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer,
                                              &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname)
      .then_return(2, {{kHostId.c_str(), kHostname.c_str()}});
  EXPECT_CALL(hostname_operations, get_local_hostname())
      .Times(1)
      .WillOnce(ThrowLocalHostnameResolutionError(
          "some error from get_local_hostname()"));

  EXPECT_THROW_LIKE(
      cluster_metadata.check_router_id(1),
      mysql_harness::SocketOperationsBase::LocalHostnameResolutionError,
      "some error from get_local_hostname()");
}

TEST_F(ClusterMetadataTest, check_router_id_router_not_found) {
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer,
                                              &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname).then_return(2, {});

  try {
    cluster_metadata.check_router_id(1);
    FAIL() << "Expected exception";
  } catch (std::runtime_error &e) {
    ASSERT_STREQ("router_id 1 not found in metadata", e.what());
  }
}

TEST_F(ClusterMetadataTest, check_router_id_different_hostname) {
  const std::string kHostId = "2";
  const std::string kHostname1 = "hostname";
  const std::string kHostname2 = "another.hostname";
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer,
                                              &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname)
      .then_return(2, {{kHostId.c_str(), kHostname1.c_str()}});
  EXPECT_CALL(hostname_operations, get_local_hostname())
      .Times(1)
      .WillOnce(Return(kHostname2));

  try {
    cluster_metadata.check_router_id(1);
    FAIL() << "Expected exception";
  } catch (std::runtime_error &e) {
    ASSERT_STREQ(
        "router_id 1 is associated with a different host ('hostname' vs "
        "'another.hostname')",
        e.what());
  }
}

TEST_F(ClusterMetadataTest, register_router_ok) {
  const std::string kRouterName = "routername";
  const std::string kHostName = "hostname";
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer,
                                              &hostname_operations);

  session_replayer.expect_query_one(kCheckHostExists)
      .then_return(3, {{"1", kHostName.c_str(), "127.0.0.1"}});
  session_replayer.expect_execute(kRegisterRouter).then_ok();
  EXPECT_CALL(hostname_operations, get_local_hostname())
      .Times(1)
      .WillOnce(Return(kHostName.c_str()));

  EXPECT_NO_THROW(cluster_metadata.register_router(kRouterName, false));
}

/**
 * @test verify that register_router() will throw if get_local_hostname() fails
 */
TEST_F(ClusterMetadataTest, register_router_get_hostname_throws) {
  const std::string kRouterName = "routername";
  const std::string kHostName = "";
  MySQLInnoDBClusterMetadata cluster_metadata(&session_replayer,
                                              &hostname_operations);

  session_replayer.expect_query_one(kCheckHostExists)
      .then_return(3, {{"1", kHostName.c_str(), "127.0.0.1"}});
  session_replayer.expect_execute(kRegisterRouter).then_ok();
  EXPECT_CALL(hostname_operations, get_local_hostname())
      .Times(1)
      .WillOnce(ThrowLocalHostnameResolutionError(
          "some error from get_local_hostname()"));

  // get_local_hostname() throwing should be handled inside register_router
  EXPECT_THROW_LIKE(
      cluster_metadata.register_router(kRouterName, false),
      mysql_harness::SocketOperationsBase::LocalHostnameResolutionError,
      "some error from get_local_hostname()");
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
