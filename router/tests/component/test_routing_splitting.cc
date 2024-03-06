/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <charconv>
#include <chrono>
#include <fstream>
#include <span>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define RAPIDJSON_HAS_STDSTRING 1

#include "mock_server_testutils.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/ranges.h"
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::SizeIs;

using clock_type = std::chrono::steady_clock;

using namespace std::chrono_literals;

// idle time for connections in the pool.
static constexpr const std::chrono::seconds kIdleTimeout(1);

static constexpr const std::string_view kErRouterTrace("4600");

std::ostream &operator<<(std::ostream &os, MysqlError e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
}

// convert a string to a number
static stdx::expected<uint64_t, std::error_code> from_string(
    std::string_view sv) {
  uint64_t num;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), num);

  if (ec != std::errc{}) return stdx::unexpected(make_error_code(ec));

  return num;
}

/**
 * convert a multi-resultset into a simple container which can be EXPECTed
 * against.
 */
static std::vector<std::vector<std::vector<std::string>>> result_as_vector(
    const MysqlClient::Statement::Result &results) {
  std::vector<std::vector<std::vector<std::string>>> resultsets;

  for (const auto &result : results) {
    std::vector<std::vector<std::string>> res_;

    const auto field_count = result.field_count();

    for (const auto &row : result.rows()) {
      std::vector<std::string> row_;

      for (unsigned int ndx = 0; ndx < field_count; ++ndx) {
        auto fld = row[ndx];

        row_.emplace_back(fld == nullptr ? "<NULL>" : fld);
      }

      res_.push_back(std::move(row_));
    }
    resultsets.push_back(std::move(res_));
  }

  return resultsets;
}

static stdx::expected<std::vector<std::vector<std::string>>, MysqlError>
query_one_result(MysqlClient &cli, std::string_view stmt) {
  auto cmd_res = cli.query(stmt);
  if (!cmd_res) return stdx::unexpected(cmd_res.error());

  auto results = result_as_vector(*cmd_res);
  if (results.size() != 1) {
    return stdx::unexpected(MysqlError{1, "Too many results", "HY000"});
  }

  return results.front();
}

static stdx::expected<std::vector<std::vector<std::string>>, MysqlError>
query_one_result(MysqlClient &cli, std::string_view stmt,
                 std::span<MYSQL_BIND> params, std::span<const char *> names) {
  auto cmd_res = cli.query(stmt, params, names);
  if (!cmd_res) return stdx::unexpected(cmd_res.error());

  auto results = result_as_vector(*cmd_res);
  if (results.size() != 1) {
    return stdx::unexpected(MysqlError{1, "Too many results", "HY000"});
  }

  return results.front();
}

static std::string create_state_file_content(
    const std::string &cluster_id,
    const std::vector<uint16_t> &metadata_servers_ports,
    const std::string &hostname = "127.0.0.1") {
  std::string metadata_servers;
  for (std::size_t i = 0; i < metadata_servers_ports.size(); i++) {
    metadata_servers += R"("mysql://)" + hostname + ":" +
                        std::to_string(metadata_servers_ports[i]) + "\"";
    if (i < metadata_servers_ports.size() - 1) metadata_servers += ",";
  }
  // clang-format off
  std::string result =
    "{"
       R"("version": "1.0.0",)"
       R"("metadata-cache": {)"
         R"("group-replication-id": ")" + cluster_id + R"(",)"
         R"("cluster-metadata-servers": [)" + metadata_servers + "]"
        "}"
      "}";
  // clang-format on

  return result;
}

class RoutingSplittingTestBase : public RouterComponentTest {
 public:
  struct Node {
    uint16_t classic_port;
    uint16_t x_port;
    uint16_t http_port;
    ProcessWrapper *proc;
  };

  std::string cluster_id = "3a0be5af-0022-11e8-9655-0800279e6a88";

  static void SetUpTestSuite() {
    schema_doc_ = std::make_unique<rapidjson::Document>();
    schema_doc_->Parse(schema_json.data(), schema_json.size());
    ASSERT_FALSE(schema_doc_->HasParseError())
        << rapidjson::GetParseError_En(schema_doc_->GetParseError()) << " at "
        << schema_doc_->GetErrorOffset() << " near\n"
        << schema_json.substr(schema_doc_->GetErrorOffset());
  }

  static void TearDownTestSuite() { schema_doc_.reset(); }

  stdx::expected<void, std::error_code> shutdown_server(uint16_t port) {
    for (auto &node : nodes_) {
      if (node.classic_port == port) {
        node.proc->kill();
        node.proc->wait_for_exit();
        return {};
      }
    }

    return stdx::unexpected(make_error_code(std::errc::no_such_process));
  }

  void start_mock_cluster() {
    std::vector<GRNode> gr_nodes;
    std::vector<ClusterNode> cluster_nodes;

    SCOPED_TRACE("// start mock-server");
    std::vector<uint16_t> classic_ports;
    for (auto &node : nodes_) {
      const auto classic_port = port_pool_.get_next_available();
      node.classic_port = classic_port;
      classic_ports.push_back(classic_port);
      node.x_port = port_pool_.get_next_available();
      node.http_port = port_pool_.get_next_available();

      node.proc = &ProcessManager::launch_mysql_server_mock(
          get_data_dir().join("splitting.js").str(), node.classic_port,
          EXIT_SUCCESS, false, node.http_port, node.x_port,
          "",  // module-prefix
          "127.0.0.1",
          30s,  // wait notify.
          true  // enable-ssl
      );
    }

    gr_nodes = classic_ports_to_gr_nodes(classic_ports);
    cluster_nodes = classic_ports_to_cluster_nodes(classic_ports);

    SCOPED_TRACE("// configure mock-servers");
    for (auto [ndx, node] : stdx::views::enumerate(nodes_)) {
      SCOPED_TRACE(
          "// Make our metadata server to return single node as a cluster "
          "member (meaning single metadata server)");
      set_mock_metadata(node.http_port,
                        cluster_id,     // gr-id
                        gr_nodes,       // gr-nodes
                        ndx,            // gr-pos
                        cluster_nodes,  // cluster-nodes
                        0,              // view-id
                        false,          // error-on-md-query
                        "localhost"     // gr-node-host
      );
    }
  }

  void start_router(
      const std::vector<std::pair<std::string, std::string>> &extra_options) {
    SCOPED_TRACE("// start router");
    auto writer = config_writer(conf_dir_.name());

    auto &default_section = writer.sections()["DEFAULT"];
    init_keyring(default_section, conf_dir_.name());
    default_section["dynamic_state"] = create_state_file(
        conf_dir_.name(),
        create_state_file_content(cluster_id, {
                                                  nodes_[0].classic_port,
                                                  nodes_[1].classic_port,
                                                  nodes_[2].classic_port,
                                              }));

    writer
        .section("connection_pool",
                 {
                     {"max_idle_server_connections", "64"},
                     {"idle_timeout", std::to_string(kIdleTimeout.count())},
                 })
        .section("metadata_cache",
                 {
                     {"cluster_type", "gr"},
                     {"router_id", "1"},
                     {"user", "mysql_router1_user"},
                     {"metadata_cluster", "main_cluster"},
                 })
        .section(
            "routing:under_test",
            {
                {"bind_port", std::to_string(router_port_)},
                {"destinations",
                 "metadata-cache://main_cluster/?role=PRIMARY_AND_SECONDARY"},
                {"protocol", "classic"},
                {"routing_strategy", "round-robin"},
                {"server_ssl_mode", "PREFERRED"},
                {"connection_sharing", "1"},
                {"connection_sharing_delay", "0"},
            });

    auto &section = writer.sections()["routing:under_test"];
    for (auto [k, v] : extra_options) {
      section[k] = v;
    }

    auto &proc = router_spawner().wait_for_notify_ready(2s).spawn(
        {"-c", writer.write()});
    ASSERT_NO_ERROR(proc.wait_for_sync_point_result());
  }

  const std::array<Node, 3> &nodes() const { return nodes_; }

  static stdx::expected<std::string, testing::AssertionResult> get_trace(
      MysqlClient &cli) {
    auto warnings_res = query_one_result(cli, "SHOW warnings");
    if (!warnings_res) {
      return stdx::unexpected(testing::AssertionFailure()
                              << warnings_res.error());
    }

    auto warnings = *warnings_res;

    EXPECT_THAT(warnings, SizeIs(::testing::Ge(1)));
    if (warnings.empty()) {
      return stdx::unexpected(testing::AssertionFailure()
                              << "expected warnings to be not empty.");
    }

    auto json_row = warnings_res->back();

    EXPECT_THAT(json_row, ElementsAre("Note", kErRouterTrace, ::testing::_));

    if (json_row.size() != 3 || json_row[0] != "Note" ||
        json_row[1] != kErRouterTrace) {
      return stdx::unexpected(testing::AssertionFailure()
                              << "expected warnings to be not empty.");
    }

    return json_row[2];
  }

  static testing::AssertionResult trace_is_valid(rapidjson::Document &doc) {
    if (doc.HasParseError()) {
      return testing::AssertionFailure()
             << rapidjson::GetParseError_En(doc.GetParseError());
    }

    rapidjson::SchemaDocument schema(*schema_doc_);
    rapidjson::SchemaValidator validator(schema);
    if (!doc.Accept(validator)) {
      rapidjson::StringBuffer schema_uri;
      validator.GetInvalidSchemaPointer().StringifyUriFragment(schema_uri);

      rapidjson::StringBuffer doc_uri;
      validator.GetInvalidDocumentPointer().StringifyUriFragment(doc_uri);

      return testing::AssertionFailure() << validator.GetError();
    }

    return testing::AssertionSuccess();
  }

  static testing::AssertionResult json_pointer_eq(
      rapidjson::Document &doc, const rapidjson::Pointer &pointer,
      const rapidjson::Value &expected_value) {
    auto *value = pointer.Get(doc);

    if (value == nullptr) {
      rapidjson::StringBuffer sb;
      pointer.Stringify(sb);

      return testing::AssertionFailure() << sb.GetString() << " not found";
    }

    // sadly googletest's ::testing::Eq() can't be used here as it wants to copy
    // the Value, which is move-only.
    if (*value != expected_value) {
      rapidjson::StringBuffer lhs_sb;
      {
        rapidjson::Writer writer(lhs_sb);
        value->Accept(writer);
      }
      rapidjson::StringBuffer rhs_sb;
      {
        rapidjson::Writer writer(rhs_sb);
        expected_value.Accept(writer);
      }

      rapidjson::StringBuffer pointer_sb;
      pointer.Stringify(pointer_sb);

      return testing::AssertionFailure()
             << "Value of: " << pointer_sb.GetString()
             << ", Actual: " << lhs_sb.GetString()
             << " Expected: " << rhs_sb.GetString();
    }

    return testing::AssertionSuccess();
  }

 protected:
  static constexpr const std::string_view schema_json{R"({
  "$schema": "http://json-schema.org/draft-04/schema#",
  "type": "object",
  "properties": {
    "start_time": {
      "type": "string",
      "format": "date-time"
    },
    "end_time": {
      "type": "string",
      "format": "date-time"
    },
    "timestamp": {
      "type": "string",
      "format": "date-time"
    },
    "name": {
      "type": "string"
    },
    "status_code": {
      "type": "string"
    },
    "attributes": {
      "type": "object"
    },
    "events": {
      "type": ["array"],
      "items": { "$ref": "#/" }
    }
  },
  "required": ["name"]
})"};

  TempDirectory conf_dir_;

  uint16_t router_port_{port_pool_.get_next_available()};

  std::array<Node, 3> nodes_{};

  static constexpr const char rest_user_[] = "user";
  static constexpr const char rest_pass_[] = "pass";

  static std::unique_ptr<rapidjson::Document> schema_doc_;
};

