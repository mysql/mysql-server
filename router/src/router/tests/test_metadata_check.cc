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

#include <cstring>
#include <sstream>
#include <stdexcept>

#include <gmock/gmock.h>

#include "cluster_metadata.h"
#include "mysql_session_replayer.h"
#include "mysqlrouter/mysql_session.h"
#include "router_test_helpers.h"

using ::testing::Return;
using namespace testing;
using namespace std::string_literals;

static MySQLSessionReplayer &q_schema_version(MySQLSessionReplayer &m) {
  m.expect_query_one(
      "SELECT * FROM mysql_innodb_cluster_metadata.schema_version");
  return m;
}

static MySQLSessionReplayer &q_schema_version(MySQLSessionReplayer &m,
                                              const char *major,
                                              const char *minor,
                                              const char *patch = nullptr) {
  m.expect_query_one(
      "SELECT * FROM mysql_innodb_cluster_metadata.schema_version");
  if (!patch)
    m.then_return(2, {{m.string_or_null(major), m.string_or_null(minor)}});
  else
    m.then_return(3, {{m.string_or_null(major), m.string_or_null(minor),
                       m.string_or_null(patch)}});
  return m;
}

static MySQLSessionReplayer &q_cluster_type(MySQLSessionReplayer &m) {
  m.expect_query_one(
      "select cluster_type from "
      "mysql_innodb_cluster_metadata.v2_this_instance");
  m.then_return(1, {{m.string_or_null("gr")}});
  return m;
}

static MySQLSessionReplayer &q_metadata_has_one_cluster(
    MySQLSessionReplayer &m) {
  m.expect_query_one(
      "select count(*) from "
      "mysql_innodb_cluster_metadata.v2_gr_clusters");
  return m;
}

static MySQLSessionReplayer &q_metadata_has_one_cluster(
    MySQLSessionReplayer &m, const char *single_cluster) {
  m.expect_query_one(
      "select count(*) from "
      "mysql_innodb_cluster_metadata.v2_gr_clusters");
  m.then_return(1, {{m.string_or_null(single_cluster)}});
  return m;
}

static MySQLSessionReplayer &q_member_state(MySQLSessionReplayer &m) {
  m.expect_query_one(
      "SELECT member_state FROM performance_schema.replication_group_members "
      "WHERE CAST(member_id AS char ascii) = CAST(@@server_uuid AS char "
      "ascii)");
  return m;
}

static MySQLSessionReplayer &q_member_state(MySQLSessionReplayer &m,
                                            const char *state) {
  m.expect_query_one(
      "SELECT member_state FROM performance_schema.replication_group_members "
      "WHERE CAST(member_id AS char ascii) = CAST(@@server_uuid AS char "
      "ascii)");
  m.then_return(1, {{m.string_or_null(state)}});
  return m;
}

static MySQLSessionReplayer &q_quorum(MySQLSessionReplayer &m) {
  m.expect_query_one(
      "SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, COUNT(*) "
      "as num_total FROM performance_schema.replication_group_members");
  return m;
}

static MySQLSessionReplayer &q_quorum(MySQLSessionReplayer &m,
                                      const char *num_onlines,
                                      const char *num_total) {
  m.expect_query_one(
      "SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, COUNT(*) "
      "as num_total FROM performance_schema.replication_group_members");
  m.then_return(2,
                {{m.string_or_null(num_onlines), m.string_or_null(num_total)}});
  return m;
}

class MetadataSchemaError : public ::testing::Test,
                            public ::testing::WithParamInterface<int> {};

const mysqlrouter::MetadataSchemaVersion kNewSchemaVersion{2, 0, 3};

TEST_P(MetadataSchemaError, query_fails) {
  MySQLSessionReplayer m;

  q_schema_version(m).then_error("error", GetParam());  // unknown database
  ASSERT_THROW_LIKE(mysqlrouter::get_metadata_schema_version(&m),
                    std::runtime_error,
                    "to contain the metadata of MySQL InnoDB Cluster");
}

INSTANTIATE_TEST_SUITE_P(Quorum, MetadataSchemaError,
                         ::testing::Values(1049, 1146));

class MetadataSchemaVersionError
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<const char *, const char *, const char *>> {};

TEST_P(MetadataSchemaVersionError, version) {
  MySQLSessionReplayer m;

  q_schema_version(m, std::get<0>(GetParam()), std::get<1>(GetParam()),
                   std::get<2>(GetParam()));
  const auto version = mysqlrouter::get_metadata_schema_version(&m);
  ASSERT_THROW_LIKE(
      {
        std::unique_ptr<mysqlrouter::ClusterMetadata> metadata(
            mysqlrouter::create_metadata(version, &m));
      },
      std::runtime_error,
      "This version of MySQL Router is not compatible with the "
      "provided MySQL InnoDB cluster metadata");
}

