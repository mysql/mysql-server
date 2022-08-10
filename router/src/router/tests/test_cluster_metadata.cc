/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include "cluster_metadata.h"

#include <cstring>
#include <stdexcept>

#include <gmock/gmock.h>

#include "mysql/harness/stdx/expected.h"
#include "mysql_session_replayer.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils.h"
#include "router_test_helpers.h"
#include "test/helpers.h"

using ::testing::Return;
using namespace testing;
using mysqlrouter::ClusterMetadataGRV2;

class MockSocketOperations : public mysql_harness::SocketOperationsBase {
 public:
  // this is what we test
  MOCK_METHOD0(get_local_hostname, std::string());
};

class ClusterMetadataTest : public ::testing::Test {
 protected:
  MySQLSessionReplayer session_replayer;
  ::testing::StrictMock<MockSocketOperations> hostname_operations;
};

const std::string kQueryGetHostname =
    "SELECT address FROM mysql_innodb_cluster_metadata.v2_routers WHERE "
    "router_id =";

const std::string kRegisterRouter =
    "INSERT INTO mysql_innodb_cluster_metadata.v2_routers"
    "        (address, product_name, router_name) VALUES";

const mysqlrouter::MetadataSchemaVersion kNewSchemaVersion{1, 0, 1};

TEST_F(ClusterMetadataTest, check_router_id_ok) {
  const std::string kHostname = "hostname";
  ClusterMetadataGRV2 cluster_metadata(kNewSchemaVersion, &session_replayer,
                                       &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname)
      .then_return(1, {{kHostname.c_str()}});
  EXPECT_CALL(hostname_operations, get_local_hostname())
      .Times(1)
      .WillOnce(Return(kHostname));

  EXPECT_NO_THROW(cluster_metadata.verify_router_id_is_ours(1));
}

ACTION_P(ThrowLocalHostnameResolutionError, msg) {
  throw mysql_harness::SocketOperationsBase::LocalHostnameResolutionError(msg);
}

/**
 * @test verify that verify_router_id_is_ours() will throw if
 * get_local_hostname() fails
 */
TEST_F(ClusterMetadataTest, check_router_id_get_hostname_throws) {
  const std::string kHostname = "";
  ClusterMetadataGRV2 cluster_metadata(kNewSchemaVersion, &session_replayer,
                                       &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname)
      .then_return(1, {{kHostname.c_str()}});
  EXPECT_CALL(hostname_operations, get_local_hostname())
      .Times(1)
      .WillOnce(ThrowLocalHostnameResolutionError(
          "some error from get_local_hostname()"));

  EXPECT_THROW_LIKE(
      cluster_metadata.verify_router_id_is_ours(1),
      mysql_harness::SocketOperationsBase::LocalHostnameResolutionError,
      "some error from get_local_hostname()");
}

TEST_F(ClusterMetadataTest, check_router_id_router_not_found) {
  ClusterMetadataGRV2 cluster_metadata(kNewSchemaVersion, &session_replayer,
                                       &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname).then_return(2, {});

  try {
    cluster_metadata.verify_router_id_is_ours(1);
    FAIL() << "Expected exception";
  } catch (std::runtime_error &e) {
    ASSERT_STREQ("router_id 1 not found in metadata", e.what());
  }
}

TEST_F(ClusterMetadataTest, check_router_id_different_hostname) {
  const std::string kHostname1 = "hostname";
  const std::string kHostname2 = "another.hostname";
  ClusterMetadataGRV2 cluster_metadata(kNewSchemaVersion, &session_replayer,
                                       &hostname_operations);

  session_replayer.expect_query_one(kQueryGetHostname)
      .then_return(1, {{kHostname1.c_str()}});
  EXPECT_CALL(hostname_operations, get_local_hostname())
      .Times(1)
      .WillOnce(Return(kHostname2));

  try {
    cluster_metadata.verify_router_id_is_ours(1);
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
  ClusterMetadataGRV2 cluster_metadata(kNewSchemaVersion, &session_replayer,
                                       &hostname_operations);

  session_replayer.expect_execute(kRegisterRouter).then_ok();
  EXPECT_CALL(hostname_operations, get_local_hostname())
      .Times(1)
      .WillOnce(Return(kHostName.c_str()));

  EXPECT_NO_THROW(cluster_metadata.register_router(kRouterName, false));
}

/**
 * @test verify that register_router() will throw if get_local_hostname()
 fails
 */
TEST_F(ClusterMetadataTest, register_router_get_hostname_throws) {
  const std::string kRouterName = "routername";
  const std::string kHostName = "";
  ClusterMetadataGRV2 cluster_metadata(kNewSchemaVersion, &session_replayer,
                                       &hostname_operations);

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