std::unique_ptr<rapidjson::Document> RoutingSplittingTestBase::schema_doc_;

class RoutingSplittingAccessModeNotSetTest : public RoutingSplittingTestBase {
 public:
  void SetUp() override {
    RouterComponentTest::SetUp();
    start_mock_cluster();

    start_router({
        {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
        {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        {"client_ssl_mode", "PREFERRED"},
    });
  }
};

TEST_F(RoutingSplittingAccessModeNotSetTest, router_set_access_mode_fails) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR3");
  RecordProperty(
      "Requirement",
      "If the config-option `connect_sharing` is `1` and the config-option "
      "`access_mode` is not set and Router receives a statement starting with "
      "`ROUTER`, the statement MUST fail.");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  {
    auto query_res = cli.query("ROUTER SET access_mode='auto'");
    ASSERT_ERROR(query_res);
    // ROUTER SET access_mode not allowed by configuration.
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }
}

class RoutingSplittingTest : public RoutingSplittingTestBase {
  void SetUp() override {
    RouterComponentTest::SetUp();

    start_mock_cluster();
    start_router({
        {"access_mode", "auto"},  // with splitting

        {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
        {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        {"client_ssl_mode", "PREFERRED"},
    });
  }
};

TEST_F(RoutingSplittingTest, default_access_mode_is_auto) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR1");
  RecordProperty(
      "Requirement",
      "If the config-option `access_mode` is `auto` and `protocol` is "
      "`classic`, the session's initial `access_mode` MUST be `auto`");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("select @@port             // to secondary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre(std::to_string(nodes_[1].classic_port))));
  }
}

TEST_F(RoutingSplittingTest, router_set_access_mode_read_only_with_trx) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR2.4");
  RecordProperty("Description",
                 "After ROUTER SET access_mode='read_only' a "
                 "transaction MUST be targetted at the read-only server.");

  // ndx=1 is the SECONDARY.
  const auto expected_port = std::to_string(nodes_[1].classic_port);

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("ROUTER SET access_mode = 'read_only'");
  ASSERT_NO_ERROR(cli.query("ROUTER SET access_mode = 'read_only'"));

  SCOPED_TRACE("select @@port             // to secondary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }

  SCOPED_TRACE("START TRANSACTION         // to secondary");
  ASSERT_NO_ERROR(cli.query("START TRANSACTION"));

  SCOPED_TRACE("select @@port             // to secondary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }

  SCOPED_TRACE("INSERT                    // to secondary");
  {
    auto stmt_res = cli.query("INSERT INTO testing.t1 VALUES ()");
    ASSERT_ERROR(stmt_res);
    // super-read-only
    EXPECT_EQ(stmt_res.error().value(), 1290) << stmt_res.error();
  }

  SCOPED_TRACE("ROLLBACK                  // to secondary");
  ASSERT_NO_ERROR(cli.query("ROLLBACK"));
}

TEST_F(RoutingSplittingTest, router_set_access_mode_read_write_with_trx) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR2.5");
  RecordProperty("Description",
                 "After ROUTER SET access_mode='read_write' a read-only "
                 "transaction MUST be targeted at the read-write server.");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("ROUTER SET access_mode = 'read_write'");
  ASSERT_NO_ERROR(cli.query("ROUTER SET access_mode = 'read_write'"));

  // ndx=0 is the PRIMARY.
  const auto expected_port = std::to_string(nodes_[0].classic_port);

  SCOPED_TRACE("select @@port             // to primary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }

  SCOPED_TRACE("START TRANSACTION READ ONLY // to primary");
  {
    auto query_res = cli.query("START TRANSACTION READ ONLY");
    ASSERT_NO_ERROR(query_res);
  }

  SCOPED_TRACE("select @@port               // to primary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }

  SCOPED_TRACE("COMMIT                      // to primary");
  ASSERT_NO_ERROR(cli.query("COMMIT"));
}

TEST_F(RoutingSplittingTest, instance_local_stmt_is_forbidden) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR2.6");
  RecordProperty("Requirement",
                 "If the config-option access_mode is 'auto' and "
                 "instance local statement is received, "
                 "Router MUST return an error.");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  for (const auto &stmt : {
           "ALTER SERVER",
           "CREATE SERVER",
           "DROP SERVER",
           "LOCK TABLES testing.t1 READ",
           "SHOW GLOBAL STATUS",
           "SHUTDOWN",
           "START GROUP_REPLICATION",
           "START REPLICA",
           "STOP GROUP_REPLICATION",
           "STOP REPLICA",
           "UNLOCK TABLES",
       }) {
    SCOPED_TRACE(stmt);

    auto query_res = cli.query(stmt);
    ASSERT_ERROR(query_res);
    // Statement not allowed if access_mode is 'auto'
    EXPECT_EQ(query_res.error().value(), 4501) << query_res.error();
  }
}

TEST_F(RoutingSplittingTest, access_mode_auto_no_trx_read_only_stmt) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR2.9");
  RecordProperty(
      "Description",
      "If the session's `access_mode` is `auto` and not transaction is "
      "started, read-only statements MUST be targeted at a read-only server.");

  // ndx=1 is the SECONDARY.
  const auto expected_port = std::to_string(nodes_[1].classic_port);

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("select @@port             // to secondary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }
}

TEST_F(RoutingSplittingTest, access_mode_auto_no_trx_read_write_stmt) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR2.10");
  RecordProperty(
      "Description",
      "If the session's `access_mode` is `auto` and not transaction is "
      "started, read-write statements MUST be targeted at a read-write "
      "server.");

  // start at PRIMARY
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    SCOPED_TRACE("// connect");
    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    SCOPED_TRACE("// INSERT               // to primary");
    ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));
  }

  // start at SECONARY (round-robin)
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    SCOPED_TRACE("// connect              // to secondary");
    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    SCOPED_TRACE("// INSERT               // to primary");
    ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));
  }
}

// ROUTER SET access_mode.

TEST_F(RoutingSplittingTest, router_set_access_mode_inside_trx) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR3.1");
  RecordProperty("Requirement",
                 "If the config option `access_mode` is set to `auto` while a "
                 "transaction is open, Router MUST return an error.");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("START TRANSACTION  // block ROUTER SET access_mode.");
  {
    auto query_res = cli.query("START TRANSACTION");
    ASSERT_NO_ERROR(query_res);
  }

  SCOPED_TRACE("ROUTER SET access_mode = 'read_only'");
  {
    auto query_res = cli.query("ROUTER SET access_mode = 'read_only'");
    ASSERT_ERROR(query_res);
    // 'ROUTER SET access_mode = <...>' not allowed while transaction is active.
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }

  SCOPED_TRACE("ROUTER SET access_mode='read_write'");
  {
    auto query_res = cli.query("ROUTER SET access_mode='read_write'");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }

  SCOPED_TRACE("ROUTER SET access_mode = 'auto'");
  {
    auto query_res = cli.query("ROUTER SET access_mode = 'auto'");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }

  SCOPED_TRACE("ROLLBACK  // unblock ROUTER SET access_mode.");
  {
    auto query_res = cli.query("ROLLBACK");
    ASSERT_NO_ERROR(query_res);
  }

  SCOPED_TRACE("ROUTER SET access_mode = 'auto' works again.");
  {
    auto query_res = cli.query("ROUTER SET access_mode = 'auto'");
    ASSERT_NO_ERROR(query_res);
  }

  SCOPED_TRACE("ROUTER SET access_mode='read_write'");
  {
    auto query_res = cli.query("ROUTER SET access_mode='read_write'");
    ASSERT_NO_ERROR(query_res);
  }
}

TEST_F(RoutingSplittingTest, router_set_access_mode_sharing_blocked) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR3.2");
  RecordProperty(
      "Requirement",
      "If the config option `access_mode` is set to `auto` while "
      "connection sharing is not possible, Router MUST return an error.");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("SET @block_sharing  // block ROUTER SET access_mode.");
  {
    auto query_res = cli.query("SET @block_sharing = 1");
    ASSERT_NO_ERROR(query_res);
  }

  SCOPED_TRACE("ROUTER SET access_mode='read_write'");
  {
    auto query_res = cli.query("ROUTER SET access_mode='read_write'");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }
}

TEST_F(RoutingSplittingTest, router_set_access_mode_read_only) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR3.3");
  RecordProperty("Requirement",
                 "If the config option `access_mode` is set to `auto` and no "
                 "transaction is open and `ROUTER SET access_mode='read_only'` "
                 "is received, Router MUST return success and set the "
                 "session's `access_mode` to `read_only`.");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("ROUTER SET access_mode = 'read_only'");
  {
    auto query_res = cli.query("ROUTER SET access_mode = 'read_only'");
    ASSERT_NO_ERROR(query_res);
  }

  SCOPED_TRACE("select @@port             // to secondary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre(std::to_string(nodes_[1].classic_port))));
  }
}

TEST_F(RoutingSplittingTest, router_set_access_mode_read_write) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR3.4");
  RecordProperty(
      "Requirement",
      "If the config option `access_mode` is set to `auto` and no "
      "transaction is open and `ROUTER SET access_mode='read_write'` "
      "is received, Router MUST return success and set the "
      "session's `access_mode` to `read_write`.");

  // ndx=0 is the PRIMARY.
  const auto expected_port = std::to_string(nodes_[0].classic_port);

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("ROUTER SET access_mode = 'read_write'");
  {
    auto query_res = cli.query("ROUTER SET access_mode = 'read_write'");
    ASSERT_NO_ERROR(query_res);
  }

  SCOPED_TRACE("select @@port             // to primary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }
}

TEST_F(RoutingSplittingTest, router_set_access_mode_auto) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR3.5");
  RecordProperty("Requirement",
                 "If the config option `access_mode` is set to `auto` and no "
                 "transaction is open and `ROUTER SET access_mode='auto'` "
                 "is received, Router MUST return success and set the "
                 "session's `access_mode` to `auto`.");

  // ndx=1 is the SECONDARY.
  const auto expected_port = std::to_string(nodes_[1].classic_port);

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE(
      "ROUTER SET access_mode = 'read_write' // to have a non-default value");
  {
    auto query_res = cli.query("ROUTER SET access_mode = 'read_write'");
    ASSERT_NO_ERROR(query_res);
  }

  SCOPED_TRACE("ROUTER SET access_mode = 'auto'");
  {
    auto query_res = cli.query("ROUTER SET access_mode = 'auto'");
    ASSERT_NO_ERROR(query_res);
  }

  SCOPED_TRACE("select @@port             // to secondary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }
}

TEST_F(RoutingSplittingTest, router_set_access_mode_invalid) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR3.6");
  RecordProperty(
      "Requirement",
      "If the config option `access_mode` is set to `auto` and `ROUTER SET "
      "access_mode` with an unexpected value, Router MUST return an error");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("ROUTER SET access_mode = 1");
  {
    auto query_res = cli.query("ROUTER SET access_mode = 1");
    ASSERT_ERROR(query_res);
  }

  SCOPED_TRACE("ROUTER SET access_mode = 'unknown'");
  {
    auto query_res = cli.query("ROUTER SET access_mode = 'unknown'");
    ASSERT_ERROR(query_res);
  }
}