INSTANTIATE_TEST_SUITE_P(Quorum, MetadataSchemaVersionError,
                         ::testing::Values(
                             // too old
                             std::make_tuple("0", "0", "1"),

                             // too new
                             std::make_tuple("3", "0", "0")));

class MetadataGroupMembers_2_0_Throws
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<const char *>> {};

// check that the server we're querying contains metadata for the group it's in
//   (metadata server group must be same as managed group currently)
TEST_P(MetadataGroupMembers_2_0_Throws, metadata_unsupported_1_0) {
  MySQLSessionReplayer m;
  q_cluster_type(m);
  std::unique_ptr<mysqlrouter::ClusterMetadata> metadata(
      mysqlrouter::create_metadata(kNewSchemaVersion, &m));

  const std::string clusters_count = std::get<0>(GetParam());
  q_metadata_has_one_cluster(m, clusters_count.c_str());
  if (clusters_count == "0") {
    ASSERT_THROW_LIKE(
        metadata->require_metadata_is_ok(), std::runtime_error,
        "Expected the metadata server to contain configuration for "
        "one cluster, found none");
  } else {
    ASSERT_THROW_LIKE(
        metadata->require_metadata_is_ok(), std::runtime_error,
        "The metadata server contains configuration for more than 1 Cluster: "s +
            clusters_count +
            ". If it was a part of a ClusterSet previously, the metadata "
            "should be recreated using dba.dropMetadataSchema() and "
            "dba.createCluster() with adoptFromGR parameter set to true");
  }
}

TEST_P(MetadataGroupMembers_2_0_Throws, metadata_unsupported_2_0_3) {
  MySQLSessionReplayer m;
  q_schema_version(m, "2", "0", "3");
  q_cluster_type(m);
  const std::string clusters_count = std::get<0>(GetParam());
  q_metadata_has_one_cluster(m, clusters_count.c_str());
  const auto version = mysqlrouter::get_metadata_schema_version(&m);
  std::unique_ptr<mysqlrouter::ClusterMetadata> metadata(
      mysqlrouter::create_metadata(version, &m));
  if (clusters_count == "0") {
    ASSERT_THROW_LIKE(
        metadata->require_metadata_is_ok(), std::runtime_error,
        "Expected the metadata server to contain configuration for "
        "one cluster, found none");
  } else {
    ASSERT_THROW_LIKE(
        metadata->require_metadata_is_ok(), std::runtime_error,
        "The metadata server contains configuration for more than 1 Cluster: "s +
            clusters_count +
            ". If it was a part of a ClusterSet previously, the metadata "
            "should be recreated using dba.dropMetadataSchema() and "
            "dba.createCluster() with adoptFromGR parameter set to true");
  }
}

INSTANTIATE_TEST_SUITE_P(Quorum, MetadataGroupMembers_2_0_Throws,
                         ::testing::Values(std::make_tuple("2"),
                                           std::make_tuple("0")));

class MetadataGroupMembers_2_0_3_Throws
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<const char *, const char *>> {};

// check that the server we're bootstrapping from has GR enabled
class MetadataMemberStateThrows
    : public ::testing::Test,
      public ::testing::WithParamInterface<const char *> {};

TEST_P(MetadataMemberStateThrows, quorum_but_bad_memberstate) {
  MySQLSessionReplayer m;

  q_schema_version(m, "2", "0", "3");
  q_cluster_type(m);
  q_metadata_has_one_cluster(m, "1");

  const auto version = mysqlrouter::get_metadata_schema_version(&m);
  std::unique_ptr<mysqlrouter::ClusterMetadata> metadata(
      mysqlrouter::create_metadata(version, &m));

  ASSERT_NO_THROW(metadata->require_metadata_is_ok());

  q_member_state(m, GetParam());

  ASSERT_THROW_LIKE(metadata->require_cluster_is_ok(), std::runtime_error,
                    "The provided server is currently not an ONLINE member of "
                    "a InnoDB cluster.");
}

INSTANTIATE_TEST_SUITE_P(Quorum, MetadataMemberStateThrows,
                         ::testing::Values("OFFLINE", "RECOVERING"));

class MetadataAccessDeniedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<unsigned int> {};