// transaction access mode

TEST_F(RoutingSplittingTest, start_transaction_read_only_to_secondary) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR4.1");
  RecordProperty("Requirement",
                 "If the session's `access_mode` is `auto` and a read-only "
                 "non-XA transaction is started, all its statements MUST be "
                 "sent to a read-only server.");
  RecordProperty("Description", "START TRANSACTION READ ONLY");

  // ndx=1 is the SECONDARY.
  const auto expected_port = std::to_string(nodes_[1].classic_port);

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("START TRANSACTION READ ONLY // to any");
  ASSERT_NO_ERROR(cli.query("START TRANSACTION READ ONLY"));

  SCOPED_TRACE("select @@port        // to secondary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }

  SCOPED_TRACE("COMMIT               // to secondary");
  ASSERT_NO_ERROR(cli.query("COMMIT"));
}

TEST_F(RoutingSplittingTest, set_transaction_read_only_to_secondary) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR4.1");
  RecordProperty("Requirement",
                 "If the session's `access_mode` is `auto` and a read-only "
                 "non-XA transaction is started, all its statements MUST be "
                 "sent to a read-only server.");
  RecordProperty("Description",
                 "SET TRANSACTION READ ONLY + START TRANSACTION");

  // ndx=1 is the SECONDARY.
  const auto expected_port = std::to_string(nodes_[1].classic_port);

  SCOPED_TRACE("// connect");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("SET TRANSACTION READ ONLY // to any");
  ASSERT_NO_ERROR(cli.query("SET TRANSACTION READ ONLY"));

  SCOPED_TRACE("START TRANSACTION         // to any");
  ASSERT_NO_ERROR(cli.query("START TRANSACTION"));

  SCOPED_TRACE("select @@port             // to secondary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }

  SCOPED_TRACE("COMMIT                    // to secondary");
  ASSERT_NO_ERROR(cli.query("COMMIT"));
}

TEST_F(RoutingSplittingTest, start_transaction_to_primary) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR4.2");
  RecordProperty("Requirement",
                 "If the session's `access_mode` is `auto` and a read-write "
                 "non-XA transaction is started, all its statements MUST be "
                 "sent to a read-write server.");
  RecordProperty("Description", "START TRANSACTION");

  // ndx=0 is the PRIMARY.
  const auto expected_port = std::to_string(nodes_[0].classic_port);

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("START TRANSACTION    // to any");
  ASSERT_NO_ERROR(cli.query("START TRANSACTION"));

  SCOPED_TRACE("select @@port        // to primary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }

  SCOPED_TRACE("COMMIT               // to primary");
  ASSERT_NO_ERROR(cli.query("COMMIT"));
}

TEST_F(RoutingSplittingTest, xa_start_to_primary) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR4.3");
  RecordProperty("Requirement",
                 "If the session's `access_mode` is `auto` and a "
                 "XA transaction is started, all its statements MUST be "
                 "sent to a read-write server.");
  RecordProperty("Description", "XA START 'abc'");

  // ndx=0 is the PRIMARY.
  const auto expected_port = std::to_string(nodes_[0].classic_port);

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("XA BEGIN 'ab'        // to any");
  ASSERT_NO_ERROR(cli.query("XA BEGIN 'ab'"));

  SCOPED_TRACE("select @@port        // to primary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }

  SCOPED_TRACE("XA END 'ab'          // to primary");
  ASSERT_NO_ERROR(cli.query("XA END 'ab'"));

  SCOPED_TRACE("XA PREPARE 'ab'      // to primary");
  ASSERT_NO_ERROR(cli.query("XA PREPARE 'ab'"));

  SCOPED_TRACE("XA COMMIT 'ab'       // to primary");
  ASSERT_NO_ERROR(cli.query("XA COMMIT 'ab'"));
}

TEST_F(RoutingSplittingTest, xa_start_read_only_to_primary) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR4.3");
  RecordProperty("Requirement",
                 "If the session's `access_mode` is `auto` and a "
                 "XA transaction is started, all its statements MUST be "
                 "sent to a read-write server.");
  RecordProperty("Description", "SET TRANSACTION READ ONLY + XA START 'abc'");

  // ndx=0 is the PRIMARY.
  const auto expected_port = std::to_string(nodes_[0].classic_port);

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("SET TRANSACTION READ ONLY   // to any");
  ASSERT_NO_ERROR(cli.query("SET TRANSACTION READ ONLY"));

  SCOPED_TRACE("XA BEGIN 'ab'        // to primary");
  ASSERT_NO_ERROR(cli.query("XA BEGIN 'ab'"));

  SCOPED_TRACE("select @@port        // to primary");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(expected_port)));
  }

  SCOPED_TRACE("XA END 'ab'          // to primary");
  ASSERT_NO_ERROR(cli.query("XA END 'ab'"));

  SCOPED_TRACE("XA PREPARE 'ab'      // to primary");
  ASSERT_NO_ERROR(cli.query("XA PREPARE 'ab'"));

  SCOPED_TRACE("XA COMMIT 'ab'       // to primary");
  ASSERT_NO_ERROR(cli.query("XA COMMIT 'ab'"));
}

// query attribute router.access_mode.

TEST_F(RoutingSplittingTest, attribute_router_access_mode_in_transaction) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR5.1");
  RecordProperty("Requirement",
                 "If the query attribute is set inside a transaction, the "
                 "statement MUST fail.");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect to first node (primary)");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("START TRANSACTION");
  {
    auto query_res = cli.query("START TRANSACTION");
    ASSERT_NO_ERROR(query_res);
  }

  SCOPED_TRACE("router.access_mode = 'read_only' should fail");
  {
    std::array<MYSQL_BIND, 1> params{
        StringParam{"read_only"},
    };
    std::array<const char *, 1> names{"router.access_mode"};
    auto query_res = cli.query("DO 1", params, names);
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1766)
        << query_res.error();  // can't set variable inside a transaction.
  }
}

TEST_F(RoutingSplittingTest, attribute_router_access_mode_read_write) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR5.2");
  RecordProperty("Requirement",
                 "If the query attribute `router.access_mode` is `read_write`, "
                 "the statement MUST be sent to a read-write server.");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect to first node (primary)");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("router.access_mode = 'read_write' + DO 1");
  {
    std::array<MYSQL_BIND, 1> params{
        StringParam{"read_write"},
    };

    std::array<const char *, 1> names{"router.access_mode"};

    auto query_res = query_one_result(cli, "select @@port", params, names);
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre(std::to_string(nodes_[0].classic_port))));
  }
}

TEST_F(RoutingSplittingTest, attribute_router_access_mode_read_only) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR5.3");
  RecordProperty("Requirement",
                 "If the query attribute `router.access_mode` is `read_only`, "
                 "the statement, MUST be sent to a read-only server.");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect to first node (primary)");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("router.access_mode = 'read_only' + INSERT");
  {
    std::array<MYSQL_BIND, 1> params{
        StringParam{"read_only"},
    };
    std::array<const char *, 1> names{"router.access_mode"};
    auto query_res =
        cli.query("INSERT INTO testing.t1 VALUES ()", params, names);
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1290);  // --super-read-only.
  }
}

TEST_F(RoutingSplittingTest, attribute_router_access_mode_invalid) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR5.4");
  RecordProperty(
      "Requirement",
      "If the query attribute `router.access_mode` has an unknown value, "
      "the statement MUST fail.");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  SCOPED_TRACE("// connect to first node (primary)");
  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("router.access_mode = 'read_only' + INSERT");
  {
    std::array<MYSQL_BIND, 1> params{
        StringParam{"unknown"},
    };
    std::array<const char *, 1> names{"router.access_mode"};
    auto query_res =
        cli.query("INSERT INTO testing.t1 VALUES ()", params, names);
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }
}

// reset-connection.

TEST_F(RoutingSplittingTest,
       reset_connection_resets_session_wait_for_my_writes_timeout) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR7.4");
  RecordProperty("Requirement",
                 "If `access_mode` is 'auto' and the client sends a "
                 "reset-connection, Router MUST reset the session's "
                 "`wait_for_my_writes_timeout`");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("ROUTER SET wait_for_my_writes_timeout = 0"));

  // generate a GTID.
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  {
    // should not wait.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "0"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "1")));
  }

  // restore the timeout to the intial value.
  ASSERT_NO_ERROR(cli.reset_connection());

  // generate a GTID.
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  {
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "1"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "1")));
  }
}

TEST_F(RoutingSplittingTest, reset_connection_resets_stickiness) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR7.6");
  RecordProperty(
      "Requirement",
      "If `access_mode` is 'auto' and the client sends a "
      "reset-connection, Router MUST reset the other remembered destination");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  // remember a read-only backend
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    // any port is fine.
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));
  }

  // remember a read-write backend
  ASSERT_NO_ERROR(cli.query("START TRANSACTION"));

  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    // any is fine, but it should differ from the first one.
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));
  }

  // abort the transaction and allow the read-only on another backend.
  ASSERT_NO_ERROR(cli.reset_connection());

  uint16_t ro_port{};
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    // any port is fine.
    ASSERT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));

    auto ro_port_res = from_string((*query_res)[0][0]);
    ASSERT_NO_ERROR(ro_port_res);
    ro_port = *ro_port_res;
  }

  // allow the read-only on another backend.
  ASSERT_NO_ERROR(cli.reset_connection());

  // stop the backend for this port, it should fail over to the other RO.
  ASSERT_NO_ERROR(shutdown_server(ro_port));

  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));
  }
}

// stickiness

TEST_F(RoutingSplittingTest, authenticate_against_any_destination) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR10.1");
  RecordProperty("Requirement",
                 "If connection-sharing is possible, Router MUST authenticate "
                 "against any server.");

  SCOPED_TRACE("// connect to one ...");
  {
    MysqlClient cli;

    cli.username("count_me");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
  }

  SCOPED_TRACE("// connect to another ...");
  {
    MysqlClient cli;

    cli.username("count_me");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
  }

  SCOPED_TRACE("// connect to another ...");
  {
    MysqlClient cli;

    cli.username("count_me");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
  }

  // connect to the backend to check how often it got connected.
  //
  for (const auto &node : nodes()) {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", node.classic_port));

    {
      auto query_res =
          query_one_result(cli,
                           "select variable_value from "
                           "performance_schema.global_status_variables where "
                           "variable_name = 'Connections'");
      ASSERT_NO_ERROR(query_res);
      ASSERT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));

      auto connections_res = from_string((*query_res)[0][0]);
      ASSERT_NO_ERROR(connections_res);

      // one node may get more than 2 connections (metadata-cache)
      // all others should get one from:
      // - this query
      // - the above connect
      EXPECT_THAT(*connections_res, testing::Ge(2));
    }
  }
}

class RoutingSplittingNoSslTest : public RoutingSplittingTestBase {
 public:
  void SetUp() override {
    RouterComponentTest::SetUp();
    start_mock_cluster();

    start_router({
        {"access_mode", "auto"},  // with splitting

        {"client_ssl_mode", "DISABLED"},  // no TLS
    });
  }
};

TEST_F(RoutingSplittingNoSslTest,
       authenticate_against_primary_destination_if_sharing_not_possible) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR10.2");
  RecordProperty("Requirement",
                 "If access_mode='auto' and connection-sharing is NOT "
                 "possible, Router MUST authenticate "
                 "against any server.");

  SCOPED_TRACE("// connect to one ...");
  {
    MysqlClient cli;

    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.username("count_me");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
  }

  SCOPED_TRACE("// connect to another ...");
  {
    MysqlClient cli;

    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.username("count_me");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
  }

  SCOPED_TRACE("// connect to another ...");
  {
    MysqlClient cli;

    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.username("count_me");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
  }

  // connect to the backend to check how often it got connected.
  //
  for (const auto &node : nodes()) {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", node.classic_port));

    {
      auto query_res =
          query_one_result(cli,
                           "select variable_value from "
                           "performance_schema.global_status_variables where "
                           "variable_name = 'Connections'");
      ASSERT_NO_ERROR(query_res);
      ASSERT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));

      auto connections_res = from_string((*query_res)[0][0]);
      ASSERT_NO_ERROR(connections_res);

      // one node may get more than 4 connections (metadata-cache)
      // all others should get one from:
      // - this query
      EXPECT_THAT(*connections_res, AnyOf(1, testing::Ge(4)));
    }
  }
}

TEST_F(RoutingSplittingTest,
       use_authenticated_servers_for_read_or_write_start_at_primary) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR10.3");
  RecordProperty("Requirement",
                 "If access_mode='auto' and connection can be shared, "
                 "Router MUST remember the access-mode of the server it "
                 "authenticated against and use it for read-only/read-write "
                 "depending on the server's mode.");
  RecordProperty("Description", "authenticate at PRIMARY");

  SCOPED_TRACE("// connect to primary");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    // if started on a RW node, this will be sticky.
    // if started on a RO node, this will switch.
    ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

    // switch to secondary.
    {
      auto query_res = query_one_result(cli, "select @@port");
      ASSERT_NO_ERROR(query_res);
      EXPECT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));
    }
  }

  // connect to the backend to check how often it got connected.

  std::vector<int> connections_per_node;
  for (const auto &node : nodes()) {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", node.classic_port));

    {
      auto query_res =
          query_one_result(cli,
                           "select variable_value from "
                           "performance_schema.global_status_variables where "
                           "variable_name = 'Connections'");
      ASSERT_NO_ERROR(query_res);
      ASSERT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));

      auto connections_res = from_string((*query_res)[0][0]);
      ASSERT_NO_ERROR(connections_res);
      connections_per_node.push_back(*connections_res);
    }
  }

  // - the PRIMARY: got the metadata-cache connection + the INSERT + this query.
  // - one SECONDARY got the switch-to-secondary + this query
  // - the other SECONDARY: only this query.
  //
  EXPECT_THAT(connections_per_node,
              testing::UnorderedElementsAre(1, testing::Ge(3), 2));
}

TEST_F(RoutingSplittingTest,
       use_authenticated_servers_for_read_or_write_start_at_secondary) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR10.3");
  RecordProperty("Requirement",
                 "If access_mode='auto' and connection can be shared, "
                 "Router MUST remember the access-mode of the server it "
                 "authenticated against and use it for read-only/read-write "
                 "depending on the server's mode.");
  RecordProperty("Description", "authenticate at SECONARY");

  SCOPED_TRACE("// connect to primary");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
  }

  SCOPED_TRACE("// connect to secondary (round-robin)");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    // if started on a RW node, this will be sticky.
    // if started on a RO node, this will switch.
    ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

    // switch to secondary.
    {
      auto query_res = query_one_result(cli, "select @@port");
      ASSERT_NO_ERROR(query_res);
      EXPECT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));
    }
  }

  // connect to the backend to check how often it got connected.

  std::vector<int> connections_per_node;
  for (const auto &node : nodes()) {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", node.classic_port));

    {
      auto query_res =
          query_one_result(cli,
                           "select variable_value from "
                           "performance_schema.global_status_variables where "
                           "variable_name = 'Connections'");
      ASSERT_NO_ERROR(query_res);
      ASSERT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));

      auto connections_res = from_string((*query_res)[0][0]);
      ASSERT_NO_ERROR(connections_res);
      connections_per_node.push_back(*connections_res);
    }
  }

  // - the PRIMARY: got the metadata-cache connection + the INSERT + this query.
  // - one SECONDARY got the switch-to-secondary + this query
  // - the other SECONDARY: only this query.
  //
  EXPECT_THAT(connections_per_node,
              testing::UnorderedElementsAre(1, testing::Ge(3), 2));
}

TEST_F(RoutingSplittingTest, insert_is_sticky) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR10.4");
  RecordProperty("Requirement",
                 "If the statement is targeted for a read-write server and "
                 "this session has not connected to a read-write server yet, "
                 "Router MUST try to open a connection to a read-write server. "
                 "Afterwards all following commands targeted for read-write "
                 "server MUST target the same read-write server.");

  SCOPED_TRACE(
      "// connect to primary, to move the round-robin to the secondary");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
  }

  SCOPED_TRACE("// connect to secondary");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    ASSERT_NO_ERROR(cli.query("ROUTER SET trace=1"));

    // switch back to primary.
    ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

    {
      auto trace_res = get_trace(cli);
      ASSERT_TRUE(trace_res);

      auto json_trace = *trace_res;

      rapidjson::Document doc;
      doc.Parse(json_trace.data(), json_trace.size());
      ASSERT_TRUE(trace_is_valid(doc));

      for (const auto &[pntr, val] : {
               std::pair{"/name", rapidjson::Value("mysql/query")},
               std::pair{"/events/0/name",
                         rapidjson::Value("mysql/query_classify")},
               std::pair{"/events/0/attributes/mysql.query.classification",
                         rapidjson::Value(
                             "accept_session_state_from_session_tracker")},
               std::pair{"/events/1/name",
                         rapidjson::Value("mysql/connect_and_forward")},
               std::pair{"/events/1/events/0/name",
                         rapidjson::Value("mysql/prepare_server_connection")},
               std::pair{"/events/1/events/0/events/0/name",
                         rapidjson::Value("mysql/from_pool_or_connect")},
               std::pair{"/events/1/events/0/events/0/events/0/name",
                         rapidjson::Value("mysql/from_pool")},
           }) {
        ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
            << json_trace;
      }

      auto endpoint_val = rapidjson::Value(
          "127.0.0.1:" + std::to_string(nodes_[0].classic_port),
          doc.GetAllocator());

      // if localhost resolves to 127.0.0.1 the connect to 127.0.0.1 succeeds:
      // then the trace will have:
      //
      // - from_pool: success. -> /0/
      //
      // if localhost resolves to ::1 and 127.0.0.1 and the connect to ::1
      // fails, then the trace will have:
      //
      // - from_pool: fails    -> /0/
      // - connect: fails      -> /1/
      // - from_pool: success. -> /2/
      ASSERT_TRUE(
          json_pointer_eq(doc,
                          rapidjson::Pointer(
                              "/events/1/events/0/events/0/events/0/attributes/"
                              "mysql.remote.endpoint"),
                          endpoint_val) ||
          json_pointer_eq(doc,
                          rapidjson::Pointer(
                              "/events/1/events/0/events/0/events/2/attributes/"
                              "mysql.remote.endpoint"),
                          endpoint_val));
    }

    SCOPED_TRACE("check a 2nd INSERT goes to the same backend");
    {
      auto query_res = cli.query("INSERT INTO testing.t1 VALUES ()");
      ASSERT_NO_ERROR(query_res);
    }

    {
      auto trace_res = get_trace(cli);
      ASSERT_TRUE(trace_res);

      auto json_trace = *trace_res;

      rapidjson::Document doc;
      doc.Parse(json_trace.data(), json_trace.size());
      ASSERT_TRUE(trace_is_valid(doc));

      for (const auto &[pntr, val] : {
               std::pair{"/name", rapidjson::Value("mysql/query")},
               std::pair{"/events/0/name",
                         rapidjson::Value("mysql/query_classify")},
               std::pair{"/events/0/attributes/mysql.query.classification",
                         rapidjson::Value(
                             "accept_session_state_from_session_tracker")},
               std::pair{"/events/1/name",
                         rapidjson::Value("mysql/connect_and_forward")},
               std::pair{"/events/1/events/0/name",
                         rapidjson::Value("mysql/prepare_server_connection")},
               std::pair{"/events/1/events/0/events/0/name",
                         rapidjson::Value("mysql/from_stash")},
               std::pair{
                   "/events/1/events/0/events/0/attributes/"
                   "mysql.remote.endpoint",
                   rapidjson::Value(
                       "127.0.0.1:" + std::to_string(nodes_[0].classic_port),
                       doc.GetAllocator())},
           }) {
        ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
            << json_trace;
      }
    }
  }
}

TEST_F(RoutingSplittingTest, select_is_sticky) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR10.5");
  RecordProperty("Requirement",
                 "If the statement is targeted for a read-only server and "
                 "this session has not connected to a read-only server yet, "
                 "Router MUST try to open a connection to a read-only server. "
                 "Afterwards all following commands targeted for read-only "
                 "server MUST target the same read-only server.");

  SCOPED_TRACE("// connect");

  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("ROUTER SET trace=1"));

  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre(std::to_string(nodes_[1].classic_port))));
  }

  {
    auto trace_res = get_trace(cli);
    ASSERT_TRUE(trace_res);

    auto json_trace = *trace_res;

    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());
    ASSERT_TRUE(trace_is_valid(doc));

    for (const auto &[pntr, val] : {
             std::pair{"/name", rapidjson::Value("mysql/query")},
             std::pair{"/events/0/name",
                       rapidjson::Value("mysql/query_classify")},
             std::pair{
                 "/events/0/attributes/mysql.query.classification",
                 rapidjson::Value(
                     "accept_session_state_from_session_tracker,read-only")},
             std::pair{"/events/1/name",
                       rapidjson::Value("mysql/connect_and_forward")},
             std::pair{"/events/1/events/0/name",
                       rapidjson::Value("mysql/prepare_server_connection")},
             std::pair{"/events/1/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool_or_connect")},
             // pool is empty.
             std::pair{"/events/1/events/0/events/0/events/1/name",
                       rapidjson::Value("mysql/connect")},
             std::pair{"/events/1/events/0/events/0/events/1/attributes/"
                       "net.peer.port",
                       rapidjson::Value(std::to_string(nodes_[1].classic_port),
                                        doc.GetAllocator())},
         }) {
      ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
          << json_trace;
    }
  }

  SCOPED_TRACE("check a 2nd SELECT goes to the same backend");
  {
    auto query_res = query_one_result(cli, "select @@port");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre(std::to_string(nodes_[1].classic_port))));
  }

  {
    auto trace_res = get_trace(cli);
    ASSERT_TRUE(trace_res);

    auto json_trace = *trace_res;

    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());
    ASSERT_TRUE(trace_is_valid(doc));

    for (const auto &[pntr, val] : {
             std::pair{"/name", rapidjson::Value("mysql/query")},
             std::pair{"/events/0/name",
                       rapidjson::Value("mysql/query_classify")},
             std::pair{
                 "/events/0/attributes/mysql.query.classification",
                 rapidjson::Value(
                     "accept_session_state_from_session_tracker,read-only")},
             std::pair{"/events/1/name",
                       rapidjson::Value("mysql/connect_and_forward")},
             std::pair{"/events/1/events/0/name",
                       rapidjson::Value("mysql/prepare_server_connection")},
             std::pair{"/events/1/events/0/events/0/name",
                       rapidjson::Value("mysql/from_stash")},
             std::pair{
                 "/events/1/events/0/events/0/attributes/"
                 "mysql.remote.endpoint",
                 rapidjson::Value(
                     "127.0.0.1:" + std::to_string(nodes_[1].classic_port),
                     doc.GetAllocator())},
         }) {
      ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
          << json_trace;
    }
  }
}