TEST_P(MetadataAccessDeniedTest, missing_permissions_throws) {
  MySQLSessionReplayer m;
  size_t failed_stmt = GetParam();

  constexpr auto kAccessDeniedCode = 1044;
  constexpr const char kAccessDeniedMsg[] =
      "Access denied for user 'native'@'%' to database "
      "'mysql_innodb_cluster_metadata'";

  // regex version kAccessDeniedMsg, to make exception message against
  constexpr const char kAccessDeniedMsgRegex[] =
      "Access denied for user 'native'@'%' to database "
      "'mysql_innodb_cluster_metadata'";

  // prepare the stmts up to the failing one

  if (failed_stmt > 0) {
    q_schema_version(m, "2", "0", "3");
  } else if (failed_stmt == 0) {
    q_schema_version(m).then_error(kAccessDeniedMsg, kAccessDeniedCode);
  }

  q_cluster_type(m);

  if (failed_stmt > 1) {
    q_metadata_has_one_cluster(m, "1");
  } else if (failed_stmt == 1) {
    q_metadata_has_one_cluster(m).then_error(kAccessDeniedMsg,
                                             kAccessDeniedCode);
  }

  mysqlrouter::MetadataSchemaVersion version;
  std::unique_ptr<mysqlrouter::ClusterMetadata> metadata;

  if (failed_stmt > 1) {
    ASSERT_NO_THROW({
      version = mysqlrouter::get_metadata_schema_version(&m);
      metadata = mysqlrouter::create_metadata(version, &m);
      metadata->require_metadata_is_ok();
    });
  } else {
    ASSERT_THROW_LIKE(
        {
          version = mysqlrouter::get_metadata_schema_version(&m);
          metadata = mysqlrouter::create_metadata(version, &m);
          metadata->require_metadata_is_ok();
        },
        std::runtime_error, kAccessDeniedMsgRegex);

    // we failed early, no further tests
    return;
  }

  if (failed_stmt > 2) {
    q_member_state(m, "ONLINE");
  } else if (failed_stmt == 2) {
    q_member_state(m).then_error(kAccessDeniedMsg, kAccessDeniedCode);
  }

  if (failed_stmt > 3) {
    q_quorum(m, "1", "1");
  } else if (failed_stmt == 3) {
    q_quorum(m).then_error(kAccessDeniedMsg, kAccessDeniedCode);
  }

  if (failed_stmt > 3) {
    ASSERT_NO_THROW(metadata->require_cluster_is_ok());
  } else {
    ASSERT_THROW_LIKE(metadata->require_cluster_is_ok(), std::runtime_error,
                      kAccessDeniedMsgRegex);
  }
}

INSTANTIATE_TEST_SUITE_P(Failure, MetadataAccessDeniedTest,
                         ::testing::Values(0, 1, 2, 3, 4));

class MetadataQuorumThrowsTest : public ::testing::Test,
                                 public ::testing::WithParamInterface<
                                     std::tuple<const char *, const char *>> {};

/**
 * ensure missing Quorum throws an exceptions
 */
TEST_P(MetadataQuorumThrowsTest, metadata_no_quorum_throws) {
  MySQLSessionReplayer m;

  q_schema_version(m, "2", "0", "3");
  q_cluster_type(m);
  q_metadata_has_one_cluster(m, "1");
  const auto version = mysqlrouter::get_metadata_schema_version(&m);
  std::unique_ptr<mysqlrouter::ClusterMetadata> metadata(
      mysqlrouter::create_metadata(version, &m));
  ASSERT_NO_THROW(metadata->require_metadata_is_ok());

  q_member_state(m, "ONLINE");
  q_quorum(m, std::get<0>(GetParam()), std::get<1>(GetParam()));
  ASSERT_THROW_LIKE(
      metadata->require_cluster_is_ok(), std::runtime_error,
      "The provided server is currently not in a InnoDB cluster group with "
      "quorum and thus may contain inaccurate or outdated data.");
}

INSTANTIATE_TEST_SUITE_P(Quorum, MetadataQuorumThrowsTest,
                         ::testing::Values(std::make_tuple("1", "3"),
                                           std::make_tuple("0", "1"),
                                           std::make_tuple("1", "2"),
                                           std::make_tuple("2", "5")));

class MetadataQuorumOkTest : public ::testing::Test,
                             public ::testing::WithParamInterface<
                                 std::tuple<const char *, const char *>> {};

/**
 * ensure Quorum is detected as "ok"
 */
TEST_P(MetadataQuorumOkTest, metadata_has_quorum_ok) {
  MySQLSessionReplayer m;

  q_schema_version(m, "2", "0", "3");
  q_cluster_type(m);
  q_metadata_has_one_cluster(m, "1");
  const auto version = mysqlrouter::get_metadata_schema_version(&m);
  std::unique_ptr<mysqlrouter::ClusterMetadata> metadata(
      mysqlrouter::create_metadata(version, &m));
  ASSERT_NO_THROW(metadata->require_metadata_is_ok());

  q_member_state(m, "ONLINE");
  q_quorum(m, std::get<0>(GetParam()), std::get<1>(GetParam()));
  ASSERT_NO_THROW(metadata->require_cluster_is_ok());
}

INSTANTIATE_TEST_SUITE_P(Quorum, MetadataQuorumOkTest,
                         ::testing::Values(std::make_tuple("1", "1"),
                                           std::make_tuple("2", "3"),
                                           std::make_tuple("3", "3"),
                                           std::make_tuple("3", "5"),
                                           std::make_tuple("2", "2")));

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