TEST_F(RoutingSplittingTest, connect_retry_secondary) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR10.6");
  RecordProperty("Requirement",
                 "If access_mode='auto' and connection to a backend fails with "
                 "a transient error, Router MUST behave according to "
                 "`connect_retry_timeout`.");
  RecordProperty("Requirement", "retry secondary");

  SCOPED_TRACE("// connect to primary");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
  }

  SCOPED_TRACE("// set secondaries to 'fail-transient-once'");
  for (auto [ndx, node] : stdx::views::enumerate(nodes())) {
    if (ndx == 0) continue;

    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", node.classic_port));
    ASSERT_NO_ERROR(cli.query("MOCK fail_connect_transient_once()"));
  }

  SCOPED_TRACE("// connect to secondary (round-robin)");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    // if started on a RW node, this will be sticky.
    // if started on a RO node, this will switch.
    ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

    // switch to secondary.
    {
      auto query_res = query_one_result(cli, "select @@port");
      ASSERT_NO_ERROR(query_res);
      EXPECT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));
    }
  }

  // connect to the backend to check how often it got connected.
  std::vector<uint64_t> connections_per_node;
  for (const auto &node : nodes()) {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto connect_res = cli.connect("127.0.0.1", node.classic_port);
    if (!connect_res) {
      // one node will still be in 'fail-once'.
      EXPECT_EQ(connect_res.error().value(), 1040) << connect_res.error();
      connections_per_node.push_back(0);
    } else {
      auto query_res =
          query_one_result(cli,
                           "select variable_value from "
                           "performance_schema.global_status_variables where "
                           "variable_name = 'Connections'");
      ASSERT_NO_ERROR(query_res);
      ASSERT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));

      auto connections_res = from_string((*query_res)[0][0]);
      ASSERT_NO_ERROR(connections_res);
      connections_per_node.push_back(*connections_res);
    }
  }

  // - the PRIMARY: the metadata-cache connection + the INSERT + this query.
  // - one SECONDARY:
  //   - the setup
  //   - the switch-to-secondary
  //   - the reconnect
  //   - this query
  // - the other SECONDARY: only this query.
  //
  EXPECT_THAT(connections_per_node,
              testing::UnorderedElementsAre(testing::Ge(3), 4, 0));
}

TEST_F(RoutingSplittingTest, connect_fail_read_only) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR10.7");
  RecordProperty("Requirement",
                 "If access_mode='auto' and connection to a backend fails with "
                 "a non-transient error, Router MUST return an error and "
                 "close the connection.");

  SCOPED_TRACE("// connect to primary");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));
  }

  SCOPED_TRACE("// connect to secondary (round-robin)");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    // switch to primary.
    ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

    SCOPED_TRACE("// set secondaries to 'fail-once'");
    for (auto [ndx, node] : stdx::views::enumerate(nodes())) {
      if (ndx == 0) continue;

      MysqlClient mock_cli;

      mock_cli.username("foo");
      mock_cli.password("bar");

      ASSERT_NO_ERROR(mock_cli.connect("127.0.0.1", node.classic_port));
      ASSERT_NO_ERROR(mock_cli.query("MOCK fail_connect_once()"));
    }

    // wait until the connection pool is empty to force a reconnect.
    std::this_thread::sleep_for(kIdleTimeout + 1s);

    // switch to secondary.
    {
      auto query_res = query_one_result(cli, "select @@port");
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(), 1129);
    }
  }

  // connect to the backend to check how often it got connected.
  std::vector<uint64_t> connections_per_node;
  for (const auto &node : nodes()) {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto connect_res = cli.connect("127.0.0.1", node.classic_port);
    if (!connect_res) {
      // host blocked.
      EXPECT_EQ(connect_res.error().value(), 1129) << connect_res.error();
      connections_per_node.push_back(0);
    } else {
      auto query_res =
          query_one_result(cli,
                           "select variable_value from "
                           "performance_schema.global_status_variables where "
                           "variable_name = 'Connections'");
      ASSERT_NO_ERROR(query_res);
      ASSERT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));

      auto connections_res = from_string((*query_res)[0][0]);
      ASSERT_NO_ERROR(connections_res);
      connections_per_node.push_back(*connections_res);
    }
  }

  // - the PRIMARY: the metadata-cache connection + the INSERT + this query.
  // - one SECONDARY:
  //   - the setup
  //   - the switch-to-secondary
  //   - this query
  // - the other SECONDARY: only this query.
  //
  EXPECT_THAT(connections_per_node,
              testing::UnorderedElementsAre(testing::Ge(3), 4, 0));
}

TEST_F(RoutingSplittingTest,
       failover_at_on_demand_connect_if_not_connected_yet) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR10.8");
  RecordProperty("Requirement",
                 "If access_mode='auto' and connection to a backend fails with "
                 "a non-transient error, Router MUST return an error and "
                 "close the connection.");

  SCOPED_TRACE("// shutdown the node that the SELECT would be sent to.");
  ASSERT_NO_ERROR(shutdown_server(nodes()[1].classic_port));

  SCOPED_TRACE("// connect to primary");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    // if started on a RW node, this will be sticky.
    // if started on a RO node, this will switch.
    ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

    // fail should failover to another secondary or fallback to primary.
    {
      auto query_res = query_one_result(cli, "select @@port");
      ASSERT_NO_ERROR(query_res);
      EXPECT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));
    }
  }

  // connect to the backend to check how often it got connected.
  std::vector<int> connections_per_node;
  for (const auto &node : nodes()) {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");

    auto connect_res = cli.connect("127.0.0.1", node.classic_port);

    if (node.classic_port == nodes()[1].classic_port) {
      ASSERT_ERROR(connect_res);

      connections_per_node.push_back(0);
    } else {
      ASSERT_NO_ERROR(connect_res);

      {
        auto query_res =
            query_one_result(cli,
                             "select variable_value from "
                             "performance_schema.global_status_variables where "
                             "variable_name = 'Connections'");
        ASSERT_NO_ERROR(query_res);
        ASSERT_THAT(*query_res, ElementsAre(ElementsAre(testing::_)));

        auto connections_res = from_string((*query_res)[0][0]);
        ASSERT_NO_ERROR(connections_res);
        connections_per_node.push_back(*connections_res);
      }
    }
  }

  // - the PRIMARY: the metadata-cache connection + the INSERT + this query.
  // - the SECONDARY: is dead.
  // - the last SECONDARY:
  //   - the switch-to-secondary
  //   - this query
  //
  EXPECT_THAT(connections_per_node, testing::ElementsAre(testing::Ge(3), 0, 2));
}

// multi-statements

TEST_F(RoutingSplittingTest, multi_statements_are_forbidden) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR2.1");
  RecordProperty(
      "Requirement",
      "If the session's `access-mode` is `auto` and a multi-statement is "
      "received, Router MUST return an error to the client.");

  SCOPED_TRACE("// connect");
  {
    MysqlClient cli;

    cli.username("foo");
    cli.password("bar");
    cli.flags(CLIENT_MULTI_STATEMENTS);

    ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

    {
      auto query_res = cli.query("DO 1; DO 2");
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(),
                4501);  // multi-statements are forbidden.
    }

    {
      auto query_res =
          cli.query("CREATE PROCEDURE testing.foo () BEGIN DO 1; DO 2; END");
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(),
                1273);  // syntax error (from mock-server)
    }

    {
      auto query_res = cli.query(
          "CREATE PROCEDURE testing.foo () BEGIN IF 1 THEN DO 1; DO 2; END IF; "
          "END");
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(),
                1273);  // syntax error (from mock-server)
    }

    {
      auto query_res = cli.query("BEGIN; DO 1; DO 2; COMMIT");
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(),
                4501);  // multi-statements are forbidden.
    }

    // trailing comma is ok.
    {
      auto query_res = cli.query("DO 2;");
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(),
                1273);  // syntax error (from mock-server)
    }
  }
}

// wait for my writes

TEST_F(RoutingSplittingTest, wait_for_my_writes_default) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR11.1");
  RecordProperty("Requirement",
                 "If `wait_for_my_writes` is enabled, Router MUST wait max "
                 "`wait_for_my_writes_timeout` seconds "
                 "for the session's last written transactions to be applied on "
                 "read-only servers.");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  ASSERT_NO_ERROR(cli.query("ROUTER SET trace=1"));

  // switch to secondary and trigger a wait.
  ASSERT_NO_ERROR(cli.query("DO 1"));

  {
    auto trace_res = get_trace(cli);
    ASSERT_TRUE(trace_res);

    auto json_trace = *trace_res;

    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());
    ASSERT_TRUE(trace_is_valid(doc));

    for (const auto &[pntr, val] : {
             std::pair{"/name", rapidjson::Value("mysql/query")},
             std::pair{"/events/0/name",
                       rapidjson::Value("mysql/query_classify")},
             std::pair{
                 "/events/0/attributes/mysql.query.classification",
                 rapidjson::Value(
                     "accept_session_state_from_session_tracker,read-only")},
             std::pair{"/events/1/name",
                       rapidjson::Value("mysql/connect_and_forward")},
             std::pair{"/events/1/events/0/name",
                       rapidjson::Value("mysql/prepare_server_connection")},
             std::pair{"/events/1/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool_or_connect")},
             // pool is empty.
             std::pair{"/events/1/events/0/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool")},
             // open a new connections to the ...
             std::pair{"/events/1/events/0/events/0/events/1/name",
                       rapidjson::Value("mysql/connect")},
             // ... secondary
             std::pair{"/events/1/events/0/events/0/events/1/attributes/"
                       "net.peer.port",
                       rapidjson::Value(std::to_string(nodes_[1].classic_port),
                                        doc.GetAllocator())},
             std::pair{"/events/1/events/0/events/1/name",
                       rapidjson::Value("mysql/authenticate")},
             std::pair{"/events/1/events/0/events/2/name",
                       rapidjson::Value("mysql/set_var")},
             // it waited.
             std::pair{"/events/1/events/0/events/3/name",
                       rapidjson::Value("mysql/wait_gtid_executed")},
             // if wait-gtid-executed passes, next will be forward.
             std::pair{"/events/1/events/1/name",
                       rapidjson::Value("mysql/forward")},
         }) {
      ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
          << json_trace;
    }
  }
}

TEST_F(RoutingSplittingTest, router_set_wait_for_my_writes_off) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR11.3");
  RecordProperty("Requirement",
                 "If `wait_for_my_writes` is disabled, ROUTER MUST not wait "
                 "for the last transaction.");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  {
    auto query_res = cli.query("router set wait_for_my_writes=0");
    ASSERT_NO_ERROR(query_res);
  }

  {
    auto query_res = cli.query("router set trace=1");
    ASSERT_NO_ERROR(query_res);
  }

  // last committed transaction
  {
    auto query_res = cli.query("INSERT INTO testing.t1 VALUES ()");
    ASSERT_NO_ERROR(query_res);
  }

  // read only statement.
  {
    auto query_res = cli.query("DO 1");
    ASSERT_NO_ERROR(query_res);
  }

  {
    auto trace_res = get_trace(cli);
    ASSERT_TRUE(trace_res);

    auto json_trace = *trace_res;

    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());
    ASSERT_TRUE(trace_is_valid(doc));

    for (const auto &[pntr, val] : {
             std::pair{"/name", rapidjson::Value("mysql/query")},
             std::pair{"/events/0/name",
                       rapidjson::Value("mysql/query_classify")},
             std::pair{
                 "/events/0/attributes/mysql.query.classification",
                 rapidjson::Value(
                     "accept_session_state_from_session_tracker,read-only")},
             std::pair{"/events/1/name",
                       rapidjson::Value("mysql/connect_and_forward")},
             std::pair{"/events/1/events/0/name",
                       rapidjson::Value("mysql/prepare_server_connection")},
             std::pair{"/events/1/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool_or_connect")},
             // pool is empty.
             std::pair{"/events/1/events/0/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool")},
             std::pair{"/events/1/events/0/events/0/events/1/name",
                       rapidjson::Value("mysql/connect")},
             std::pair{"/events/1/events/0/events/0/events/1/attributes/"
                       "net.peer.port",
                       rapidjson::Value(std::to_string(nodes_[1].classic_port),
                                        doc.GetAllocator())},
             std::pair{"/events/1/events/0/events/1/name",
                       rapidjson::Value("mysql/authenticate")},
             std::pair{"/events/1/events/0/events/2/name",
                       rapidjson::Value("mysql/set_var")},
             // if wait-gtid-executed passes, next will be forward.
             std::pair{"/events/1/events/1/name",
                       rapidjson::Value("mysql/forward")},
         }) {
      ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
          << json_trace;
    }

    // it didn't wait.
    ASSERT_EQ(rapidjson::Pointer("/events/1/events/0/events/4/name").Get(doc),
              nullptr)
        << json_trace;
  }
}

TEST_F(RoutingSplittingTest, router_set_wait_for_my_writes_timeout_0) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR11.2");
  RecordProperty(
      "Requirement",
      "If the session-variable `wait_for_my_writes_timeout` is exceeded, "
      "Router MUST try to fallback to the read-write server.");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  SCOPED_TRACE("wait_for_my_writes_timeout=0");
  {
    auto query_res = cli.query("router set wait_for_my_writes_timeout=0");
    ASSERT_NO_ERROR(query_res);
  }

  {
    auto query_res = cli.query("router set trace=1");
    ASSERT_NO_ERROR(query_res);
  }

  // last committed transaction
  {
    auto query_res = cli.query("INSERT INTO testing.t1 VALUES ()");
    ASSERT_NO_ERROR(query_res);
  }

  // read only statement.
  {
    auto query_res = cli.query("DO 1");
    ASSERT_NO_ERROR(query_res);
  }

  {
    auto trace_res = get_trace(cli);
    ASSERT_TRUE(trace_res);

    auto json_trace = *trace_res;

    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());
    ASSERT_TRUE(trace_is_valid(doc));

    for (const auto &[pntr, val] : {
             std::pair{"/name", rapidjson::Value("mysql/query")},
             std::pair{"/events/0/name",
                       rapidjson::Value("mysql/query_classify")},
             std::pair{
                 "/events/0/attributes/mysql.query.classification",
                 rapidjson::Value(
                     "accept_session_state_from_session_tracker,read-only")},
             std::pair{"/events/1/name",
                       rapidjson::Value("mysql/connect_and_forward")},
             std::pair{"/events/1/events/0/name",
                       rapidjson::Value("mysql/prepare_server_connection")},
             std::pair{"/events/1/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool_or_connect")},
             // pool is empty.
             std::pair{"/events/1/events/0/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool")},
             std::pair{"/events/1/events/0/events/0/events/1/name",
                       rapidjson::Value("mysql/connect")},
             std::pair{"/events/1/events/0/events/0/events/1/attributes/"
                       "net.peer.port",
                       rapidjson::Value(std::to_string(nodes_[1].classic_port),
                                        doc.GetAllocator())},
             std::pair{"/events/1/events/0/events/1/name",
                       rapidjson::Value("mysql/authenticate")},
             std::pair{"/events/1/events/0/events/2/name",
                       rapidjson::Value("mysql/set_var")},
             // it waited, with timeout 0.
             std::pair{"/events/1/events/0/events/3/name",
                       rapidjson::Value("mysql/wait_gtid_executed")},
             // if wait-gtid-executed passes, next will be forward.
             std::pair{"/events/1/events/1/name",
                       rapidjson::Value("mysql/forward")},
         }) {
      ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
          << json_trace;
    }
  }
}

// tests which call start_router() itself.
class RoutingSplittingManualTest : public RoutingSplittingTestBase {
  void SetUp() override {
    RouterComponentTest::SetUp();

    start_mock_cluster();
  }
};

// config: wait-for-my-writes

TEST_F(RoutingSplittingManualTest, config_wait_for_my_writes_is_not_set) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR12.2");
  RecordProperty("Requirement",
                 "If the config-option `wait_for_my_writes` is not set, "
                 "Router MUST set session's `wait_for_my_writes` to `1` after "
                 "the client connected");

  start_router({
      {"access_mode", "auto"},  // with splitting

      {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
      {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      {"client_ssl_mode", "PREFERRED"},
  });

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  ASSERT_NO_ERROR(cli.query("ROUTER SET trace=1"));

  // switch to secondary and trigger a wait.
  ASSERT_NO_ERROR(cli.query("DO 1"));

  {
    auto trace_res = get_trace(cli);
    ASSERT_TRUE(trace_res);

    auto json_trace = *trace_res;

    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());
    ASSERT_TRUE(trace_is_valid(doc));

    for (const auto &[pntr, val] : {
             std::pair{"/name", rapidjson::Value("mysql/query")},
             std::pair{"/events/0/name",
                       rapidjson::Value("mysql/query_classify")},
             std::pair{
                 "/events/0/attributes/mysql.query.classification",
                 rapidjson::Value(
                     "accept_session_state_from_session_tracker,read-only")},
             std::pair{"/events/1/name",
                       rapidjson::Value("mysql/connect_and_forward")},
             std::pair{"/events/1/events/0/name",
                       rapidjson::Value("mysql/prepare_server_connection")},
             std::pair{"/events/1/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool_or_connect")},
             // pool is empty.
             std::pair{"/events/1/events/0/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool")},
             // open a new connections to the ...
             std::pair{"/events/1/events/0/events/0/events/1/name",
                       rapidjson::Value("mysql/connect")},
             // ... secondary
             std::pair{"/events/1/events/0/events/0/events/1/attributes/"
                       "net.peer.port",
                       rapidjson::Value(std::to_string(nodes_[1].classic_port),
                                        doc.GetAllocator())},
             std::pair{"/events/1/events/0/events/1/name",
                       rapidjson::Value("mysql/authenticate")},
             std::pair{"/events/1/events/0/events/2/name",
                       rapidjson::Value("mysql/set_var")},
             // it waited.
             std::pair{"/events/1/events/0/events/3/name",
                       rapidjson::Value("mysql/wait_gtid_executed")},
             // if wait-gtid-executed passes, next will be forward.
             std::pair{"/events/1/events/1/name",
                       rapidjson::Value("mysql/forward")},
         }) {
      ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
          << json_trace;
    }
  }
}

TEST_F(RoutingSplittingManualTest, config_wait_for_my_writes_is_zero) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR12.3");
  RecordProperty("Requirement",
                 "If the config-option `wait_for_my_writes` is `0`, "
                 "Router MUST set session's `wait_for_my_writes` to `0` after "
                 "the client connected");

  ASSERT_NO_FATAL_FAILURE(start_router({
      {"access_mode", "auto"},      // with splitting
      {"wait_for_my_writes", "0"},  //

      {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
      {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      {"client_ssl_mode", "PREFERRED"},
  }));

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  ASSERT_NO_ERROR(cli.query("ROUTER SET trace=1"));

  // switch to secondary and trigger a wait.
  ASSERT_NO_ERROR(cli.query("DO 1"));

  {
    auto trace_res = get_trace(cli);
    ASSERT_TRUE(trace_res);

    auto json_trace = *trace_res;

    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());
    ASSERT_TRUE(trace_is_valid(doc));

    for (const auto &[pntr, val] : {
             std::pair{"/name", rapidjson::Value("mysql/query")},
             std::pair{"/events/0/name",
                       rapidjson::Value("mysql/query_classify")},
             std::pair{
                 "/events/0/attributes/mysql.query.classification",
                 rapidjson::Value(
                     "accept_session_state_from_session_tracker,read-only")},
             std::pair{"/events/1/name",
                       rapidjson::Value("mysql/connect_and_forward")},
             std::pair{"/events/1/events/0/name",
                       rapidjson::Value("mysql/prepare_server_connection")},
             std::pair{"/events/1/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool_or_connect")},
             // pool is empty.
             std::pair{"/events/1/events/0/events/0/events/0/name",
                       rapidjson::Value("mysql/from_pool")},
             // open a new connections to the ...
             std::pair{"/events/1/events/0/events/0/events/1/name",
                       rapidjson::Value("mysql/connect")},
             // ... secondary
             std::pair{"/events/1/events/0/events/0/events/1/attributes/"
                       "net.peer.port",
                       rapidjson::Value(std::to_string(nodes_[1].classic_port),
                                        doc.GetAllocator())},
             std::pair{"/events/1/events/0/events/1/name",
                       rapidjson::Value("mysql/authenticate")},
             std::pair{"/events/1/events/0/events/2/name",
                       rapidjson::Value("mysql/set_var")},
             // if wait-gtid-executed passes, next will be forward.
             std::pair{"/events/1/events/1/name",
                       rapidjson::Value("mysql/forward")},
         }) {
      ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
          << json_trace;
    }

    // no wait-for-gtid
    ASSERT_EQ(rapidjson::Pointer("/events/1/events/0/events/4/name").Get(doc),
              nullptr);
  }
}

TEST_F(RoutingSplittingManualTest, config_wait_for_my_writes_is_one) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR12.4");
  RecordProperty("Requirement",
                 "If the config-option `wait_for_my_writes` is `1`, "
                 "Router MUST set session's `wait_for_my_writes` to `1` after "
                 "the client connected");

  start_router({
      {"access_mode", "auto"},      // with splitting
      {"wait_for_my_writes", "1"},  //

      {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
      {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      {"client_ssl_mode", "PREFERRED"},
  });

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  // switch to secondary and a wait.
  {
    // should wait for 1 second.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "1"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "0")));
  }
}

// config: wait-for-my-writes-timeout

TEST_F(RoutingSplittingManualTest,
       config_wait_for_my_writes_timeout_is_not_set) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR13.2");
  RecordProperty(
      "Requirement",
      "If the config-option `wait_for_my_writes_timeout` is not set, "
      "Router MUST default to `1` second.");

  ASSERT_NO_FATAL_FAILURE(start_router({
      {"access_mode", "auto"},  // with splitting
      {"wait_for_my_writes", "1"},

      {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
      {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      {"client_ssl_mode", "PREFERRED"},
  }));

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  // switch to secondary and a wait.
  {
    // should wait for 1 second.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "1"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "0")));
  }
}

TEST_F(RoutingSplittingManualTest, config_wait_for_my_writes_timeout_is_valid) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR13.3");
  RecordProperty(
      "Requirement",
      "If the config-option `wait_for_my_writes_timeout` is valid, "
      "Router MUST set session's `wait_for_my_writes_timeout` to its when "
      "the client connects");

  start_router({
      {"access_mode", "auto"},  // with splitting
      {"wait_for_my_writes", "1"},
      {"wait_for_my_writes_timeout", "0"},

      {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
      {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      {"client_ssl_mode", "PREFERRED"},
  });

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  // switch to secondary and a wait.
  {
    // should not wait.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "0"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "1")));
  }
}

TEST_F(RoutingSplittingManualTest, router_set_wait_for_my_writes_is_zero) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR14.1");
  RecordProperty(
      "Requirement",
      "If Router receives `ROUTER SET wait_for_my_writes` with a valid value "
      "Router MUST set session's `wait_for_my_writes` to that value");

  start_router({
      {"access_mode", "auto"},  // with splitting

      {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
      {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      {"client_ssl_mode", "PREFERRED"},
  });

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  ASSERT_NO_ERROR(cli.query("ROUTER SET wait_for_my_writes = 0"));

  // switch to secondary and a wait.
  {
    // should wait for 0 seconds.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "0"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "0")));
  }
}

TEST_F(RoutingSplittingManualTest, router_set_wait_for_my_writes_is_one) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR14.1");
  RecordProperty(
      "Requirement",
      "If Router receives `ROUTER SET wait_for_my_writes` with a valid value "
      "Router MUST set session's `wait_for_my_writes` to that value");

  start_router({
      {"access_mode", "auto"},  // with splitting

      {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
      {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      {"client_ssl_mode", "PREFERRED"},
  });

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  ASSERT_NO_ERROR(cli.query("ROUTER SET wait_for_my_writes = 1"));

  // switch to secondary and a wait.
  {
    // should wait for 0 seconds.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "1"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "0")));
  }
}

TEST_F(RoutingSplittingTest, router_set_wait_for_my_writes_invalid) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR14.2");
  RecordProperty("Requirement",
                 "If `ROUTER SET wait_for_my_writes` is called with an "
                 "unexpected value, it MUST fail.");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  {
    auto query_res = cli.query("router set wait_for_my_writes=2");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }

  {
    auto query_res = cli.query("router set wait_for_my_writes=-1");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }

  {
    auto query_res = cli.query("router set wait_for_my_writes='abc'");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }

  {
    auto query_res = cli.query("router set wait_for_my_writes=null");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }
}

// ROUTER SET: wait_for_my_writes_timeout.

TEST_F(RoutingSplittingManualTest,
       router_set_wait_for_my_writes_timeout_is_zero) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR15.1");
  RecordProperty(
      "Requirement",
      "If Router receives `ROUTER SET wait_for_my_writes_timeout` with a valid "
      "value "
      "Router MUST set session's `wait_for_my_writes_timeout` to that value");

  start_router({
      {"access_mode", "auto"},  // with splitting

      {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
      {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
      {"client_ssl_mode", "PREFERRED"},
  });

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  ASSERT_NO_ERROR(cli.query("ROUTER SET wait_for_my_writes_timeout = 0"));

  // switch to secondary and a wait.
  {
    // should wait for 0 seconds.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "0"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "1")));
  }
}

TEST_F(RoutingSplittingTest, router_set_wait_for_my_writes_timeout_invalid) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR15.2");
  RecordProperty("Requirement",
                 "If `ROUTER SET wait_for_my_writes_timeout` is called with an "
                 "unexpected value, it MUST fail.");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  {
    auto query_res = cli.query("router set wait_for_my_writes_timeout=3601");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }

  {
    auto query_res = cli.query("router set wait_for_my_writes_timeout=-1");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }

  {
    auto query_res = cli.query("router set wait_for_my_writes_timeout='abc'");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }

  {
    auto query_res = cli.query("router set wait_for_my_writes_timeout=null");
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }
}

TEST_F(RoutingSplittingTest, wait_for_my_writes_query_attribute_has_priority) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR16.1");
  RecordProperty(
      "Requirement",
      "If the query-attribute `router.wait_for_my_writes` is called "
      "with `0` or `1`, it MUST set the session's `wait_for_my_writes`.");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("ROUTER SET wait_for_my_writes = 1"));

  // generate a GTID.
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  {
    int wait_for_my_writes = 0;  // don't wait.
    std::array<MYSQL_BIND, 1> params{
        IntegerParam{&wait_for_my_writes},
    };

    std::array<const char *, 1> names{
        "router.wait_for_my_writes",
    };

    // should not wait.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables", params,
        names);
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "0"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "0")));
  }

  {
    // back the value of ROUTER SET.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "1"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "0")));
  }
}

TEST_F(RoutingSplittingTest, wait_for_my_writes_query_attribute_invalid_value) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR16.2");
  RecordProperty("Requirement",
                 "If the query-attribute `router.wait_for_my_writes` is called "
                 "with an invalid value, the statement MUST fail.");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  {
    int wait_for_my_writes = 25;
    std::array<MYSQL_BIND, 1> params{
        IntegerParam{&wait_for_my_writes},
    };

    std::array<const char *, 1> names{
        "router.wait_for_my_writes",
    };

    auto query_res = cli.query("select @@port", params, names);
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }
}

TEST_F(RoutingSplittingTest, wait_for_my_writes_query_attribute_invalid_type) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR16.2");
  RecordProperty("Requirement",
                 "If the query-attribute `router.wait_for_my_writes` is called "
                 "with an invalid value, the statement MUST fail.");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  {
    std::array<MYSQL_BIND, 1> params{
        StringParam{"not-a-number"},
    };

    std::array<const char *, 1> names{
        "router.wait_for_my_writes",
    };

    auto query_res = cli.query("select @@port", params, names);
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }
}

TEST_F(RoutingSplittingTest,
       wait_for_my_writes_timeout_query_attribute_has_priority) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR17.1");
  RecordProperty(
      "Requirement",
      "If the query-attribute `router.wait_for_my_writes` is called "
      "with `0` or `1`, it MUST set the session's `wait_for_my_writes`.");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  ASSERT_NO_ERROR(cli.query("ROUTER SET wait_for_my_writes = 1"));
  ASSERT_NO_ERROR(cli.query("ROUTER SET wait_for_my_writes_timeout = 1"));

  // generate a GTID.
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  {
    int wait_for_my_writes_timeout = 0;  // don't wait.
    std::array<MYSQL_BIND, 1> params{
        IntegerParam{&wait_for_my_writes_timeout},
    };

    std::array<const char *, 1> names{
        "router.wait_for_my_writes_timeout",
    };

    // should not wait.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables", params,
        names);
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "0"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "1")));
  }

  {
    // back the value of ROUTER SET.
    auto query_res = query_one_result(
        cli, "select * from performance_schema.status_variables");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(
        *query_res,
        ElementsAre(ElementsAre("Wait_for_executed_gtid_set", "1"),
                    ElementsAre("Wait_for_executed_gtid_set_no_timeout", "1")));
  }
}

TEST_F(RoutingSplittingTest,
       wait_for_my_writes_timeout_query_attribute_invalid_value) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR17.2");
  RecordProperty(
      "Requirement",
      "If the query-attribute `router.wait_for_my_writes_timeout` is called "
      "with an invalid value, the statement MUST fail.");
  RecordProperty("Description", "Invalid Value");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  {
    int wait_for_my_writes_timeout = -1;
    std::array<MYSQL_BIND, 1> params{
        IntegerParam{&wait_for_my_writes_timeout},
    };

    std::array<const char *, 1> names{
        "router.wait_for_my_writes_timeout",
    };

    auto query_res = cli.query("select @@port", params, names);
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }
}

TEST_F(RoutingSplittingTest,
       wait_for_my_writes_timeout_query_attribute_invalid_type) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR17.2");
  RecordProperty(
      "Requirement",
      "If the query-attribute `router.wait_for_my_writes_timeout` is called "
      "with an invalid value, the statement MUST fail.");
  RecordProperty("Description", "Invalid Type");

  SCOPED_TRACE("// connect");
  MysqlClient cli;

  cli.username("foo");
  cli.password("bar");

  ASSERT_NO_ERROR(cli.connect("127.0.0.1", router_port_));

  {
    std::array<MYSQL_BIND, 1> params{
        StringParam{"not-a-number"},
    };

    std::array<const char *, 1> names{
        "router.wait_for_my_writes_timeout",
    };

    auto query_res = cli.query("select @@port", params, names);
    ASSERT_ERROR(query_res);
    EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
  }
}

class RouterBootstrapTest : public RouterComponentBootstrapTest {};

TEST_F(RouterBootstrapTest, default_has_rw_split) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR18.1");
  RecordProperty("Requirement",
                 "If `mysqlrouter` is bootstrapped and `--disable-rw-split` is "
                 "NOT specified, the bootstrap MUST generate a `routing` "
                 "section which enables read-write splitting.");

  std::vector<Config> config{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_gr.js").str()},
  };

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      config, mysqlrouter::ClusterType::GR_V2, {}, EXIT_SUCCESS));

  ASSERT_THAT(config_file, ::testing::Not(::testing::IsEmpty()));

  const std::string config_file_str = get_file_output(config_file);

  EXPECT_THAT(config_file_str,
              ::testing::HasSubstr("[routing:bootstrap_rw_split]"));
}

TEST_F(RouterBootstrapTest, disable_rw_split) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR18.2");
  RecordProperty("Requirement",
                 "If `mysqlrouter` is bootstrapped and `--disable-rw-split` is "
                 "specified, the bootstrap MUST NOT generate a `routing` "
                 "section which enables read-write splitting.");

  std::vector<Config> config{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_gr.js").str()},
  };

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      config, mysqlrouter::ClusterType::GR_V2, {}, EXIT_SUCCESS, {}, 30s,
      mysqlrouter::MetadataSchemaVersion{2, 0, 3},
      {
          "--disable-rw-split",
      }));

  ASSERT_THAT(config_file, ::testing::Not(::testing::IsEmpty()));

  const std::string config_file_str = get_file_output(config_file);
  EXPECT_THAT(
      config_file_str,
      ::testing::Not(::testing::HasSubstr("[routing:bootstrap_rw_split]")));
}

// fail-to-start.

struct Requirement {
  Requirement(std::string id_, std::string text_)
      : id(std::move(id_)), text(std::move(text_)) {}

  Requirement(std::string id_, std::string text_, std::string desc)
      : id(std::move(id_)),
        text(std::move(text_)),
        description(std::move(desc)) {}

  std::string id;
  std::string text;
  std::string description;
};

struct RoutingSplittingConfigInvalidParam {
  std::string testname;

  Requirement requirement;

  std::map<std::string, std::string> extra_options;

  std::function<void(const std::string &)> log_matcher;
};

class RoutingSplittingConfigInvalid
    : public RouterComponentTest,
      public ::testing::WithParamInterface<RoutingSplittingConfigInvalidParam> {
 public:
  struct Node {
    uint16_t classic_port;
    uint16_t x_port;
    uint16_t http_port;
    ProcessWrapper *proc;
  };

  std::string cluster_id = "3a0be5af-0022-11e8-9655-0800279e6a88";

  void SetUp() override {
    RouterComponentTest::SetUp();

    std::vector<GRNode> gr_nodes;
    std::vector<ClusterNode> cluster_nodes;

    SCOPED_TRACE("// start mock-server");
    std::vector<uint16_t> classic_ports;
    for (auto &node : nodes_) {
      const auto classic_port = port_pool_.get_next_available();
      node.classic_port = classic_port;
      classic_ports.push_back(classic_port);
      node.x_port = port_pool_.get_next_available();
      node.http_port = port_pool_.get_next_available();

      node.proc = &ProcessManager::launch_mysql_server_mock(
          get_data_dir().join("splitting.js").str(), node.classic_port,
          EXIT_SUCCESS, false, node.http_port, node.x_port,
          "",  // module-prefix
          "127.0.0.1",
          30s,  // wait notify.
          true  // enable-ssl
      );
    }

    gr_nodes = classic_ports_to_gr_nodes(classic_ports);
    cluster_nodes = classic_ports_to_cluster_nodes(classic_ports);

    SCOPED_TRACE("// configure mock-servers");
    for (auto [ndx, node] : stdx::views::enumerate(nodes_)) {
      SCOPED_TRACE(
          "// Make our metadata server to return single node as a cluster "
          "member (meaning single metadata server)");
      set_mock_metadata(node.http_port,
                        cluster_id,     // gr-id
                        gr_nodes,       // gr-nodes
                        ndx,            // gr-pos
                        cluster_nodes,  // cluster-nodes
                        0,              // view-id
                        false,          // error-on-md-query
                        "127.0.0.1"     // gr-node-host
      );
    }
  }

 protected:
  std::array<Node, 3> nodes_{};

  TempDirectory conf_dir_;

  uint16_t server_port_{port_pool_.get_next_available()};
  uint16_t router_port_{port_pool_.get_next_available()};
};

TEST_P(RoutingSplittingConfigInvalid, check) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", GetParam().requirement.id);
  RecordProperty("Requirement", GetParam().requirement.text);
  auto desc = GetParam().requirement.description;
  if (!desc.empty()) {
    RecordProperty("Description", desc);
  }

  auto writer = config_writer(conf_dir_.name());

  auto &default_section = writer.sections()["DEFAULT"];
  init_keyring(default_section, conf_dir_.name());
  default_section["dynamic_state"] = create_state_file(
      conf_dir_.name(),
      create_state_file_content(cluster_id, {
                                                nodes_[0].classic_port,
                                                nodes_[1].classic_port,
                                                nodes_[2].classic_port,
                                            }));
  writer.section("connection_pool",  //
                 {
                     {"max_idle_server_connections", "64"},
                 });
  writer.section("metadata_cache",  //
                 {
                     {"cluster_type", "gr"},
                     {"router_id", "1"},
                     {"user", "mysql_router1_user"},
                     {"metadata_cluster", "main_cluster"},
                 });

  std::map<std::string, std::string> routing_options{
      {"bind_port", std::to_string(router_port_)},
      {"routing_strategy", "round-robin"},
  };

  for (auto kv : GetParam().extra_options) {
    routing_options.insert(kv);
  }

  writer.section("routing:under_test", routing_options);

  auto &proc =
      router_spawner()
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
          .expected_exit_code(EXIT_FAILURE)
          .spawn({"-c", writer.write()});

  proc.wait_for_exit();

  GetParam().log_matcher(proc.get_logfile_content());
}

static const RoutingSplittingConfigInvalidParam
    routing_splitting_invalid_params[] = {
        {"access_mode_unknown",
         {
             "FR1.2",
             "If the config option `access_mode` is set to an unexpected "
             "value, Router MUST fail to started",
         },
         {
             {"access_mode", "unknown"},
             {"protocol", "classic"},
             {"destinations",
              "metadata-cache://foo/?role=PRIMARY_AND_SECONDARY"},
         },
         [](const std::string &log) {
           EXPECT_THAT(
               log,
               ::testing::HasSubstr(
                   "option access_mode in [routing:under_test] is invalid; "
                   "valid are auto (was 'unknown')"));
         }},
        {"no_connection_sharing",
         {
             "FR1.3",
             "If the config option `access_mode` is set to `auto` and "
             "connection sharing is not enabled, Router MUST fail to start.",
         },
         {
             {"access_mode", "auto"},
             {"protocol", "classic"},
             {"destinations",
              "metadata-cache://foo/?role=PRIMARY_AND_SECONDARY"},
         },
         [](const std::string &log) {
           EXPECT_THAT(
               log, ::testing::HasSubstr(
                        "'access_mode=auto' requires 'connection_sharing=1'"));
         }},
        {"destination_not_metadata_cache",
         {
             "FR1.4",
             "If the config option `access_mode` is set to `auto` and "
             "`destinations is not a `metadata-cache` URL, Router MUST fail to "
             "start",
         },
         {
             {"access_mode", "auto"},
             {"protocol", "classic"},
             {"destinations", "127.0.0.1:3306"},
         },
         [](const std::string &log) {
           EXPECT_THAT(
               log, ::testing::HasSubstr("'access_mode=auto' requires "
                                         "'destinations=metadata-cache:...'"));
         }},
        {"destination_no_role",
         {"FR1.5",
          "If the config option `access_mode` is set to `auto` and "
          "`destinations` `metadata-cache` URL has a `role` that is not "
          "`PRIMARY_AND_SECONDARY` Router MUST fail to start",
          "no ?role"},
         {
             {"access_mode", "auto"},
             {"protocol", "classic"},
             {"destinations", "metadata-cache://foo/"},
         },
         [](const std::string &log) {
           EXPECT_THAT(
               log, ::testing::HasSubstr(
                        "Missing 'role' in routing destination specification"));
         }},
        {"destination_role_is_PRIMARY",
         {"FR1.5",
          "If the config option `access_mode` is set to `auto` and "
          "`destinations` `metadata-cache` URL has a `role` that is not "
          "`PRIMARY_AND_SECONDARY` Router MUST fail to start",
          "role is PRIMARY"},
         {
             {"access_mode", "auto"},
             {"protocol", "classic"},
             {"destinations", "metadata-cache://foo/?role=PRIMARY"},
         },
         [](const std::string &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "'access_mode=auto' requires that the 'role' "
                                "in 'destinations=metadata-cache:...?role=...' "
                                "is 'PRIMARY_AND_SECONDARY"));
         }},
        {"destination_role_is_SECONDARY",
         {"FR1.5",
          "If the config option `access_mode` is set to `auto` and "
          "`destinations` `metadata-cache` URL has a `role` that is not "
          "`PRIMARY_AND_SECONDARY` Router MUST fail to start",
          "role is SECONARY"},
         {
             {"access_mode", "auto"},
             {"protocol", "classic"},
             {"destinations", "metadata-cache://foo/?role=SECONDARY"},
         },
         [](const std::string &log) {
           EXPECT_THAT(log, ::testing::HasSubstr(
                                "'access_mode=auto' requires that the 'role' "
                                "in 'destinations=metadata-cache:...?role=...' "
                                "is 'PRIMARY_AND_SECONDARY"));
         }},
        {"protocol_not_classic",
         {
             "FR1.6",
             "If the config option `access_mode` is set to `auto` and "
             "`protocol` is NOT set to `classic`, Router MUST fail to start.",
         },
         {
             {"access_mode", "auto"},
             {"protocol", "x"},
             {"destinations",
              "metadata-cache://foo/?role=PRIMARY_AND_SECONDARY"},
         },
         [](const std::string &log) {
           EXPECT_THAT(
               log, ::testing::HasSubstr("'access_mode=auto' is only "
                                         "supported with 'protocol=classic'"));
         }},
        {"wait_for_my_writes_negative",
         {"FR12.1",
          "If the config option `wait_for_my_writes` has an invalid value, "
          "Router MUST fail to start.",
          "wait_for_my_writes=-1"},
         {
             {"access_mode", "auto"},
             {"protocol", "classic"},
             {"destinations",
              "metadata-cache://main_cluster/?role=PRIMARY_AND_SECONDARY"},
             {"connection_sharing", "1"},
             {"wait_for_my_writes", "-1"},
         },
         [](const std::string &log) {
           EXPECT_THAT(
               log,
               ::testing::HasSubstr(
                   "option wait_for_my_writes in [routing:under_test] needs a "
                   "value of either 0, 1, false or true, was '-1"));
         }},
        {"wait_for_my_writes_too_large",
         {"FR12.1",
          "If the config option `wait_for_my_writes` has an invalid value, "
          "Router MUST fail to start.",
          "wait_for_my_writes=2 (too large)"},
         {
             {"access_mode", "auto"},
             {"protocol", "classic"},
             {"destinations",
              "metadata-cache://main_cluster/?role=PRIMARY_AND_SECONDARY"},
             {"connection_sharing", "1"},
             {"wait_for_my_writes", "2"},
         },
         [](const std::string &log) {
           EXPECT_THAT(
               log,
               ::testing::HasSubstr(
                   "option wait_for_my_writes in [routing:under_test] needs a "
                   "value of either 0, 1, false or true, was '2"));
         }},
        {"wait_for_my_writes_timeout_not_a_number",
         {"FR13.1",
          "If the config option `wait_for_my_writes_timeout` is out-of-range, "
          "Router MUST fail to start.",
          "wait_for_my_writes_timeout=-1"},
         {
             {"access_mode", "auto"},
             {"protocol", "classic"},
             {"destinations",
              "metadata-cache://main_cluster/?role=PRIMARY_AND_SECONDARY"},
             {"connection_sharing", "1"},
             {"wait_for_my_writes_timeout", "abc"},
         },
         [](const std::string &log) {
           EXPECT_THAT(
               log,
               ::testing::HasSubstr(
                   "option wait_for_my_writes_timeout in [routing:under_test] "
                   "needs value between 0 and 3600 inclusive, was "
                   "'abc'"));
         }},
        {"wait_for_my_writes_timeout_too_small",
         {"FR13.1",
          "If the config option `wait_for_my_writes_timeout` is out-of-range, "
          "Router MUST fail to start.",
          "wait_for_my_writes_timeout=-1"},
         {
             {"access_mode", "auto"},
             {"protocol", "classic"},
             {"destinations",
              "metadata-cache://main_cluster/?role=PRIMARY_AND_SECONDARY"},
             {"connection_sharing", "1"},
             {"wait_for_my_writes_timeout", "-1"},
         },
         [](const std::string &log) {
           EXPECT_THAT(
               log,
               ::testing::HasSubstr(
                   "option wait_for_my_writes_timeout in [routing:under_test] "
                   "needs value between 0 and 3600 inclusive, was "
                   "'-1'"));
         }},
};

INSTANTIATE_TEST_SUITE_P(Spec, RoutingSplittingConfigInvalid,
                         ::testing::ValuesIn(routing_splitting_invalid_params),
                         [](auto &info) { return info.param.testname; });

int main(int argc, char *argv[]) {
  net::impl::socket::init();  // WSAStartup

  ProcessManager::set_origin(mysql_harness::Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
