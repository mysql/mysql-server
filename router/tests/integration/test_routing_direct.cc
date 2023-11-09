/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include <algorithm>  // min
#include <charconv>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_set>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock-more-matchers.h>
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#define RAPIDJSON_HAS_STDSTRING 1

#include "my_rapidjson_size_t.h"

#include <rapidjson/pointer.h>

#include "exit_status.h"
#include "hexify.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/stdx/filesystem.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysql/harness/tls_context.h"
#include "mysql/harness/utility/string.h"  // join
#include "mysqlrouter/classic_protocol_codec_frame.h"
#include "mysqlrouter/classic_protocol_codec_message.h"
#include "mysqlrouter/classic_protocol_frame.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/http_request.h"
#include "mysqlrouter/utils.h"
#include "openssl_version.h"  // ROUTER_OPENSSL_VERSION
#include "process_launcher.h"
#include "process_manager.h"
#include "procs.h"
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "scope_guard.h"
#include "shared_server.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::StartsWith;

static constexpr const auto kIdleServerConnectionsSleepTime{10ms};

static constexpr const std::string_view kDisabled{"DISABLED"};
static constexpr const std::string_view kRequired{"REQUIRED"};
static constexpr const std::string_view kPreferred{"PREFERRED"};
static constexpr const std::string_view kPassthrough{"PASSTHROUGH"};
static constexpr const std::string_view kAsClient{"AS_CLIENT"};

std::ostream &operator<<(std::ostream &os, MysqlError e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
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
      row_.reserve(field_count);

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
  if (!cmd_res) return stdx::make_unexpected(cmd_res.error());

  auto results = result_as_vector(*cmd_res);
  if (results.size() != 1) {
    return stdx::make_unexpected(MysqlError{1, "Too many results", "HY000"});
  }

  return results.front();
}

// query a single row and return an array of N std::strings.
template <size_t N>
stdx::expected<std::array<std::string, N>, MysqlError> query_one(
    MysqlClient &cli, std::string_view stmt) {
  auto cmd_res = cli.query(stmt);
  if (!cmd_res) return stdx::make_unexpected(cmd_res.error());

  auto results = std::move(*cmd_res);

  auto res_it = results.begin();
  if (res_it == results.end()) {
    return stdx::make_unexpected(MysqlError(1, "No results", "HY000"));
  }

  if (res_it->field_count() != N) {
    return stdx::make_unexpected(
        MysqlError(1, "field-count doesn't match", "HY000"));
  }

  auto rows = res_it->rows();
  auto rows_it = rows.begin();
  if (rows_it == rows.end()) {
    return stdx::make_unexpected(MysqlError(1, "No rows", "HY000"));
  }

  std::array<std::string, N> out;
  for (auto [ndx, f] : stdx::views::enumerate(out)) {
    f = (*rows_it)[ndx];
  }

  ++rows_it;
  if (rows_it != rows.end()) {
    return stdx::make_unexpected(MysqlError(1, "Too many rows", "HY000"));
  }

  ++res_it;
  if (res_it != results.end()) {
    return stdx::make_unexpected(MysqlError(1, "Too many results", "HY000"));
  }

  return out;
}

// convert a string to a number
static stdx::expected<uint64_t, std::error_code> from_string(
    std::string_view sv) {
  uint64_t num;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), num);

  if (ec != std::errc{}) return stdx::make_unexpected(make_error_code(ec));

  return num;
}

// get the pfs-events executed on a connection.
static stdx::expected<std::vector<std::pair<std::string, uint32_t>>, MysqlError>
changed_event_counters_impl(MysqlClient &cli, std::string_view stmt) {
  auto query_res = cli.query(stmt);
  if (!query_res) return stdx::make_unexpected(query_res.error());

  auto query_it = query_res->begin();

  if (!(query_it != query_res->end())) {
    return stdx::make_unexpected(MysqlError(1234, "No resultset", "HY000"));
  }

  if (2 != query_it->field_count()) {
    return stdx::make_unexpected(
        MysqlError(1234, "Expected two fields", "HY000"));
  }

  std::vector<std::pair<std::string, uint32_t>> events;

  for (auto row : query_it->rows()) {
    auto num_res = from_string(row[1]);
    if (!num_res) {
      return stdx::make_unexpected(
          MysqlError(1234,
                     "converting " + std::string(row[1] ? row[1] : "<NULL>") +
                         " to an <uint32_t> failed",
                     "HY000"));
    }

    events.emplace_back(row[0], *num_res);
  }

  return events;
}

static stdx::expected<std::vector<std::pair<std::string, uint32_t>>, MysqlError>
changed_event_counters(MysqlClient &cli) {
  return changed_event_counters_impl(cli, R"(SELECT EVENT_NAME, COUNT_STAR
 FROM performance_schema.events_statements_summary_by_thread_by_event_name AS e
 JOIN performance_schema.threads AS t ON (e.THREAD_ID = t.THREAD_ID)
WHERE t.PROCESSLIST_ID = CONNECTION_ID()
  AND COUNT_STAR > 0
ORDER BY EVENT_NAME)");
}

struct ConnectionParam {
  std::string testname;

  std::string_view client_ssl_mode;
  std::string_view server_ssl_mode;

  [[nodiscard]] bool redundant_combination() const {
    return
        // same as DISABLED|DISABLED
        (client_ssl_mode == kDisabled && server_ssl_mode == kAsClient) ||
        // same as DISABLED|REQUIRED
        (client_ssl_mode == kDisabled && server_ssl_mode == kPreferred) ||
        // same as PREFERRED|PREFERRED
        (client_ssl_mode == kPreferred && server_ssl_mode == kRequired) ||
        // same as REQUIRED|REQUIRED
        (client_ssl_mode == kRequired && server_ssl_mode == kAsClient) ||
        // same as REQUIRED|REQUIRED
        (client_ssl_mode == kRequired && server_ssl_mode == kPreferred);
  }
};

const ConnectionParam connection_params[] = {
    // DISABLED
    {
        "DISABLED__DISABLED",
        kDisabled,  // client_ssl_mode
        kDisabled,  // server_ssl_mode
    },
    {
        "DISABLED__AS_CLIENT",
        kDisabled,
        kAsClient,
    },
    {
        "DISABLED__REQUIRED",
        kDisabled,
        kRequired,
    },
    {
        "DISABLED__PREFERRED",
        kDisabled,
        kPreferred,
    },

    // PASSTHROUGH
    {
        "PASSTHROUGH__AS_CLIENT",
        kPassthrough,
        kAsClient,
    },

    // PREFERRED
    {
        "PREFERRED__DISABLED",
        kPreferred,
        kDisabled,
    },
    {
        "PREFERRED__AS_CLIENT",
        kPreferred,
        kAsClient,
    },
    {
        "PREFERRED__PREFERRED",
        kPreferred,
        kPreferred,
    },
    {
        "PREFERRED__REQUIRED",
        kPreferred,
        kRequired,
    },

    // REQUIRED ...
    {
        "REQUIRED__DISABLED",
        kRequired,
        kDisabled,
    },
    {
        "REQUIRED__AS_CLIENT",
        kRequired,
        kAsClient,
    },
    {
        "REQUIRED__PREFERRED",
        kRequired,
        kPreferred,
    },
    {
        "REQUIRED__REQUIRED",
        kRequired,
        kRequired,
    },
};

// distinct set of ssl-mode combinations for slow tests.
//
// If the client does SSL, REQUIRED and PREFERRED are equivalent
// if server_ssl_mode isn't "AS_CLIENT".
//
// If the client not use SSL, then DISABLED and PREFERRED are equivalent.
//
// For non-authentication related tests, it is enough to enable and disable SSL
// on the client and server side once.
//
// Tests for Authentication MUST use "connection_params" which checks all
// combinations.
const ConnectionParam distinct_connection_params[] = {
    // DISABLED
    {
        "DISABLED__DISABLED",
        kDisabled,  // client_ssl_mode
        kDisabled,  // server_ssl_mode
    },

    // PASSTHROUGH
    {
        "PASSTHROUGH__AS_CLIENT",
        kPassthrough,
        kAsClient,
    },

    // PREFERRED
    {
        "PREFERRED__AS_CLIENT",
        kPreferred,
        kAsClient,
    },

    // REQUIRED ...
    {
        "REQUIRED__REQUIRED",
        kRequired,
        kRequired,
    },
};

class SharedRouter {
 public:
  SharedRouter(TcpPortPool &port_pool)
      : port_pool_(port_pool), rest_port_{port_pool_.get_next_available()} {}

  integration_tests::Procs &process_manager() { return procs_; }

  static std::vector<std::string> destinations_from_shared_servers(
      const std::array<SharedServer *, 1> &servers) {
    std::vector<std::string> dests;
    dests.reserve(servers.size());

    for (const auto &s : servers) {
      dests.push_back(s->server_host() + ":" +
                      std::to_string(s->server_port()));
    }

    return dests;
  }

  void spawn_router(const std::vector<std::string> &destinations) {
    auto userfile = conf_dir_.file("userfile");
    {
      std::ofstream ofs(userfile);
      // user:pass
      ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
             "YJgciRvb69";
    }

    auto writer = process_manager().config_writer(conf_dir_.name());

    writer
        .section("rest_routing",
                 {
                     {"require_realm", "somerealm"},
                 })
        .section("http_auth_realm:somerealm",
                 {
                     {"backend", "somebackend"},
                     {"method", "basic"},
                     {"name", "some realm"},
                 })
        .section("http_auth_backend:somebackend",
                 {
                     {"backend", "file"},
                     {"filename", userfile},
                 })
        .section("http_server", {{"port", std::to_string(rest_port_)}});

    for (const auto &param : connection_params) {
      auto port_key =
          std::make_tuple(param.client_ssl_mode, param.server_ssl_mode);
      auto ports_it = ports_.find(port_key);

      const auto port =
          ports_it == ports_.end()
              ? (ports_[port_key] = port_pool_.get_next_available())
              : ports_it->second;

      writer.section("routing:classic_" + param.testname, {
        {"bind_port", std::to_string(port)},
#if !defined(_WIN32)
            {"socket", socket_path(param)},
#endif
            {"destinations", mysql_harness::join(destinations, ",")},
            {"protocol", "classic"}, {"routing_strategy", "round-robin"},

            {"client_ssl_mode", std::string(param.client_ssl_mode)},
            {"server_ssl_mode", std::string(param.server_ssl_mode)},

            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
            {"connection_sharing", "0"},  //
            {"connect_retry_timeout", "0"},
      });
    }

    auto bindir = process_manager().get_origin();
    auto builddir = bindir.join("..");

    auto &proc =
        process_manager()
            .spawner(bindir.join("mysqlrouter").str())
            .with_core_dump(true)
            .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::READY)
            .spawn({"-c", writer.write()});

    proc.set_logging_path(process_manager().get_logging_dir().str(),
                          "mysqlrouter.log");

    if (!proc.wait_for_sync_point_result()) {
      GTEST_SKIP() << "router failed to start";
    }
  }

  [[nodiscard]] auto host() const { return router_host_; }

  [[nodiscard]] uint16_t port(const ConnectionParam &param) const {
    return ports_.at(
        std::make_tuple(param.client_ssl_mode, param.server_ssl_mode));
  }

  [[nodiscard]] std::string socket_path(const ConnectionParam &param) const {
    return Path(conf_dir_.name())
        .join("classic_"s + std::string(param.client_ssl_mode) + "_" +
              std::string(param.server_ssl_mode) + ".sock")
        .str();
  }

  [[nodiscard]] auto rest_port() const { return rest_port_; }
  [[nodiscard]] auto rest_user() const { return rest_user_; }
  [[nodiscard]] auto rest_pass() const { return rest_pass_; }

  stdx::expected<int, std::error_code> rest_get_int(
      const std::string &uri, const std::string &pointer) {
    JsonDocument json_doc;

    fetch_json(rest_client_, uri, json_doc);

    if (auto *v = JsonPointer(pointer).Get(json_doc)) {
      if (!v->IsInt()) {
        return stdx::make_unexpected(
            make_error_code(std::errc::invalid_argument));
      }
      return v->GetInt();
    } else {
      std::cerr << json_doc << "\n";

      return stdx::make_unexpected(
          make_error_code(std::errc::no_such_file_or_directory));
    }
  }

  // number of active connections
  stdx::expected<int, std::error_code> num_connections(
      const ConnectionParam &param) {
    return rest_get_int(
        rest_api_basepath + "/routes/classic_" + param.testname + "/status",
        "/activeConnections");
  }

  // wait for number of active connections reaches a given value.
  stdx::expected<void, std::error_code> wait_for_num_connections(
      const ConnectionParam &param, int expected_value,
      std::chrono::seconds timeout) {
    using clock_type = std::chrono::steady_clock;

    const auto end_time = clock_type::now() + timeout;
    do {
      auto int_res = num_connections(param);
      if (!int_res) return stdx::make_unexpected(int_res.error());

      if (*int_res == expected_value) return {};

      if (clock_type::now() > end_time) {
        return stdx::make_unexpected(make_error_code(std::errc::timed_out));
      }

      std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
    } while (true);
  }

 private:
  integration_tests::Procs procs_;
  TcpPortPool &port_pool_;

  TempDirectory conf_dir_;

  static const constexpr char router_host_[] = "127.0.0.1";
  std::map<std::tuple<std::string_view, std::string_view>, uint16_t> ports_;

  uint16_t rest_port_;

  IOContext rest_io_ctx_;
  RestClient rest_client_{rest_io_ctx_, "127.0.0.1", rest_port_, rest_user_,
                          rest_pass_};

  static constexpr const char rest_user_[] = "user";
  static constexpr const char rest_pass_[] = "pass";
};

/* test environment.
 *
 * spawns servers for the tests.
 */
class TestEnv : public ::testing::Environment {
 public:
  static constexpr const int kStartedSharedServers = 1;

  void SetUp() override {
    for (auto &s : shared_servers_) {
      if (s == nullptr) {
        s = new SharedServer(port_pool_);
        s->prepare_datadir();
        s->spawn_server();

        if (s->mysqld_failed_to_start()) {
          GTEST_SKIP() << "mysql-server failed to start.";
        }
        s->setup_mysqld_accounts();
      }
    }
  }

  std::array<SharedServer *, kStartedSharedServers> servers() {
    return shared_servers_;
  }

  TcpPortPool &port_pool() { return port_pool_; }

  void TearDown() override {
    for (auto &s : shared_servers_) {
      if (s == nullptr || s->mysqld_failed_to_start()) continue;

      EXPECT_NO_ERROR(s->shutdown());
    }

    for (auto &s : shared_servers_) {
      if (s == nullptr || s->mysqld_failed_to_start()) continue;

      EXPECT_NO_ERROR(s->process_manager().wait_for_exit());
    }

    for (auto &s : shared_servers_) {
      if (s != nullptr) delete s;

      s = nullptr;
    }

    SharedServer::destroy_statics();
  }

 protected:
  TcpPortPool port_pool_;

  std::array<SharedServer *, kStartedSharedServers> shared_servers_{};
};

TestEnv *test_env{};

/* test-suite with shared routers.
 */
class TestWithSharedRouter {
 public:
  static void SetUpTestSuite(TcpPortPool &port_pool,
                             const std::array<SharedServer *, 1> &servers) {
    for (const auto &s : servers) {
      if (s->mysqld_failed_to_start()) GTEST_SKIP();
    }

    if (shared_router_ == nullptr) {
      shared_router_ = new SharedRouter(port_pool);

      SCOPED_TRACE("// spawn router");
      shared_router_->spawn_router(
          SharedRouter::destinations_from_shared_servers(servers));
    }
  }

  static void TearDownTestSuite() {
    delete shared_router_;
    shared_router_ = nullptr;
  }

  static SharedRouter *router() { return shared_router_; }

 protected:
  static SharedRouter *shared_router_;
};

SharedRouter *TestWithSharedRouter::shared_router_ = nullptr;

static stdx::expected<unsigned long, MysqlError> fetch_connection_id(
    MysqlClient &cli) {
  auto query_res = cli.query("SELECT connection_id()");
  if (!query_res) return query_res.get_unexpected();

  // get the first field, of the first row of the first resultset.
  for (const auto &result : *query_res) {
    if (result.field_count() == 0) {
      return stdx::make_unexpected(MysqlError(1, "not a resultset", "HY000"));
    }

    for (auto row : result.rows()) {
      auto connection_id = strtoull(row[0], nullptr, 10);

      return connection_id;
    }
  }

  return stdx::make_unexpected(MysqlError(1, "no rows", "HY000"));
}

class ConnectionTestBase : public RouterComponentTest {
 public:
  static constexpr const size_t kNumServers = 1;

  static void SetUpTestSuite() {
    for (const auto &s : shared_servers()) {
      if (s->mysqld_failed_to_start()) GTEST_SKIP();
    }

    TestWithSharedRouter::SetUpTestSuite(test_env->port_pool(),
                                         shared_servers());
  }

  static void TearDownTestSuite() { TestWithSharedRouter::TearDownTestSuite(); }

  static std::array<SharedServer *, kNumServers> shared_servers() {
    return test_env->servers();
  }

  SharedRouter *shared_router() { return TestWithSharedRouter::router(); }

  void SetUp() override {
    for (auto &s : shared_servers()) {
      // shared_server_ may be null if TestWithSharedServer::SetUpTestSuite
      // threw?
      if (s == nullptr || s->mysqld_failed_to_start()) {
        GTEST_SKIP() << "failed to start mysqld";
      }
    }
  }

  ~ConnectionTestBase() override {
    if (::testing::Test::HasFailure()) {
      shared_router()->process_manager().dump_logs();
    }
  }

 protected:
  const std::string valid_ssl_key_{SSL_TEST_DATA_DIR "/server-key-sha512.pem"};
  const std::string valid_ssl_cert_{SSL_TEST_DATA_DIR
                                    "/server-cert-sha512.pem"};

  const std::string wrong_password_{"wrong_password"};
  const std::string empty_password_{""};
};

class ConnectionTest : public ConnectionTestBase,
                       public ::testing::WithParamInterface<ConnectionParam> {};

TEST_P(ConnectionTest, classic_protocol_wait_timeout) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.query("SET wait_timeout = 1"));

  EXPECT_NO_ERROR(
      shared_router()->wait_for_num_connections(GetParam(), 0, 10s));

  SCOPED_TRACE("// ping after kill");
  {
    auto ping_res = cli.ping();
    ASSERT_ERROR(ping_res);
    EXPECT_THAT(
        ping_res.error().value(),
        AnyOf(2013,  // Lost connection to MySQL server during query
                     //
              4031   // The client was disconnected by the server because of
                     // inactivity. See wait_timeout and interactive_timeout
                     // for configuring this behavior.
              ))
        << ping_res.error();
  }
}

TEST_P(ConnectionTest, classic_protocol_kill_via_select) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto connection_id_res = fetch_connection_id(cli);
  ASSERT_NO_ERROR(connection_id_res);

  auto connection_id = connection_id_res.value();

  SCOPED_TRACE("// killing connection " + std::to_string(connection_id));
  {
    auto kill_res =
        cli.query("KILL CONNECTION " + std::to_string(connection_id));
    ASSERT_ERROR(kill_res);
    EXPECT_EQ(kill_res.error().value(), 1317) << kill_res.error();
    // Query execution was interrupted
  }

  SCOPED_TRACE("// ping after kill");
  {
    auto ping_res = cli.ping();
    ASSERT_ERROR(ping_res);
    EXPECT_EQ(ping_res.error().value(), 2013) << ping_res.error();
    // Lost connection to MySQL server during query
  }
}

TEST_P(ConnectionTest, classic_protocol_list_dbs) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.list_dbs());
}

TEST_P(ConnectionTest, classic_protocol_change_user_native_empty) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto account = SharedServer::native_empty_password_account();

  ASSERT_NO_ERROR(cli.change_user(account.username, account.password, ""));

  {
    auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(
                              account.username + "@localhost", "<NULL>")));
  }
}

TEST_P(ConnectionTest, classic_protocol_change_user_native) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto account = SharedServer::native_password_account();

  ASSERT_NO_ERROR(cli.change_user(account.username, account.password, ""));

  {
    auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(
                              account.username + "@localhost", "<NULL>")));
  }
}

#if !defined(_WIN32)
TEST_P(ConnectionTest, classic_protocol_native_over_socket) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_password_account();
  cli.username(account.username);
  cli.password(account.password);

  if (GetParam().client_ssl_mode == kRequired) {
    cli.set_option(MysqlClient::SslMode(SSL_MODE_REQUIRED));
  }

  auto connect_res = cli.connect(MysqlClient::unix_socket_t{},
                                 shared_router()->socket_path(GetParam()));
  ASSERT_NO_ERROR(connect_res);

  {
    auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(
                              account.username + "@localhost", "<NULL>")));
  }

  {
    auto cmd_res = query_one_result(cli,
                                    "SELECT VARIABLE_VALUE "
                                    " FROM performance_schema.session_status "
                                    "WHERE variable_name LIKE 'Ssl_cipher'");
    ASSERT_NO_ERROR(cmd_res);

    if (GetParam().server_ssl_mode == kPreferred ||
        GetParam().server_ssl_mode == kRequired ||
        (GetParam().server_ssl_mode == kAsClient &&
         (GetParam().client_ssl_mode == kPreferred ||
          GetParam().client_ssl_mode == kRequired))) {
      // some cipher is set
      EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(Not(IsEmpty()))));
    } else {
      // no cipher is set
      EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(IsEmpty())));
    }
  }
}

TEST_P(ConnectionTest, classic_protocol_change_user_native_over_socket) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  if (GetParam().client_ssl_mode == kRequired) {
    cli.set_option(MysqlClient::SslMode(SSL_MODE_REQUIRED));
  }

  auto connect_res = cli.connect(MysqlClient::unix_socket_t{},
                                 shared_router()->socket_path(GetParam()));
  ASSERT_NO_ERROR(connect_res);

  auto account = SharedServer::native_password_account();
  ASSERT_NO_ERROR(cli.change_user(account.username, account.password, ""));

  auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
  ASSERT_NO_ERROR(cmd_res);

  EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(account.username + "@localhost",
                                                "<NULL>")));
}
#endif

TEST_P(ConnectionTest, classic_protocol_change_user_caching_sha2_empty) {
  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset the auth-cache
  }

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto account = SharedServer::caching_sha2_empty_password_account();
  {
    auto change_user_res =
        cli.change_user(account.username, account.password, "");
    ASSERT_NO_ERROR(change_user_res);
  }

  {
    auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(
                              account.username + "@localhost", "<NULL>")));
  }
}

TEST_P(ConnectionTest, classic_protocol_change_user_caching_sha2) {
  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset the auth-cache
  }

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.set_option(MysqlClient::GetServerPublicKey(true));

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto expect_success = !(GetParam().client_ssl_mode == kDisabled &&
                          (GetParam().server_ssl_mode == kRequired ||
                           GetParam().server_ssl_mode == kPreferred));

  auto account = SharedServer::caching_sha2_password_account();
  {
    auto change_user_res =
        cli.change_user(account.username, account.password, "");
    if (expect_success) {
      ASSERT_NO_ERROR(change_user_res);
    } else {
      ASSERT_ERROR(change_user_res);
    }
  }

  if (expect_success) {
    auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(
                              account.username + "@localhost", "<NULL>")));
  }
}

#if !defined(_WIN32)
TEST_P(ConnectionTest, classic_protocol_caching_sha2_over_socket) {
  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset the auth-cache
  }

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_password_account();
  cli.username(account.username);
  cli.password(account.password);

  if (GetParam().client_ssl_mode == kRequired) {
    cli.set_option(MysqlClient::SslMode(SSL_MODE_REQUIRED));
  }

  auto connect_res = cli.connect(MysqlClient::unix_socket_t{},
                                 shared_router()->socket_path(GetParam()));
  ASSERT_NO_ERROR(connect_res);

  auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
  ASSERT_NO_ERROR(cmd_res);

  EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(account.username + "@localhost",
                                                "<NULL>")));
}

TEST_P(ConnectionTest, classic_protocol_change_user_caching_sha2_over_socket) {
  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset the auth-cache
  }

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  if (GetParam().client_ssl_mode == kRequired) {
    cli.set_option(MysqlClient::SslMode(SSL_MODE_REQUIRED));
  }

  auto connect_res = cli.connect(MysqlClient::unix_socket_t{},
                                 shared_router()->socket_path(GetParam()));
  ASSERT_NO_ERROR(connect_res);

  auto account = SharedServer::caching_sha2_password_account();
  ASSERT_NO_ERROR(cli.change_user(account.username, account.password, ""));

  auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
  ASSERT_NO_ERROR(cmd_res);

  EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(account.username + "@localhost",
                                                "<NULL>")));
}
#endif

TEST_P(ConnectionTest, classic_protocol_change_user_caching_sha2_with_schema) {
  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset the auth-cache
  }

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.set_option(MysqlClient::GetServerPublicKey(true));

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("root@localhost", "<NULL>")));
  }

  auto expect_success = !(GetParam().client_ssl_mode == kDisabled &&
                          (GetParam().server_ssl_mode == kRequired ||
                           GetParam().server_ssl_mode == kPreferred));

  auto account = SharedServer::caching_sha2_password_account();
  {
    auto change_user_res =
        cli.change_user(account.username, account.password, "testing");
    if (expect_success) {
      ASSERT_NO_ERROR(change_user_res);
    } else {
      ASSERT_ERROR(change_user_res);
    }
  }

  if (expect_success) {
    auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(
                              account.username + "@localhost", "testing")));
  }
}

TEST_P(ConnectionTest, classic_protocol_change_user_sha256_password_empty) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto account = SharedServer::sha256_empty_password_account();

  ASSERT_NO_ERROR(cli.change_user(account.username, account.password, ""));

  {
    auto query_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(
                                account.username + "@localhost", "<NULL>")));
  }
}

TEST_P(ConnectionTest, classic_protocol_change_user_sha256_password) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// check the server side matches the SSL requirements");
  {
    auto cipher_res = query_one_result(cli, R"(
SELECT VARIABLE_VALUE
  FROM performance_schema.session_status
 WHERE VARIABLE_NAME = 'ssl_cipher')");
    ASSERT_NO_ERROR(cipher_res);

    if (GetParam().server_ssl_mode == kDisabled ||
        (GetParam().server_ssl_mode == kAsClient &&
         GetParam().client_ssl_mode == kDisabled)) {
      EXPECT_THAT(*cipher_res, ElementsAre(ElementsAre("")));
    } else {
      EXPECT_THAT(*cipher_res, ElementsAre(ElementsAre(::testing::Ne(""))));
    }
  }

  {
    auto query_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res,
                ElementsAre(ElementsAre("root@localhost", "<NULL>")));
  }

  auto expect_success = !(GetParam().client_ssl_mode == kDisabled &&
                          (GetParam().server_ssl_mode == kRequired ||
                           GetParam().server_ssl_mode == kPreferred));

  auto account = SharedServer::sha256_password_account();
  {
    auto change_user_res =
        cli.change_user(account.username, account.password, "" /* = schema */);
    if (expect_success) {
      ASSERT_NO_ERROR(change_user_res);
    } else {
      ASSERT_ERROR(change_user_res);
    }
  }

  if (expect_success) {
    auto query_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(
                                account.username + "@localhost", "<NULL>")));
  }
}

TEST_P(ConnectionTest, classic_protocol_statistics) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  EXPECT_NO_ERROR(cli.stat());

  EXPECT_NO_ERROR(cli.stat());
}

// COM_DEBUG -> mysql_dump_debug_info.
TEST_P(ConnectionTest, classic_protocol_debug_succeeds) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::admin_account();
  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  EXPECT_NO_ERROR(cli.dump_debug_info());

  EXPECT_NO_ERROR(cli.dump_debug_info());
}

// COM_DEBUG -> mysql_dump_debug_info.
TEST_P(ConnectionTest, classic_protocol_debug_fails) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();
  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));
  {
    auto res = cli.dump_debug_info();
    ASSERT_ERROR(res);
    EXPECT_EQ(res.error().value(), 1227);  // access denied, you need SUPER
  }

  {
    auto res = cli.dump_debug_info();
    ASSERT_ERROR(res);
    EXPECT_EQ(res.error().value(), 1227);  // access denied, you need SUPER
  }
}

TEST_P(ConnectionTest, classic_protocol_reset_connection) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  EXPECT_NO_ERROR(cli.reset_connection());

  EXPECT_NO_ERROR(cli.reset_connection());
}

TEST_P(ConnectionTest, classic_protocol_query_no_result) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.query("DO 1"));
}

TEST_P(ConnectionTest, classic_protocol_query_with_result) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto query_res = cli.query("SELECT * FROM sys.version");
  ASSERT_NO_ERROR(query_res);
}

TEST_P(ConnectionTest, classic_protocol_query_call) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  //  cli.flags(CLIENT_MULTI_RESULTS);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto query_res = cli.query("CALL testing.multiple_results()");
    ASSERT_NO_ERROR(query_res);

    size_t ndx{};
    for (const auto &res : *query_res) {
      if (ndx == 0) {
        EXPECT_EQ(res.field_count(), 1);
      } else if (ndx == 1) {
        EXPECT_EQ(res.field_count(), 1);
      } else {
        EXPECT_EQ(res.field_count(), 0);
      }
      ++ndx;
    }

    EXPECT_EQ(ndx, 3);
  }
}

TEST_P(ConnectionTest, classic_protocol_query_fail) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.query("DO");
  ASSERT_ERROR(res);
  EXPECT_EQ(res.error().value(), 1064)
      << res.error();  // You have an error in your SQL syntax
}

TEST_P(ConnectionTest, classic_protocol_query_load_data_local_infile) {
  // enable local_infile
  {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    {
      auto query_res = cli.query("SET GLOBAL local_infile=1");
      ASSERT_NO_ERROR(query_res);
    }
  }

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  ASSERT_NO_ERROR(cli.set_option(MysqlClient::LocalInfile(1)));

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto query_res = cli.query("DROP TABLE IF EXISTS testing.t1");
    ASSERT_NO_ERROR(query_res);
  }

  {
    auto query_res = cli.query("CREATE TABLE testing.t1 (word varchar(20))");
    ASSERT_NO_ERROR(query_res);
  }

  {
    auto query_res = cli.query("SET GLOBAL local_infile=1");
    ASSERT_NO_ERROR(query_res);
  }

  {
    auto query_res = cli.query("LOAD DATA LOCAL INFILE '" SSL_TEST_DATA_DIR
                               "/words.dat' "
                               "INTO TABLE testing.t1");
    ASSERT_NO_ERROR(query_res);
  }
}

TEST_P(ConnectionTest,
       classic_protocol_query_load_data_local_infile_no_server_support) {
  // enable local_infile
  {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    ASSERT_NO_ERROR(cli.query("SET GLOBAL local_infile=0"));
  }

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  ASSERT_NO_ERROR(cli.set_option(MysqlClient::LocalInfile(1)));

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto query_res = cli.query("DROP TABLE IF EXISTS testing.t1");
    ASSERT_NO_ERROR(query_res);
  }

  {
    auto query_res = cli.query("CREATE TABLE testing.t1 (word varchar(20))");
    ASSERT_NO_ERROR(query_res);
  }

  {
    auto query_res = cli.query("SET GLOBAL local_infile=1");
    ASSERT_NO_ERROR(query_res);
  }

  {
    auto query_res = cli.query("LOAD DATA LOCAL INFILE '" SSL_TEST_DATA_DIR
                               "/words.dat' "
                               "INTO TABLE testing.t1");
    ASSERT_NO_ERROR(query_res);
  }
}

TEST_P(ConnectionTest, classic_protocol_use_schema_fail) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto query_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res,
                ElementsAre(ElementsAre("root@localhost", "<NULL>")));
  }

  auto res = cli.use_schema("does_not_exist");
  ASSERT_ERROR(res);
  EXPECT_EQ(res.error().value(), 1049) << res.error();  // Unknown Database

  // still the same schema
  {
    auto query_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res,
                ElementsAre(ElementsAre("root@localhost", "<NULL>")));
  }
}

/*
 * empty initial-schema, explicit use-schema
 */
TEST_P(ConnectionTest, classic_protocol_use_schema) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto res = cli.use_schema("sys");
    ASSERT_NO_ERROR(res);
  }

  {
    auto schema_res = query_one_result(cli, "SELECT SCHEMA()");
    ASSERT_NO_ERROR(schema_res);

    EXPECT_THAT(*schema_res, ElementsAre(ElementsAre("sys")));
  }
}

/*
 * check initial-schema is propagated.
 */
TEST_P(ConnectionTest, classic_protocol_initial_schema) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto query_res = query_one_result(cli, "SELECT SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("testing")));
  }

  ASSERT_NO_ERROR(cli.use_schema("sys"));

  {
    auto query_res = query_one_result(cli, "SELECT SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("sys")));
  }
}

/*
 * check non-existant initial schema fails the connect()
 */
TEST_P(ConnectionTest, classic_protocol_initial_schema_fail) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("does_not_exist");

  auto connect_res =
      cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
  ASSERT_ERROR(connect_res);

  EXPECT_EQ(connect_res.error(),
            MysqlError(1049, "Unknown database 'does_not_exist'", "42000"));
}

TEST_P(ConnectionTest, classic_protocol_use_schema_drop_schema) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.query("CREATE SCHEMA droppy"));

  ASSERT_NO_ERROR(cli.use_schema("droppy"));

  ASSERT_NO_ERROR(cli.query("DROP SCHEMA droppy"));

  {
    auto query_res = query_one_result(cli, "SELECT SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("<NULL>"  // SCHEMA()
                                                    )));
  }
}

TEST_P(ConnectionTest, classic_protocol_set_vars) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));
  // + set_option

  // reset, set_option (+ set_option)
  {
    // various kinds of setting session vars
    //
    // (var|SESSION var|@@SESSION.var) (:=|=) string, number, float
    ASSERT_NO_ERROR(cli.query(
        "SET"
        "  @@SeSSion.timestamp = 1.5,"            // timestamp allows floats
        "  SESSION optimizer_trace_offset = -2,"  // trace_offset allows
                                                  // minus-ints
        "  sql_quote_show_create = 0,"  // just for the non-SESSION prefix
        "  unique_checks := ON"         // for the := one
        ));
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/set_option", 1)));
  }

  {
    auto query_res = query_one_result(cli,
                                      "SELECT"
                                      "  @@SESSION.timestamp,"
                                      "  @@SESSION.optimizer_trace_offset,"
                                      "  @@SESSION.sql_quote_show_create,"
                                      "  @@SESSION.unique_checks");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res,
                ElementsAre(ElementsAre("1.500000", "-2", "0", "1")));
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/select", 2),
                                         Pair("statement/sql/set_option", 1)));
  }
}

TEST_P(ConnectionTest, classic_protocol_set_uservar) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.query("SET @my_user_var = 42"));

  {
    auto query_res = query_one_result(cli, "SELECT @my_user_var");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("42")));
  }
}

TEST_P(ConnectionTest, classic_protocol_set_uservar_via_select) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto query_res = query_one_result(cli, "SELECT @my_user_var := 42");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("42")));
  }

  {
    auto query_res = query_one_result(cli, "SELECT @my_user_var");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("42")));
  }
}

/**
 * SHOW WARNINGS
 */
TEST_P(ConnectionTest, classic_protocol_show_warnings) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("DO 0/0");
    ASSERT_NO_ERROR(cmd_res);
  }

  // one warning
  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res,
                ElementsAre(ElementsAre("Warning", "1365", "Division by 0")));
  }

  // LIMIT 1
  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS LIMIT 1");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res,
                ElementsAre(ElementsAre("Warning", "1365", "Division by 0")));
  }

  // LIMIT 0, 1
  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS LIMIT 0, 1");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res,
                ElementsAre(ElementsAre("Warning", "1365", "Division by 0")));
  }

  // LIMIT 0
  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS LIMIT 0");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, IsEmpty());
  }

  // no errors
  {
    auto cmd_res = query_one_result(cli, "SHOW ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, IsEmpty());
  }

  // LIMIT ... no number.
  {
    auto cmd_res = cli.query("SHOW WARNINGS LIMIT");
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1064);  // parse error
  }
}

/**
 * SHOW WARNINGS + reset-connection.
 *
 * after a reset-connection the cached warnings should be empty.
 */
TEST_P(ConnectionTest, classic_protocol_show_warnings_and_reset) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("DO 0/0,");
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1064) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                ::testing::StartsWith("You have an error in your SQL"))
        << cmd_res.error();
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res,
                ElementsAre(ElementsAre(
                    "Error", "1064", ::testing::StartsWith("You have an "))));
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res,
                ElementsAre(ElementsAre(
                    "Error", "1064", ::testing::StartsWith("You have an "))));
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW COUNT(*) WARNINGS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("1")));
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW COUNT(*) ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("1")));
  }

  // reset.
  ASSERT_NO_ERROR(cli.reset_connection());

  // warnings should be gone now.
  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ::testing::IsEmpty());
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ::testing::IsEmpty());
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW COUNT(*) WARNINGS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("0")));
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW COUNT(*) ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("0")));
  }
}

/**
 * SHOW WARNINGS + change-user.
 *
 * after a change-user the cached warnings should be empty.
 */
TEST_P(ConnectionTest, classic_protocol_show_warnings_and_change_user) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto account = SharedServer::caching_sha2_empty_password_account();

  {
    auto cmd_res = cli.query("CREATE TABLE testing.tbl (ID INT)");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto cmd_res =
        cli.query("INSERT INTO testing.tbl SELECT 0/0 + _utf8'' + 0/0");
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1365) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                ::testing::StartsWith("Division by 0"))
        << cmd_res.error();
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(
        *cmd_res,
        ElementsAre(ElementsAre("Warning", "3719",
                                ::testing::StartsWith("'utf8' is currently")),
                    ElementsAre("Error", "1365",
                                ::testing::StartsWith("Division by 0"))));
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res,
                ElementsAre(ElementsAre(
                    "Error", "1365", ::testing::StartsWith("Division by 0"))));
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW COUNT(*) WARNINGS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("2")));
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW COUNT(*) ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("1")));
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(
        *events_res,
        ElementsAre(Pair("statement/sql/create_table", 1),
                    Pair("statement/sql/insert_select", 1),
                    Pair("statement/sql/select", 2),  // SHOW COUNT(*) ...
                    Pair("statement/sql/show_errors", 1),
                    Pair("statement/sql/show_warnings", 1)));
  }

  // switch to another user.
  ASSERT_NO_ERROR(cli.change_user(account.username, account.password, ""));

  // warnings should be gone now.
  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ::testing::IsEmpty());
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ::testing::IsEmpty());
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW COUNT(*) WARNINGS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("0")));
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW COUNT(*) ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("0")));
  }

  {
    auto cmd_res = cli.query("DROP TABLE testing.tbl");
    ASSERT_NO_ERROR(cmd_res);
  }
}

/**
 * FR2.2: SHOW WARNINGS
 */
TEST_P(ConnectionTest,
       classic_protocol_show_warnings_without_server_connection) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.query("DO 0/0"));

  for (auto &s : shared_servers()) {
    s->close_all_connections();
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS");

    // the connection wasn't in the pool and got killed.
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 2013) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                ::testing::StartsWith("Lost connection to MySQL server"))
        << cmd_res.error();
  }
}

/**
 * SHOW ERRORS
 */
TEST_P(ConnectionTest, classic_protocol_show_errors_after_connect) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = query_one_result(cli, "SHOW ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ::testing::IsEmpty());
  }
}

TEST_P(ConnectionTest, classic_protocol_set_names) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // set-trackers, reset, set-trackers, set-names
  {
    auto cmd_res = cli.query("SET NAMES 'utf8mb4'");
    ASSERT_NO_ERROR(cmd_res);
  }

  // reset, set-trackers, select
  {
    auto cmd_res = query_one_result(cli, R"(SELECT
@@session.character_set_client,
@@session.character_set_connection,
@@session.character_set_results
)");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res,
                ElementsAre(ElementsAre("utf8mb4", "utf8mb4", "utf8mb4")));
  }

  // reset, set-trackers
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/select", 1),
                                         Pair("statement/sql/set_option", 1)));
  }
}

/**
 * FR5.2: LOCK TABLES
 */
TEST_P(ConnectionTest, classic_protocol_lock_tables_and_reset) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));
  // set-trackers

  // reset, set-trackers
  {
    auto query_res = cli.query("CREATE TABLE testing.tbl (ID INT)");
    ASSERT_NO_ERROR(query_res);
  }

  // reset, set-trackers
  {
    // LOCK TABLES disables sharing.
    auto cmd_res = cli.query("LOCK TABLES testing.tbl READ");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto cmd_res = query_one_result(cli, "SELECT * FROM testing.tbl");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, IsEmpty());
  }

  // reset-connection enables sharing again.
  ASSERT_NO_ERROR(cli.reset_connection());

  // reset, set-trackers
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                ElementsAre(Pair("statement/com/Reset Connection", 1),
                            Pair("statement/sql/create_table", 1),
                            Pair("statement/sql/lock_tables", 1),
                            Pair("statement/sql/select", 1)));
  }

  // reset, set-trackers
  {
    auto cmd_res = query_one_result(cli, "SELECT * FROM testing.tbl");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, IsEmpty());
  }

  // reset, set-trackers
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                ElementsAre(Pair("statement/com/Reset Connection", 1),
                            Pair("statement/sql/create_table", 1),
                            Pair("statement/sql/lock_tables", 1),
                            Pair("statement/sql/select", 3)));
  }

  // cleanup
  {
    auto query_res = cli.query("DROP TABLE testing.tbl");
    ASSERT_NO_ERROR(query_res);
  }
}

TEST_P(ConnectionTest, classic_protocol_prepare_fail) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.prepare("SEL ?");
  ASSERT_ERROR(res);
  EXPECT_EQ(res.error().value(), 1064) << res.error();  // Syntax Error

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Prepare", 1)));
  }
}

/**
 * FR6.3: successful prepared statement: disable sharing until reset-connection
 */
TEST_P(ConnectionTest, classic_protocol_prepare_execute) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// prepare");
  auto res = cli.prepare("SELECT ?");
  ASSERT_NO_ERROR(res);

  auto stmt = std::move(res.value());

  SCOPED_TRACE("// bind_params");
  std::array<MYSQL_BIND, 1> params{
      NullParam{},
  };
  ASSERT_NO_ERROR(stmt.bind_params(params));

  SCOPED_TRACE(
      "// execute again to trigger a StmtExecute with new-params-bound = 1.");
  {
    auto exec_res = stmt.execute();
    ASSERT_NO_ERROR(exec_res);

    for ([[maybe_unused]] auto res : *exec_res) {
      // drain the resultsets.
    }
  }

  SCOPED_TRACE(
      "// execute again to trigger a StmtExecute with new-params-bound = 0.");
  {
    auto exec_res = stmt.execute();
    ASSERT_NO_ERROR(exec_res);

    for ([[maybe_unused]] auto res : *exec_res) {
      // drain the resultsets.
    }
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Execute", 2),
                                         Pair("statement/com/Prepare", 1)));
  }

  ASSERT_NO_ERROR(cli.reset_connection());

  // share again.
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                ElementsAre(Pair("statement/com/Execute", 2),
                            Pair("statement/com/Prepare", 1),
                            // explicit
                            Pair("statement/com/Reset Connection", 1),
                            // events
                            Pair("statement/sql/select", 1)));
  }
}

TEST_P(ConnectionTest, classic_protocol_prepare_execute_fetch) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.prepare("SELECT ?");
  ASSERT_NO_ERROR(res);

  auto stmt = std::move(res.value());

  // create a read-only cursor force a COM_STMT_FETCH
  EXPECT_NO_ERROR(stmt.set_attr(MysqlClient::PreparedStatement::CursorType{1}));
  EXPECT_NO_ERROR(
      stmt.set_attr(MysqlClient::PreparedStatement::PrefetchRows{1}));

  int one{1};
  std::array<MYSQL_BIND, 1> params{
      IntegerParam{&one},
  };
  auto bind_res = stmt.bind_params(params);
  EXPECT_TRUE(bind_res) << bind_res.error();

  auto exec_res = stmt.execute();
  EXPECT_TRUE(exec_res) << exec_res.error();

  // may contain multi-resultset
  size_t results{0};
  size_t rows{0};
  for (auto result : exec_res.value()) {
    ++results;
    if (result.field_count() > 0) {
      int count;
      std::array<MYSQL_BIND, 1> fields{IntegerParam{&count}};

      result.bind_result(fields);
      for (const auto fetch_status : result.rows()) {
        EXPECT_EQ(fetch_status.status(), 0);
        ++rows;
      }
    }
  }
  EXPECT_EQ(results, 1);
  EXPECT_EQ(rows, 1);

  // share again.
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Execute", 1),
                                         Pair("statement/com/Fetch", 2),
                                         Pair("statement/com/Prepare", 1)));
  }
}

TEST_P(ConnectionTest, classic_protocol_prepare_append_data_execute) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.prepare("SELECT ?");
  ASSERT_NO_ERROR(res);

  auto stmt = std::move(res.value());

  std::string one{"1"};
  std::array<MYSQL_BIND, 1> params{
      StringParam{one},
  };
  {
    auto bind_res = stmt.bind_params(params);
    ASSERT_NO_ERROR(bind_res) << bind_res.error();
  }

  // a..b..c..d

  // longdata: c_string with len
  {
    auto append_res = stmt.append_param_data(0, "a", 1);
    ASSERT_NO_ERROR(append_res);
  }

  // longdata: string_view
  {
    auto append_res = stmt.append_param_data(0, "b"sv);
    ASSERT_NO_ERROR(append_res);
  }

  // longdata: string_view from std::string
  {
    auto append_res = stmt.append_param_data(0, std::string("c"));
    ASSERT_NO_ERROR(append_res);
  }

  // longdata: string_view from c-string
  {
    auto append_res = stmt.append_param_data(0, "d");
    ASSERT_NO_ERROR(append_res);
  }

  {
    auto exec_res = stmt.execute();
    ASSERT_NO_ERROR(exec_res);

    // may contain multi-resultset
    size_t results{0};
    size_t rows{0};
    for (auto result : exec_res.value()) {
      ++results;
      if (result.field_count() > 0) {
        std::string data;
        data.resize(16);                // resize to alloca space
        unsigned long data_actual_len;  // actual length
        std::array<MYSQL_BIND, 1> fields{StringParam{data, &data_actual_len}};

        result.bind_result(fields);
        for (const auto fetch_status [[maybe_unused]] : result.rows()) {
          EXPECT_EQ(data_actual_len, 4);
          EXPECT_EQ(data.size(), 16);

          data.resize(std::min(static_cast<size_t>(data_actual_len),
                               data.size()));  // only shrink

          EXPECT_EQ(data, "abcd");
          ++rows;
        }
      }
    }
    EXPECT_EQ(results, 1);
    EXPECT_EQ(rows, 1);
  }

  // execute again
  {
    auto exec_res = stmt.execute();
    ASSERT_NO_ERROR(exec_res);
  }
}

TEST_P(ConnectionTest, classic_protocol_prepare_append_data_reset_execute) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.prepare("SELECT ?");
  ASSERT_NO_ERROR(res);

  auto stmt = std::move(res.value());

  std::string one{"1"};
  std::array<MYSQL_BIND, 1> params{
      StringParam{one},
  };
  {
    auto bind_res = stmt.bind_params(params);
    EXPECT_TRUE(bind_res) << bind_res.error();
  }

  // a..b..c..d

  // longdata: c_string with len
  {
    auto append_res = stmt.append_param_data(0, "a", 1);
    EXPECT_TRUE(append_res) << append_res.error();
  }

  // longdata: string_view
  {
    auto append_res = stmt.append_param_data(0, "b"sv);
    EXPECT_TRUE(append_res) << append_res.error();
  }

  // longdata: string_view from std::string
  {
    auto append_res = stmt.append_param_data(0, std::string("c"));
    EXPECT_TRUE(append_res) << append_res.error();
  }

  // longdata: string_view from c-string
  {
    auto append_res = stmt.append_param_data(0, "d");
    EXPECT_TRUE(append_res) << append_res.error();
  }

  // reset the append data and use the 'one' instead.
  {
    auto reset_res = stmt.reset();
    EXPECT_TRUE(reset_res) << reset_res.error();
  }

  {
    auto exec_res = stmt.execute();
    EXPECT_TRUE(exec_res) << exec_res.error();

    // may contain multi-resultset
    size_t results{0};
    size_t rows{0};
    for (auto result : exec_res.value()) {
      ++results;
      if (result.field_count() > 0) {
        std::string data;
        data.resize(16);                // resize to alloca space
        unsigned long data_actual_len;  // actual length
        std::array<MYSQL_BIND, 1> fields{StringParam{data, &data_actual_len}};

        result.bind_result(fields);
        for (const auto fetch_status [[maybe_unused]] : result.rows()) {
          EXPECT_EQ(data_actual_len, 1);
          EXPECT_EQ(data.size(), 16);

          data.resize(std::min(static_cast<size_t>(data_actual_len),
                               data.size()));  // only shrink

          // the 'one' is used.
          EXPECT_EQ(data, "1");
          ++rows;
        }
      }
    }
    EXPECT_EQ(results, 1);
    EXPECT_EQ(rows, 1);
  }

  // execute again
  {
    auto exec_res = stmt.execute();
    EXPECT_TRUE(exec_res) << exec_res.error();
  }
}

/*
 * stmt-execute -> ok
 */
TEST_P(ConnectionTest, classic_protocol_prepare_execute_no_result) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.prepare("DO ?");
  ASSERT_NO_ERROR(res);

  // leave the statement open across the reset_connection to ensure it isn't
  // closed from the client side.
  auto stmt = std::move(*res);

  std::array<MYSQL_BIND, 1> params{
      NullParam{},
  };

  ASSERT_NO_ERROR(stmt.bind_params(params));

  auto exec_res = stmt.execute();
  ASSERT_NO_ERROR(exec_res);

  for ([[maybe_unused]] auto res : *exec_res) {
    // drain the resultsets.
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Execute", 1),
                                         Pair("statement/com/Prepare", 1)));
  }

  SCOPED_TRACE("// reset the connection to allow sharing again.");
  ASSERT_NO_ERROR(cli.reset_connection());

  // share again.
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                ElementsAre(Pair("statement/com/Execute", 1),
                            Pair("statement/com/Prepare", 1),
                            // explicit
                            Pair("statement/com/Reset Connection", 1),
                            // events
                            Pair("statement/sql/select", 1)));
  }
}

/*
 * stmt-execute -> stored-procedure
 */
TEST_P(ConnectionTest, classic_protocol_prepare_execute_call) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.prepare("CALL testing.multiple_results()");
  ASSERT_NO_ERROR(res);

  // leave the statement open across the reset_connection to ensure it isn't
  // closed from the client side.
  auto stmt = std::move(*res);

  auto exec_res = stmt.execute();
  ASSERT_NO_ERROR(exec_res);

  size_t num_res{};
  for ([[maybe_unused]] auto res : *exec_res) {
    // drain the resultsets.
    ++num_res;
  }
  // select
  // select
  // call
  EXPECT_EQ(num_res, 3);

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Execute", 1),
                                         Pair("statement/com/Prepare", 1),
                                         Pair("statement/sp/stmt", 2)));
  }

  SCOPED_TRACE("// reset the connection to allow sharing again.");
  ASSERT_NO_ERROR(cli.reset_connection());

  // share again.
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                ElementsAre(Pair("statement/com/Execute", 1),
                            Pair("statement/com/Prepare", 1),
                            // explicit
                            Pair("statement/com/Reset Connection", 1),
                            Pair("statement/sp/stmt", 2),
                            // events
                            Pair("statement/sql/select", 1)));
  }
}

TEST_P(ConnectionTest, classic_protocol_prepare_execute_missing_bind_param) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.prepare("SELECT ?");
  ASSERT_NO_ERROR(res);

  auto stmt = std::move(res.value());

  // no bind.

  auto exec_res = stmt.execute();
  ASSERT_ERROR(exec_res);
  EXPECT_EQ(exec_res.error().value(), 2031) << exec_res.error();
  // No data supplied for parameters in prepared statement
}

TEST_P(ConnectionTest, classic_protocol_prepare_reset) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.prepare("SELECT ?");
  ASSERT_NO_ERROR(res);

  auto stmt = std::move(res.value());

  ASSERT_NO_ERROR(stmt.reset());
}

TEST_P(ConnectionTest, classic_protocol_set_option) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  EXPECT_NO_ERROR(cli.set_server_option(MYSQL_OPTION_MULTI_STATEMENTS_ON));
}

TEST_P(ConnectionTest, classic_protocol_set_option_fails) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res =
        cli.set_server_option(static_cast<enum_mysql_set_option>(255));
    ASSERT_ERROR(cmd_res);

    EXPECT_EQ(cmd_res.error().value(), 1047);  // unknown command.
  }
}

TEST_P(ConnectionTest, classic_protocol_binlog_dump) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // source_binlog_checksum needs to be set to what the server is, otherwise it
  // will fail at binlog_dump();

  ASSERT_NO_ERROR(
      cli.query("SET @source_binlog_checksum=@@global.binlog_checksum"));

  // purge the logs
  ASSERT_NO_ERROR(cli.query("RESET BINARY LOGS AND GTIDS"));

  {
    MYSQL_RPL rpl{};

    rpl.start_position = 4;
    rpl.server_id = 0;
    rpl.flags = 1 << 0 /* NON_BLOCK */;

    ASSERT_NO_ERROR(cli.binlog_dump(rpl));

    do {
      ASSERT_NO_ERROR(cli.binlog_fetch(rpl));
    } while (rpl.size != 0);
  }

  // server closes the connection and therefore the client connection should be
  // closed too.
  {
    auto cmd_res = cli.ping();
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 2013) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                ::testing::StartsWith("Lost connection to MySQL server"))
        << cmd_res.error();
  }
}

TEST_P(ConnectionTest, classic_protocol_binlog_dump_fail_no_checksum) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));
  {
    MYSQL_RPL rpl{};

    rpl.start_position = 4;
    rpl.server_id = 0;
    rpl.flags = 1 << 0 /* NON_BLOCK */;

    ASSERT_NO_ERROR(cli.binlog_dump(rpl));

    ASSERT_NO_ERROR(cli.binlog_fetch(rpl));

    {
      auto res = cli.binlog_fetch(rpl);
      ASSERT_ERROR(res);
      EXPECT_EQ(res.error().value(), 1236) << res.error();
      EXPECT_THAT(res.error().message(),
                  AnyOf(StartsWith("Replica can not handle"),
                        StartsWith("Slave can not handle")))
          << res.error();
    }
  }

  // server closes the connection and therefore the client connection should be
  // closed too.
  {
    auto cmd_res = cli.ping();
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 2013) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                ::testing::StartsWith("Lost connection to MySQL server"))
        << cmd_res.error();
  }
}

/**
 * COM_BINLOG_DUMP always closes the connection when it finishes.
 *
 * no sharing.
 */
TEST_P(ConnectionTest, classic_protocol_binlog_dump_gtid) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // source_binlog_checksum needs to be set to what the server is, otherwise it
  // will fail at binlog_dump();

  ASSERT_NO_ERROR(
      cli.query("SET @source_binlog_checksum=@@global.binlog_checksum"));

  {
    MYSQL_RPL rpl{};

    rpl.start_position = 4;
    rpl.server_id = 0;
    rpl.flags = MYSQL_RPL_GTID | (1 << 0);

    ASSERT_NO_ERROR(cli.binlog_dump(rpl));

    do {
      ASSERT_NO_ERROR(cli.binlog_fetch(rpl));
    } while (rpl.size != 0);
  }

  // server closes the connection and therefore the client connection should be
  // closed too.
  {
    auto cmd_res = cli.ping();
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 2013) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                ::testing::StartsWith("Lost connection to MySQL server"))
        << cmd_res.error();
  }
}

// source_binlog_checksum needs to be set to what the server is, otherwise it
// will fail at binlog_dump();
TEST_P(ConnectionTest, classic_protocol_binlog_dump_gtid_fail_no_checksum) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    MYSQL_RPL rpl{};

    rpl.start_position = 4;
    rpl.server_id = 0;
    rpl.flags = MYSQL_RPL_GTID | (1 << 0);

    ASSERT_NO_ERROR(cli.binlog_dump(rpl));

    // format-description event
    ASSERT_NO_ERROR(cli.binlog_fetch(rpl));

    {
      auto res = cli.binlog_fetch(rpl);
      ASSERT_ERROR(res);
      EXPECT_EQ(res.error().value(), 1236) << res.error();
      EXPECT_THAT(res.error().message(),
                  AnyOf(StartsWith("Replica can not handle"),
                        StartsWith("Slave can not handle")))
          << res.error();
    }
  }

  // should fail as the server closed the connection on us.
  {
    auto cmd_res = cli.ping();
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 2013) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                ::testing::StartsWith("Lost connection to MySQL server"))
        << cmd_res.error();
  }
}

TEST_P(ConnectionTest, classic_protocol_binlog_dump_gtid_fail_wrong_position) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  MYSQL_RPL rpl{};

  rpl.start_position = 0;
  rpl.server_id = 0;
  rpl.flags = MYSQL_RPL_GTID | (1 << 0);

  ASSERT_NO_ERROR(cli.binlog_dump(rpl));

  {
    auto res = cli.binlog_fetch(rpl);
    ASSERT_ERROR(res);
    EXPECT_EQ(res.error().value(), 1236) << res.error();
    EXPECT_THAT(res.error().message(),
                AnyOf(StartsWith("Client requested master to start replication "
                                 "from position < 4"),
                      StartsWith("Client requested source to start replication "
                                 "from position < 4")))
        << res.error();
  }

  // should fail as the server closed the connection on us.
  {
    auto cmd_res = cli.ping();
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 2013) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                ::testing::StartsWith("Lost connection to MySQL server"))
        << cmd_res.error();
  }
}

/**
 * Check, caching-sha2-password over plaintext works.
 *
 * when the client connects with ssl_mode=DISABLED and uses
 * caching-sha2-password the first time, it will fail "Auth requires secure
 * connections".
 *
 * After successful login of another client that uses SSL, a plaintext client
 * should be able to login too.
 */
TEST_P(ConnectionTest, classic_protocol_caching_sha2_over_plaintext_with_pass) {
  if (GetParam().client_ssl_mode == kRequired) {
    GTEST_SKIP() << "test requires plaintext connection.";
  }

  auto account = SharedServer::caching_sha2_single_use_password_account();

  std::string username(account.username);
  std::string password(account.password);

  for (auto &s : shared_servers()) {
    auto cli_res = s->admin_cli();
    ASSERT_NO_ERROR(cli_res);

    auto admin_cli = std::move(cli_res.value());

    s->create_account(admin_cli, account);
  }

  // remove the account at the end of the test again.
  Scope_guard drop_at_end([account]() {
    for (auto &s : shared_servers()) {
      auto cli_res = s->admin_cli();
      ASSERT_NO_ERROR(cli_res);

      auto admin_cli = std::move(cli_res.value());

      s->drop_account(admin_cli, account);
    }
  });

  SCOPED_TRACE("// caching sha2 password requires secure connection");
  {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));

    cli.username(username);
    cli.password(password);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    ASSERT_ERROR(connect_res);
    EXPECT_EQ(connect_res.error().value(), 2061) << connect_res.error();
    // Authentication plugin 'caching_sha2_password' reported error:
    // Authentication requires secure connection.
  }

  SCOPED_TRACE(
      "// caching sha2 password over secure connection should succeed");
  {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_PREFERRED));

    cli.username(username);
    cli.password(password);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (GetParam().client_ssl_mode == kDisabled) {
      // the client side is not encrypted, but caching-sha2 wants SSL.
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 2061) << connect_res.error();
      // Authentication plugin 'caching_sha2_password' reported error:
      // Authentication requires secure connection.
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  }

  SCOPED_TRACE(
      "// caching sha2 password over plain connection should succeed after one "
      "successful auth");
  if (GetParam().client_ssl_mode != kDisabled) {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_PREFERRED));

    cli.username(username);
    cli.password(password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));
  }
}

//
// sha256_password
//

/**
 * Check, sha256-password over plaintext works with get-server-key.
 */
TEST_P(ConnectionTest,
       classic_protocol_sha256_password_over_plaintext_with_get_server_key) {
  if (GetParam().client_ssl_mode == kRequired) {
    GTEST_SKIP() << "test requires plaintext connection.";
  }

  bool expect_success =
#if OPENSSL_VERSION_NUMBER < ROUTER_OPENSSL_VERSION(1, 0, 2)
      (GetParam().client_ssl_mode == kDisabled &&
       (GetParam().server_ssl_mode == kDisabled ||
        GetParam().server_ssl_mode == kAsClient)) ||
      (GetParam().client_ssl_mode == kPassthrough) ||
      (GetParam().client_ssl_mode == kPreferred &&
       (GetParam().server_ssl_mode == kDisabled ||
        GetParam().server_ssl_mode == kAsClient));
#else
      !(GetParam().client_ssl_mode == kDisabled &&
        (GetParam().server_ssl_mode == kRequired ||
         GetParam().server_ssl_mode == kPreferred));
#endif

  auto account = SharedServer::sha256_password_account();

  std::string username(account.username);
  std::string password(account.password);

  SCOPED_TRACE("// first connection");
  {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    cli.username(username);
    cli.password(password);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (!expect_success) {
      // server will treat the public-key-request as wrong password.
      ASSERT_ERROR(connect_res);
    } else {
      ASSERT_NO_ERROR(connect_res);

      ASSERT_NO_ERROR(cli.ping());
    }
  }

  SCOPED_TRACE("// second connection");
  if (expect_success) {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    cli.username(username);
    cli.password(password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    ASSERT_NO_ERROR(cli.ping());
  }
}

/**
 * Check, sha256-empty-password over plaintext works with get-server-key.
 *
 * as empty passwords are not encrypted, it also works of the router works
 * with client_ssl_mode=DISABLED
 */
TEST_P(
    ConnectionTest,
    classic_protocol_sha256_password_empty_over_plaintext_with_get_server_key) {
  if (GetParam().client_ssl_mode == kRequired) {
    GTEST_SKIP() << "test requires plaintext connection.";
  }

  auto account = SharedServer::sha256_empty_password_account();

  std::string username(account.username);
  std::string password(account.password);

  SCOPED_TRACE("// first connection");
  {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    cli.username(username);
    cli.password(password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    ASSERT_NO_ERROR(cli.ping());
  }

  SCOPED_TRACE("// second connection");
  {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    cli.username(username);
    cli.password(password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    ASSERT_NO_ERROR(cli.ping());
  }
}

/**
 * Check, caching-sha2-password over plaintext works with get-server-key.
 */
TEST_P(
    ConnectionTest,
    classic_protocol_caching_sha2_password_over_plaintext_with_get_server_key) {
  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset the auth-cache
  }

  if (GetParam().client_ssl_mode == kRequired) {
    GTEST_SKIP() << "test requires plaintext connection.";
  }

  bool expect_success =
#if OPENSSL_VERSION_NUMBER < ROUTER_OPENSSL_VERSION(1, 0, 2)
      // DISABLED/DISABLED will get the public-key from the server.
      //
      // other modes that should fail, will fail as the router can't get the
      // public-key from the ssl-certs in openssl 1.0.1
      (GetParam().client_ssl_mode == kDisabled &&
       (GetParam().server_ssl_mode == kDisabled ||
        GetParam().server_ssl_mode == kAsClient)) ||
      (GetParam().client_ssl_mode == kPassthrough) ||
      (GetParam().client_ssl_mode == kPreferred &&
       (GetParam().server_ssl_mode == kDisabled ||
        GetParam().server_ssl_mode == kAsClient));
#else
      !(GetParam().client_ssl_mode == kDisabled &&
        (GetParam().server_ssl_mode == kRequired ||
         GetParam().server_ssl_mode == kPreferred));
#endif

  auto account = SharedServer::caching_sha2_password_account();

  std::string username(account.username);
  std::string password(account.password);

  SCOPED_TRACE("// first connection");
  {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.set_option(MysqlClient::GetServerPublicKey(true));
    ASSERT_NO_ERROR(cli.set_option(MysqlClient::ConnectAttributeAdd(
        "testname",
        "caching_sha2_password_over_plaintext_with_get_server_key")));

    cli.username(username);
    cli.password(password);

    // client_ssl_mode = DISABLED
    //
    // works if the auth is using the "cached" part (a earlier successful auth
    // happened)

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (!expect_success) {
      // - client will request a public-key
      // - router has no public key as "client_ssl_mode = DISABLED"
      // - client will ask for server's public-key but the server will treat
      // the request as "password is 0x02" and fail.
      ASSERT_ERROR(connect_res);
    } else {
      ASSERT_NO_ERROR(connect_res);

      ASSERT_NO_ERROR(cli.ping());
    }
  }

  SCOPED_TRACE("// populate the auth-cache on the server");
  for (const auto &s : shared_servers()) {
    MysqlClient cli;

    cli.username(username);
    cli.password(password);

    ASSERT_NO_ERROR(cli.connect(s->server_host(), s->server_port()));
  }

  SCOPED_TRACE("// second connection");
  {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    cli.username(username);
    cli.password(password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    ASSERT_NO_ERROR(cli.ping());
  }
}

/**
 * Check, empty caching-sha2-password over plaintext works with get-server-key.
 */
TEST_P(
    ConnectionTest,
    classic_protocol_caching_sha2_password_empty_over_plaintext_with_get_server_key) {
  if (GetParam().client_ssl_mode == kRequired) {
    GTEST_SKIP() << "test requires plaintext connection.";
  }

  auto account = SharedServer::caching_sha2_empty_password_account();

  std::string username(account.username);
  std::string password(account.password);

  SCOPED_TRACE("// first connection");
  {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    cli.username(username);
    cli.password(password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    ASSERT_NO_ERROR(cli.ping());
  }

  SCOPED_TRACE("// second connection");
  {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    cli.username(username);
    cli.password(password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    ASSERT_NO_ERROR(cli.ping());
  }
}

/**
 * check unknown command handling.
 *
 * after a unknown command a error packet should be returned.
 */
TEST_P(ConnectionTest, classic_protocol_unknown_command) {
  if (GetParam().client_ssl_mode == kRequired) {
    GTEST_SKIP() << "test requires plaintext connection.";
  }

  SCOPED_TRACE("// connecting to server");

  MysqlClient cli;

  // disable SSL as the test wants to inject an invalid command directly.
  cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// send an invalid command");
  {
    std::array<uint8_t, 5> invalid_packet{0x01, 0x00, 0x00, 0x00, 0xff};

    auto write_res = net::impl::socket::write(
        cli.native_handle(), invalid_packet.data(), invalid_packet.size());
    ASSERT_NO_ERROR(write_res);
    EXPECT_EQ(*write_res, 5);
  }

  SCOPED_TRACE("// check that an error packet is returned");
  {
    std::vector<uint8_t> read_buf;
    {
      read_buf.resize(1024);

      auto read_res = net::impl::socket::read(cli.native_handle(),
                                              read_buf.data(), read_buf.size());

      ASSERT_NO_ERROR(read_res);
      read_buf.resize(*read_res);
    }

    auto decode_res = classic_protocol::decode<classic_protocol::frame::Frame<
        classic_protocol::message::server::Error>>(
        net::buffer(read_buf), {CLIENT_TRANSACTIONS | CLIENT_PROTOCOL_41});
    ASSERT_NO_ERROR(decode_res);

    auto msg = decode_res->second.payload();

    EXPECT_EQ(msg.error_code(), 1047);
    EXPECT_EQ(msg.message(), "Unknown command 255");
    EXPECT_EQ(msg.sql_state(), "HY000");
  }

  SCOPED_TRACE(
      "// after an invalid command, normal commands should still work.");
  ASSERT_NO_ERROR(cli.ping());
}

/**
 * check max-connection errors are properly forwarded to the client.
 *
 * There are two scenarios:
 *
 * 1. if there is NO SUPER connection, then max-connection fails after
 *    authentication as a SUPER connection would still be allowed.
 * 2. If there is a SUPER connection, then max-connection fails at greeting
 *    as neither SUPER nor normal connections are allowed.
 */
TEST_P(ConnectionTest, classic_protocol_server_greeting_error) {
  SCOPED_TRACE("// set max-connections = 1, globally");
  {
    MysqlClient admin_cli;

    auto admin_account = SharedServer::admin_account();

    admin_cli.username(admin_account.username);
    admin_cli.password(admin_account.password);

    ASSERT_NO_ERROR(admin_cli.connect(shared_router()->host(),
                                      shared_router()->port(GetParam())));

    ASSERT_NO_ERROR(admin_cli.query("SET GLOBAL max_connections = 1"));
  }

  Scope_guard restore_at_end{[this]() {
    auto reset_globals = [this]() -> stdx::expected<void, MysqlError> {
      auto admin_account = SharedServer::admin_account();

      MysqlClient admin_cli;

      admin_cli.username(admin_account.username);
      admin_cli.password(admin_account.password);

      auto connect_res = admin_cli.connect(shared_router()->host(),
                                           shared_router()->port(GetParam()));
      if (!connect_res) return stdx::make_unexpected(connect_res.error());

      auto query_res = admin_cli.query("SET GLOBAL max_connections = DEFAULT");
      if (!query_res) return stdx::make_unexpected(query_res.error());

      return {};
    };

    // it may take a while until the last connection of the test is closed
    // before this admin connection can be opened to reset the globals again.
    auto end_time = std::chrono::steady_clock::now() + 1s;

    while (true) {
      auto reset_res = reset_globals();
      if (!reset_res) {
        auto ec = reset_res.error();

        // wait a bit until all connections are closed.
        //
        // 1040 is Too many connections.
        if (ec.value() == 1040 && std::chrono::steady_clock::now() < end_time) {
          std::this_thread::sleep_for(20ms);
          continue;
        }
      }

      ASSERT_NO_ERROR(reset_res);
      break;
    }
  }};

  {
    SCOPED_TRACE("// connecting to server");

    auto account = SharedServer::native_password_account();

    MysqlClient cli;  // keep it open
    {
      cli.username(account.username);
      cli.password(account.password);

      auto connect_res = cli.connect(shared_router()->host(),
                                     shared_router()->port(GetParam()));
      ASSERT_NO_ERROR(connect_res);
    }

    // fails at auth as the a SUPER account could still connect
    SCOPED_TRACE("// connecting to server");
    {
      MysqlClient cli2;

      cli2.username(account.username);
      cli2.password(account.password);

      auto connect_res = cli2.connect(shared_router()->host(),
                                      shared_router()->port(GetParam()));
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1040)  // max-connections reached
          << connect_res.error();
    }

    SCOPED_TRACE("// connecting to server as SUPER user");
    MysqlClient cli_super;  // keep it open
    {
      auto admin_account = SharedServer::admin_account();

      cli_super.username(admin_account.username);
      cli_super.password(admin_account.password);

      auto connect_res = cli_super.connect(shared_router()->host(),
                                           shared_router()->port(GetParam()));
      ASSERT_NO_ERROR(connect_res);
    }

    // fails at greeting, as one SUPER-connection and ${max_connections}
    // user-connections are connected.
    SCOPED_TRACE("// connecting to server");
    {
      MysqlClient cli2;

      cli2.username(account.username);
      cli2.password(account.password);

      auto connect_res = cli2.connect(shared_router()->host(),
                                      shared_router()->port(GetParam()));
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1040)  // max-connections reached
          << connect_res.error();
    }
  }

  SCOPED_TRACE("// reset again");  // before the Scope_guard fires.
}

/**
 * check that server doesn't report "Aborted Clients".
 */
TEST_P(ConnectionTest, classic_protocol_quit_no_aborted_connections) {
  SCOPED_TRACE("// connecting to server directly");
  auto admin_res = shared_servers()[0]->admin_cli();
  ASSERT_NO_ERROR(admin_res);

  auto admin_cli = std::move(*admin_res);

  auto before_res = query_one_result(admin_cli,
                                     "SELECT VARIABLE_VALUE "
                                     "FROM performance_schema.global_status "
                                     "WHERE variable_name = 'Aborted_clients'");
  ASSERT_NO_ERROR(before_res);

  SCOPED_TRACE("// connecting to server through router");
  {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    // and close again.
  }

  auto after_res = query_one_result(admin_cli,
                                    "SELECT VARIABLE_VALUE "
                                    "FROM performance_schema.global_status "
                                    "WHERE variable_name = 'Aborted_clients'");
  ASSERT_NO_ERROR(after_res);

  SCOPED_TRACE("// expect no new aborted clients");
  EXPECT_EQ((*before_res)[0][0], (*after_res)[0][0]);
}

TEST_P(ConnectionTest, classic_protocol_charset_after_connect) {
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  cli.set_option(MysqlClient::CharsetName("latin1"));

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = query_one_result(
        cli, "select @@character_set_client, @@collation_connection");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res,
                ElementsAre(ElementsAre("latin1", "latin1_swedish_ci")));
  }
}

TEST_P(ConnectionTest, classic_protocol_router_trace_set_fails) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "FR2");
  RecordProperty("Requirement",
                 "If connection pooling is not active, or the query is sent "
                 "via other commands (e.g. `COM_STMT_PREPARE`) the behaviour "
                 "MUST not change.");

  RecordProperty("Description",
                 "check that `ROUTER SET trace = 1` fails via `COM_QUERY`");

  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("ROUTER SET trace = 1");
    ASSERT_ERROR(cmd_res);

    EXPECT_EQ(cmd_res.error().value(), 1064) << cmd_res.error();
  }
}

TEST_P(ConnectionTest, classic_protocol_query_attribute_router_trace_ignored) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "FR2");
  RecordProperty("Requirement",
                 "If connection pooling is not active, or the query is sent "
                 "via other commands (e.g. `COM_STMT_PREPARE`) the behaviour "
                 "MUST not change.");

  RecordProperty("Description",
                 "check that query attributes starting with `router.` are "
                 "forwarded as is and don't generate a trace.");

  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    uint8_t tiny_one{1};
    std::array<MYSQL_BIND, 1> params{};
    params[0] = {};
    params[0].buffer = &tiny_one;
    params[0].buffer_length = sizeof(tiny_one);
    params[0].buffer_type = MYSQL_TYPE_TINY;
    std::array<const char *, 1> param_names{{"router.trace"}};

    auto cmd_res = cli.query("DO 1", params, param_names);
    ASSERT_NO_ERROR(cmd_res);

    auto warning_count_res = cli.warning_count();
    ASSERT_NO_ERROR(warning_count_res);
    EXPECT_EQ(*warning_count_res, 0);
  }
}

// 1238
static const std::unordered_set<std::string_view> read_only_sys_vars{
    "admin_address",
    "admin_port",
    "auto_generate_certs",
    "back_log",
    "basedir",
    "bind_address",
    "binlog_gtid_simple_recovery",
    "binlog_rotate_encryption_master_key_at_startup",
    "binlog_row_event_max_size",
    "build_id",
    "caching_sha2_password_auto_generate_rsa_keys",
    "caching_sha2_password_digest_rounds",
    "caching_sha2_password_private_key_path",
    "caching_sha2_password_public_key_path",
    "character_set_system",
    "character_sets_dir",
    "core_file",
    "create_admin_listener_thread",
    "datadir",
    "default_authentication_plugin",
    "disabled_storage_engines",
    "disconnect_on_expired_password",
    "error_count",
    "external_user",
    "ft_max_word_len",
    "ft_min_word_len",
    "ft_query_expansion_limit",
    "ft_stopword_file",
    "gtid_executed",
    "gtid_owned",
    "have_compress",
    "have_dynamic_loading",
    "have_geometry",
    "have_openssl",
    "have_profiling",
    "have_query_cache",
    "have_rtree_keys",
    "have_ssl",
    "have_statement_timeout",
    "have_symlink",
    "hostname",
    "init_file",
    "innodb_adaptive_hash_index_parts",
    "innodb_api_disable_rowlock",
    "innodb_api_enable_binlog",
    "innodb_api_enable_mdl",
    "innodb_autoinc_lock_mode",
    "innodb_buffer_pool_chunk_size",
    "innodb_buffer_pool_instances",
    "innodb_buffer_pool_load_at_startup",
    "innodb_data_file_path",
    "innodb_data_home_dir",
    "innodb_dedicated_server",
    "innodb_directories",
    "innodb_doublewrite_batch_size",
    "innodb_doublewrite_dir",
    "innodb_doublewrite_files",
    "innodb_doublewrite_pages",
    "innodb_flush_method",
    "innodb_force_load_corrupted",
    "innodb_force_recovery",
    "innodb_ft_cache_size",
    "innodb_ft_max_token_size",
    "innodb_ft_min_token_size",
    "innodb_ft_sort_pll_degree",
    "innodb_ft_total_cache_size",
    "innodb_log_file_size",
    "innodb_log_files_in_group",
    "innodb_log_group_home_dir",
    "innodb_numa_interleave",
    "innodb_open_files",
    "innodb_page_cleaners",
    "innodb_page_size",
    "innodb_purge_threads",
    "innodb_read_io_threads",
    "innodb_read_only",
    "innodb_rollback_on_timeout",
    "innodb_sort_buffer_size",
    "innodb_sync_array_size",
    "innodb_temp_data_file_path",
    "innodb_temp_tablespaces_dir",
    "innodb_undo_directory",
    "innodb_use_native_aio",
    "innodb_validate_tablespace_paths",
    "innodb_write_io_threads",
    "innodb_version",
    "large_files_support",
    "large_page_size",
    "large_pages",
    "lc_messages_dir",
    "license",
    "locked_in_memory",
    "log_bin",
    "log_bin_basename",
    "log_bin_index",
    "log_error",
    "log_replica_updates",
    "log_slave_updates",
    "lower_case_file_system",
    "lower_case_table_names",
    "max_digest_length",
    "myisam_mmap_size",
    "myisam_recover_options",
    "mysqlx_bind_address",
    "mysqlx_port",
    "mysqlx_port_open_timeout",
    "mysqlx_socket",
    "mysqlx_ssl_ca",
    "mysqlx_ssl_capath",
    "mysqlx_ssl_cert",
    "mysqlx_ssl_cipher",
    "mysqlx_ssl_crl",
    "mysqlx_ssl_crlpath",
    "mysqlx_ssl_key",
    "ngram_token_size",
    "named_pipe",
    "old",
    "open_files_limit",
    "performance_schema",
    "performance_schema_accounts_size",
    "performance_schema_digests_size",
    "performance_schema_error_size",
    "performance_schema_events_stages_history_long_size",
    "performance_schema_events_stages_history_size",
    "performance_schema_events_statements_history_long_size",
    "performance_schema_events_statements_history_size",
    "performance_schema_events_transactions_history_long_size",
    "performance_schema_events_transactions_history_size",
    "performance_schema_events_waits_history_long_size",
    "performance_schema_events_waits_history_size",
    "performance_schema_hosts_size",
    "performance_schema_max_cond_classes",
    "performance_schema_max_cond_instances",
    "performance_schema_max_digest_length",
    "performance_schema_max_file_classes",
    "performance_schema_max_file_handles",
    "performance_schema_max_file_instances",
    "performance_schema_max_index_stat",
    "performance_schema_max_memory_classes",
    "performance_schema_max_metadata_locks",
    "performance_schema_max_meter_classes",
    "performance_schema_max_metric_classes",
    "performance_schema_max_mutex_classes",
    "performance_schema_max_mutex_instances",
    "performance_schema_max_prepared_statements_instances",
    "performance_schema_max_program_instances",
    "performance_schema_max_rwlock_classes",
    "performance_schema_max_rwlock_instances",
    "performance_schema_max_socket_classes",
    "performance_schema_max_socket_instances",
    "performance_schema_max_sql_text_length",
    "performance_schema_max_stage_classes",
    "performance_schema_max_statement_classes",
    "performance_schema_max_statement_stack",
    "performance_schema_max_table_handles",
    "performance_schema_max_table_instances",
    "performance_schema_max_table_lock_stat",
    "performance_schema_max_thread_classes",
    "performance_schema_max_thread_instances",
    "performance_schema_session_connect_attrs_size",
    "performance_schema_setup_actors_size",
    "performance_schema_setup_objects_size",
    "performance_schema_users_size",
    "persist_only_admin_x509_subject",
    "persist_sensitive_variables_in_plaintext",
    "persisted_globals_load",
    "pid_file",
    "plugin_dir",
    "port",
    "protocol_version",
    "proxy_user",
    "relay_log",
    "relay_log_basename",
    "relay_log_index",
    "relay_log_recovery",
    "relay_log_space_limit",
    "replica_load_tmpdir",
    "replica_skip_errors",
    "report_host",
    "report_password",
    "report_port",
    "report_user",
    "sha256_password_auto_generate_rsa_keys",
    "sha256_password_private_key_path",
    "sha256_password_public_key_path",
    "shared_memory",
    "shared_memory_base_name",
    "secure_file_priv",
    "server_uuid",
    "skip_external_locking",
    "skip_networking",
    "skip_name_resolve",
    "skip_replica_start",
    "skip_show_database",
    "skip_slave_start",
    "slave_load_tmpdir",
    "slave_skip_errors",
    "socket",
    "ssl_fips_mode",
    "statement_id",
    "system_time_zone",
    "table_open_cache_instances",
    "thread_handling",
    "thread_stack",
    "tls_certificates_enforced_validation",
    "tmpdir",
    "version",
    "version_comment",
    "version_compile_machine",
    "version_compile_os",
    "version_compile_zlib",
    "warning_count",
};

// 1229
static const std::unordered_set<std::string_view> global_sys_vars{
    "activate_all_roles_on_login",
    "admin_ssl_ca",
    "admin_ssl_capath",
    "admin_ssl_cert",
    "admin_ssl_cipher",
    "admin_ssl_crl",
    "admin_ssl_crlpath",
    "admin_ssl_key",
    "admin_tls_ciphersuites",
    "admin_tls_version",
    "authentication_policy",
    "automatic_sp_privileges",
    "avoid_temporal_upgrade",
    "binlog_cache_size",
    "binlog_checksum",
    "binlog_encryption",
    "binlog_error_action",
    "binlog_expire_logs_auto_purge",
    "binlog_expire_logs_seconds",
    "binlog_group_commit_sync_delay",
    "binlog_group_commit_sync_no_delay_count",
    "binlog_max_flush_queue_time",
    "binlog_order_commits",
    "binlog_row_metadata",
    "binlog_stmt_cache_size",
    "binlog_transaction_dependency_history_size",
    "binlog_transaction_dependency_tracking",
    "check_proxy_users",
    "concurrent_insert",
    "connect_timeout",
    "default_password_lifetime",
    "delay_key_write",
    "delayed_insert_limit",
    "delayed_insert_timeout",
    "delayed_queue_size",
    "enforce_gtid_consistency",
    "event_scheduler",
    "flush",
    "flush_time",
    "ft_boolean_syntax",
    "general_log",
    "general_log_file",
    "global_connection_memory_limit",
    "gtid_executed_compression_period",
    "gtid_mode",
    "gtid_purged",
    "host_cache_size",
    "init_connect",
    "init_replica",
    "init_slave",
    "innodb_adaptive_flushing",
    "innodb_adaptive_flushing_lwm",
    "innodb_adaptive_hash_index",
    "innodb_adaptive_max_sleep_delay",
    "innodb_api_bk_commit_interval",
    "innodb_api_trx_level",
    "innodb_autoextend_increment",
    "innodb_buffer_pool_dump_at_shutdown",
    "innodb_buffer_pool_dump_now",
    "innodb_buffer_pool_dump_pct",
    "innodb_buffer_pool_filename",
    "innodb_buffer_pool_in_core_file",
    "innodb_buffer_pool_load_abort",
    "innodb_buffer_pool_load_now",
    "innodb_buffer_pool_size",
    "innodb_change_buffer_max_size",
    "innodb_change_buffering",
    "innodb_checksum_algorithm",
    "innodb_cmp_per_index_enabled",
    "innodb_commit_concurrency",
    "innodb_compression_failure_threshold_pct",
    "innodb_compression_level",
    "innodb_compression_pad_pct_max",
    "innodb_concurrency_tickets",
    "innodb_deadlock_detect",
    "innodb_default_row_format",
    "innodb_disable_sort_file_cache",
    "innodb_doublewrite",
    "innodb_extend_and_initialize",
    "innodb_fast_shutdown",
    "innodb_file_per_table",
    "innodb_fill_factor",
    "innodb_flush_log_at_timeout",
    "innodb_flush_log_at_trx_commit",
    "innodb_flush_neighbors",
    "innodb_flush_sync",
    "innodb_flushing_avg_loops",
    "innodb_fsync_threshold",
    "innodb_ft_aux_table",
    "innodb_ft_enable_diag_print",
    "innodb_ft_num_word_optimize",
    "innodb_ft_result_cache_limit",
    "innodb_ft_server_stopword_table",
    "innodb_idle_flush_pct",
    "innodb_io_capacity",
    "innodb_io_capacity_max",
    "innodb_log_buffer_size",
    "innodb_log_checksums",
    "innodb_log_compressed_pages",
    "innodb_log_spin_cpu_abs_lwm",
    "innodb_log_spin_cpu_pct_hwm",
    "innodb_log_wait_for_flush_spin_hwm",
    "innodb_log_write_ahead_size",
    "innodb_log_writer_threads",
    "innodb_lru_scan_depth",
    "innodb_max_dirty_pages_pct",
    "innodb_max_dirty_pages_pct_lwm",
    "innodb_max_purge_lag",
    "innodb_max_purge_lag_delay",
    "innodb_max_undo_log_size",
    "innodb_monitor_disable",
    "innodb_monitor_enable",
    "innodb_monitor_reset",
    "innodb_monitor_reset_all",
    "innodb_old_blocks_pct",
    "innodb_old_blocks_time",
    "innodb_online_alter_log_max_size",
    "innodb_optimize_fulltext_only",
    "innodb_print_all_deadlocks",
    "innodb_print_ddl_logs",
    "innodb_purge_batch_size",
    "innodb_purge_rseg_truncate_frequency",
    "innodb_random_read_ahead",
    "innodb_read_ahead_threshold",
    "innodb_redo_log_archive_dirs",
    "innodb_redo_log_capacity",
    "innodb_redo_log_encrypt",
    "innodb_replication_delay",
    "innodb_rollback_segments",
    "innodb_segment_reserve_factor",
    "innodb_spin_wait_delay",
    "innodb_spin_wait_pause_multiplier",
    "innodb_stats_auto_recalc",
    "innodb_stats_include_delete_marked",
    "innodb_stats_method",
    "innodb_stats_on_metadata",
    "innodb_stats_persistent",
    "innodb_stats_persistent_sample_pages",
    "innodb_stats_transient_sample_pages",
    "innodb_status_output",
    "innodb_status_output_locks",
    "innodb_sync_spin_loops",
    "innodb_thread_concurrency",
    "innodb_thread_sleep_delay",
    "innodb_undo_log_encrypt",
    "innodb_undo_log_truncate",
    "innodb_undo_tablespaces",
    "innodb_use_fdatasync",
    "key_buffer_size",
    "key_cache_age_threshold",
    "key_cache_block_size",
    "key_cache_division_limit",
    "keyring_operations",
    "local_infile",
    "log_bin_trust_function_creators",
    "log_error_services",
    "log_error_suppression_list",
    "log_error_verbosity",
    "log_output",
    "log_queries_not_using_indexes",
    "log_raw",
    "log_slow_admin_statements",
    "log_slow_extra",
    "log_slow_replica_statements",
    "log_slow_slave_statements",
    "log_statements_unsafe_for_binlog",
    "log_throttle_queries_not_using_indexes",
    "log_timestamps",
    "mandatory_roles",
    "master_verify_checksum",
    "max_binlog_cache_size",
    "max_binlog_size",
    "max_binlog_stmt_cache_size",
    "max_connect_errors",
    "max_connections",
    "max_prepared_stmt_count",
    "max_relay_log_size",
    "max_write_lock_count",
    "myisam_data_pointer_size",
    "myisam_max_sort_file_size",
    "myisam_use_mmap",
    "mysql_native_password_proxy_users",
    "mysqlx_compression_algorithms",
    "mysqlx_connect_timeout",
    "mysqlx_deflate_default_compression_level",
    "mysqlx_deflate_max_client_compression_level",
    "mysqlx_document_id_unique_prefix",
    "mysqlx_enable_hello_notice",
    "mysqlx_idle_worker_thread_timeout",
    "mysqlx_interactive_timeout",
    "mysqlx_lz4_default_compression_level",
    "mysqlx_lz4_max_client_compression_level",
    "mysqlx_max_allowed_packet",
    "mysqlx_max_connections",
    "mysqlx_min_worker_threads",
    "mysqlx_zstd_default_compression_level",
    "mysqlx_zstd_max_client_compression_level",
    "named_pipe_full_access_group",
    "offline_mode",
    "partial_revokes",
    "password_history",
    "password_require_current",
    "password_reuse_interval",
    "performance_schema_max_digest_sample_age",
    "performance_schema_show_processlist",
    "protocol_compression_algorithms",
    "read_only",
    "regexp_stack_limit",
    "regexp_time_limit",
    "relay_log_purge",
    "replica_allow_batching",
    "replica_checkpoint_group",
    "replica_checkpoint_period",
    "replica_compressed_protocol",
    "replica_exec_mode",
    "replica_max_allowed_packet",
    "replica_net_timeout",
    "replica_parallel_type",
    "replica_parallel_workers",
    "replica_pending_jobs_size_max",
    "replica_preserve_commit_order",
    "replica_sql_verify_checksum",
    "replica_transaction_retries",
    "replica_type_conversions",
    "replication_optimize_for_static_plugin_config",
    "replication_sender_observe_commit_only",
    "require_secure_transport",
    "rpl_read_size",
    "rpl_stop_replica_timeout",
    "rpl_stop_slave_timeout",
    "schema_definition_cache",
    "server_id",
    "server_id_bits",
    "sha256_password_proxy_users",
    "slave_allow_batching",
    "slave_checkpoint_group",
    "slave_checkpoint_period",
    "slave_compressed_protocol",
    "slave_exec_mode",
    "slave_max_allowed_packet",
    "slave_net_timeout",
    "slave_parallel_type",
    "slave_parallel_workers",
    "slave_pending_jobs_size_max",
    "slave_preserve_commit_order",
    "slave_rows_search_algorithms",
    "slave_sql_verify_checksum",
    "slave_transaction_retries",
    "slave_type_conversions",
    "slow_launch_time",
    "slow_query_log",
    "slow_query_log_file",
    "source_verify_checksum",
    "sql_replica_skip_counter",
    "sql_slave_skip_counter",
    "ssl_ca",
    "ssl_capath",
    "ssl_cert",
    "ssl_cipher",
    "ssl_crl",
    "ssl_crlpath",
    "ssl_key",
    "ssl_session_cache_mode",
    "ssl_session_cache_timeout",
    "stored_program_cache",
    "stored_program_definition_cache",
    "super_read_only",
    "sync_binlog",
    "sync_master_info",
    "sync_relay_log",
    "sync_relay_log_info",
    "sync_source_info",
    "table_definition_cache",
    "table_encryption_privilege_check",
    "table_open_cache",
    "tablespace_definition_cache",
    "temptable_max_mmap",
    "temptable_max_ram",
    "temptable_use_mmap",
    "thread_cache_size",
    "tls_ciphersuites",
    "tls_version",
};

// 1227
static const std::unordered_set<std::string_view> super_sys_vars{
    "binlog_direct_non_transactional_updates",
    "binlog_format",
    "binlog_row_image",
    "binlog_row_value_options",
    "binlog_rows_query_log_events",
    "binlog_transaction_compression",
    "binlog_transaction_compression_level_zstd",
    "bulk_insert_buffer_size",
    "character_set_database",
    "character_set_filesystem",
    "connection_memory_chunk_size",
    "connection_memory_limit",
    "global_connection_memory_tracking",
    "gtid_executed",
    "gtid_next",
    "histogram_generation_max_mem_size",
    "innodb_strict_mode",
    "innodb_tmpdir",
    "internal_tmp_mem_storage_engine",
    "immediate_server_version",
    "max_error_count",
    "max_delayed_threads",
    "max_examined_row_limit",
    "min_examined_row_limit",
    "max_insert_delayed_threads",
    "low_priority_updates",
    "pseudo_replica_mode",
    "pseudo_slave_mode",
    "pseudo_thread_id",
    "original_server_version",
    "original_commit_timestamp",
    "preload_buffer_size",
    "select_into_disk_sync_delay",
    "select_into_buffer_size",
    "show_old_temporals",
    "sql_generate_invisible_primary_key",
    "sql_require_primary_key",
    "sql_log_bin",
    "sql_log_off",
    "sql_required_primary",
    "xa_detach_on_prepare",
};

// 1621
static const std::unordered_set<std::string_view> read_only_super_sys_vars{
    "max_allowed_packet",
    "max_user_connections",
    "net_buffer_length",
};

TEST_P(ConnectionTest, classic_protocol_replay_session_trackers) {
  RecordProperty("Description", "check if system-variables can be replayed.");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto session_vars_res =
      query_one_result(cli,
                       "SELECT * "
                       "  FROM performance_schema.session_variables "
                       " ORDER BY VARIABLE_NAME");
  ASSERT_NO_ERROR(session_vars_res);
  auto session_vars = *session_vars_res;

  for (auto var : session_vars) {
    std::ostringstream oss;
    oss << "SET @@SESSION." << std::quoted(var[0], '`') << "=";

    if (var[1].empty()) {
      if (var[0] == "innodb_ft_user_stopword_table") {
        oss << "NULL";
      } else {
        oss << "''";
      }
    } else {
      // check if the string needs to be replayed as number or as string.
      auto needs_quotation = [](std::string_view s) -> bool {
        if (s.empty()) return true;  // -> ""

        // std::from_chars(double) would have worked here, but GCC 10 doesn't
        // provide that.
        const char *start = s.data();
        const char *const end = start + s.size();

        // skip initial sign as the number may be a uint64_t or a int64_t
        if (*start == '-' || *start == '+') ++start;

        uint64_t num;
        auto is_numeric = std::from_chars(start, end, num);

        if (is_numeric.ec == std::errc{}) {
          if (is_numeric.ptr == end) return false;  // 10 -> 10

          if (*is_numeric.ptr != '.') return true;  // 10abc -> "10abc"

          start = is_numeric.ptr + 1;
          is_numeric = std::from_chars(start, end, num);

          if (is_numeric.ec == std::errc{} && is_numeric.ptr == end) {
            return false;  // 10.12 -> 10.12
          }
        }

        return true;  // abc -> "abc"
      };

      if (needs_quotation(var[1])) {
        oss << std::quoted(var[1]);
      } else {
        oss << var[1];
      }
    }

    SCOPED_TRACE("// " + oss.str());

    auto set_var_res = query_one_result(cli, oss.str());

    if (read_only_sys_vars.contains(var[0])) {
      ASSERT_ERROR(set_var_res);
      // is a read only variable
      EXPECT_EQ(set_var_res.error().value(), 1238) << set_var_res.error();
    } else if (global_sys_vars.contains(var[0])) {
      ASSERT_ERROR(set_var_res);
      EXPECT_EQ(set_var_res.error().value(), 1229) << set_var_res.error();
    } else if (super_sys_vars.contains(var[0])) {
      ASSERT_ERROR(set_var_res);
      EXPECT_EQ(set_var_res.error().value(), 1227) << set_var_res.error();
    } else if (read_only_super_sys_vars.contains(var[0])) {
      ASSERT_ERROR(set_var_res);
      EXPECT_EQ(set_var_res.error().value(), 1621) << set_var_res.error();
    } else {
      EXPECT_NO_ERROR(set_var_res);
    }
  }
}

// 1231
static const std::unordered_set<std::string_view> not_nullable_sys_vars{
    "autocommit",
    "big_tables",
    "binlog_direct_non_transactional_updates",
    "binlog_format",
    "binlog_row_image",
    "binlog_row_value_options",
    "binlog_rows_query_log_events",
    "binlog_transaction_compression",
    "block_encryption_mode",
    "character_set_client",
    "character_set_connection",
    "character_set_server",
    "collation_connection",
    "collation_database",
    "collation_server",
    "completion_type",
    "default_collation_for_utf8mb4",
    "default_storage_engine",
    "default_table_encryption",
    "default_tmp_storage_engine",
    "end_markers_in_json",
    "explain_format",
    "explicit_defaults_for_timestamp",
    "foreign_key_checks",
    "global_connection_memory_tracking",
    "group_replication_consistency",
    "innodb_ft_enable_stopword",
    "innodb_table_locks",
    "internal_tmp_mem_storage_engine",
    "keep_files_on_create",
    "lc_messages",
    "lc_time_names",
    "low_priority_updates",
    "myisam_stats_method",
    "new",
    "old_alter_table",
    "optimizer_switch",
    "optimizer_trace",
    "optimizer_trace_features",
    "print_identified_with_as_hex",
    "profiling",
    "pseudo_replica_mode",
    "pseudo_slave_mode",
    "rbr_exec_mode",
    "require_row_format",
    "resultset_metadata",
    "select_into_disk_sync",
    "session_track_gtids",
    "session_track_schema",
    "session_track_state_change",
    "session_track_transaction_info",
    "show_create_table_skip_secondary_engine",
    "show_create_table_verbosity",
    "show_gipk_in_create_table_and_information_schema",
    "show_old_temporals",
    "sql_auto_is_null",
    "sql_big_selects",
    "sql_buffer_result",
    "sql_generate_invisible_primary_key",
    "sql_log_bin",
    "sql_log_off",
    "sql_mode",
    "sql_notes",
    "sql_quote_show_create",
    "sql_require_primary_key",
    "sql_safe_updates",
    "sql_warnings",
    "terminology_use_previous",
    "time_zone",
    "transaction_allow_batching",
    "transaction_isolation",
    "transaction_read_only",
    "unique_checks",
    "updatable_views_with_limit",
    "use_secondary_engine",
    "windowing_use_high_precision",
    "xa_detach_on_prepare",
};

// 1232 Incorrect argument type.
static const std::unordered_set<std::string_view>
    null_is_invalid_argument_sys_vars{
        "auto_increment_increment",
        "auto_increment_offset",
        "binlog_transaction_compression_level_zstd",
        "bulk_insert_buffer_size",
        "connection_memory_chunk_size",
        "connection_memory_limit",
        "cte_max_recursion_depth",
        "default_week_format",
        "div_precision_increment",
        "eq_range_index_dive_limit",
        "generated_random_password_length",
        "group_concat_max_len",
        "histogram_generation_max_mem_size",
        "identity",
        "immediate_server_version",
        "information_schema_stats_expiry",
        "innodb_ddl_buffer_size",
        "innodb_ddl_threads",
        "innodb_lock_wait_timeout",
        "innodb_parallel_read_threads",
        "insert_id",
        "interactive_timeout",
        "join_buffer_size",
        "last_insert_id",
        "lock_wait_timeout",
        "long_query_time",
        "max_allowed_packet",
        "max_delayed_threads",
        "max_error_count",
        "max_execution_time",
        "max_heap_table_size",
        "max_insert_delayed_threads",
        "max_join_size",
        "max_length_for_sort_data",
        "max_points_in_geometry",
        "max_seeks_for_key",
        "max_sort_length",
        "max_sp_recursion_depth",
        "max_user_connections",
        "min_examined_row_limit",
        "myisam_sort_buffer_size",
        "mysqlx_read_timeout",
        "mysqlx_wait_timeout",
        "mysqlx_write_timeout",
        "net_buffer_length",
        "net_read_timeout",
        "net_retry_count",
        "net_write_timeout",
        "optimizer_max_subgraph_pairs",
        "optimizer_prune_level",
        "optimizer_search_depth",
        "optimizer_trace_limit",
        "optimizer_trace_max_mem_size",
        "optimizer_trace_offset",
        "original_commit_timestamp",
        "original_server_version",
        "parser_max_mem_size",
        "preload_buffer_size",
        "profiling_history_size",
        "pseudo_thread_id",
        "query_alloc_block_size",
        "query_prealloc_size",
        "rand_seed1",
        "rand_seed2",
        "range_alloc_block_size",
        "range_optimizer_max_mem_size",
        "read_buffer_size",
        "read_rnd_buffer_size",
        "secondary_engine_cost_threshold",
        "select_into_buffer_size",
        "select_into_disk_sync_delay",
        "set_operations_buffer_size",
        "sort_buffer_size",
        "sql_select_limit",
        "timestamp",
        "tmp_table_size",
        "transaction_alloc_block_size",
        "transaction_prealloc_size",
        "wait_timeout",
    };

TEST_P(ConnectionTest, classic_protocol_session_vars_nullable) {
  RecordProperty("Description", "check if system-variables can be replayed.");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto session_vars_res =
      query_one_result(cli,
                       "SELECT * "
                       "  FROM performance_schema.session_variables "
                       " ORDER BY VARIABLE_NAME");
  ASSERT_NO_ERROR(session_vars_res);
  auto session_vars = *session_vars_res;

  for (auto var : session_vars) {
    std::ostringstream oss;
    oss << "SET @@SESSION." << std::quoted(var[0], '`') << "="
        << "NULL";

    SCOPED_TRACE("// " + oss.str());

    auto set_var_res = query_one_result(cli, oss.str());

    if (!set_var_res) {
      EXPECT_THAT(set_var_res.error().value(),
                  ::testing::AnyOf(1227,  // super sys-var
                                   1229,  // global sys-var
                                   1231,  // not nullable
                                   1232,  // NULL is invalid-argument
                                   1238   // read-only
                                   ))
          << set_var_res.error();
    } else {
      EXPECT_NO_ERROR(set_var_res);

      // ensure that no new nullable sys-vars are added.
      EXPECT_THAT(var[0], testing::AnyOf("character_set_results",
                                         "innodb_ft_user_stopword_table",
                                         "session_track_system_variables"));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ConnectionTest,
                         ::testing::ValuesIn(connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

class ConnectionTestSlow
    : public ConnectionTestBase,
      public ::testing::WithParamInterface<ConnectionParam> {};

TEST_P(ConnectionTestSlow, classic_protocol_slow_query_abort_client) {
  RecordProperty("Description",
                 "check that close of the client connection while a query "
                 "waits for a response aborts the query.");

  using clock_type = std::chrono::steady_clock;

  auto account = SharedServer::native_empty_password_account();

  auto admin_account = SharedServer::admin_account();

  auto admin_cli_res = shared_servers()[0]->admin_cli();
  ASSERT_NO_ERROR(admin_cli_res);
  auto admin_cli = std::move(*admin_cli_res);

  {
    auto query_res =
        query_one_result(admin_cli,
                         "SELECT * FROM performance_schema.processlist "
                         "WHERE user = '" +
                             account.username + "'");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, testing::SizeIs(0));
  }

  TempDirectory testdir;
  auto testfilename = testdir.file("sleep.test");

  {
    std::ofstream ofs(testfilename);
    ofs << "SELECT SLEEP(20);\n";
  }

  const ExitStatus expected_exit_status{
#ifndef _WIN32
      ExitStatus::terminated_t{}, SIGKILL
#else
      EXIT_SUCCESS
#endif
  };
  const auto host = shared_router()->host();
  const auto port = shared_router()->port(GetParam());
  auto &long_running_query =
      launch_command(ProcessManager::get_origin().join("mysqltest").str(),
                     {
                         "--user", account.username,        //
                         "--password=" + account.password,  //
                         "--host", host,                    //
                         "--protocol", "TCP",               //
                         "--port", std::to_string(port),    //
                         "--test-file", testfilename,       //
                     },
                     expected_exit_status, true, -1s);

  // wait until the SLEEP appears in the processlist
  for (auto start = clock_type::now();; std::this_thread::sleep_for(100ms)) {
    ASSERT_LT(clock_type::now() - start, 10s);

    auto query_res = query_one_result(admin_cli,
                                      "SELECT id, command, state "
                                      "FROM performance_schema.processlist "
                                      "WHERE user = '" +
                                          account.username + "'");
    ASSERT_NO_ERROR(query_res);
    auto result = *query_res;

    if (result.size() == 1 && result[0][1] == "Query") break;
  }

  // interrupt the SLEEP() query
  long_running_query.send_shutdown_event(
      mysql_harness::ProcessLauncher::ShutdownEvent::KILL);

  // wait until it is gone.
  for (auto start = clock_type::now();; std::this_thread::sleep_for(500ms)) {
    // the server will check that the query is killed every 5 seconds since it
    // started to SLEEP()
    ASSERT_LT(clock_type::now() - start, 12s);

    auto query_res =
        query_one_result(admin_cli,
                         "SELECT * FROM performance_schema.processlist "
                         "WHERE user = '" +
                             account.username + "'");
    ASSERT_NO_ERROR(query_res);

    if (query_res->empty()) break;

#if 0
    for (const auto &row : *query_res) {
      std::cerr << mysql_harness::join(row, " | ") << "\n";
    }
#endif
  }
}

TEST_P(ConnectionTestSlow, classic_protocol_execute_slow_query_abort_client) {
  RecordProperty("Description",
                 "check that close of the client connection while a prepared "
                 "statement executes waits for a response aborts the execute.");

  using clock_type = std::chrono::steady_clock;

  auto account = SharedServer::native_empty_password_account();

  auto admin_account = SharedServer::admin_account();

  auto admin_cli_res = shared_servers()[0]->admin_cli();
  ASSERT_NO_ERROR(admin_cli_res);
  auto admin_cli = std::move(*admin_cli_res);

  {
    auto query_res =
        query_one_result(admin_cli,
                         "SELECT * FROM performance_schema.processlist "
                         "WHERE user = '" +
                             account.username + "'");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, testing::SizeIs(0));
  }

  TempDirectory testdir;
  auto testfilename = testdir.file("sleep.test");

  {
    std::ofstream ofs(testfilename);
    ofs << "SELECT SLEEP(20);\n";
  }

  const ExitStatus expected_exit_status{
#ifndef _WIN32
      ExitStatus::terminated_t{}, SIGKILL
#else
      EXIT_SUCCESS
#endif
  };
  const auto host = shared_router()->host();
  const auto port = shared_router()->port(GetParam());
  auto &long_running_query =
      launch_command(ProcessManager::get_origin().join("mysqltest").str(),
                     {"--user", account.username,        //
                      "--password=" + account.password,  //
                      "--host", host,                    //
                      "--protocol", "TCP",               //
                      "--port", std::to_string(port),    //
                      "--test-file", testfilename,       //
                      "--ps-protocol"},
                     expected_exit_status, true, -1s);

  // wait until the SLEEP appears in the processlist
  for (auto start = clock_type::now();; std::this_thread::sleep_for(100ms)) {
    ASSERT_LT(clock_type::now() - start, 10s);

    auto query_res = query_one_result(admin_cli,
                                      "SELECT id, command, state "
                                      "FROM performance_schema.processlist "
                                      "WHERE user = '" +
                                          account.username + "'");
    ASSERT_NO_ERROR(query_res);
    auto result = *query_res;

    if (result.size() == 1 && result[0][1] == "Execute") break;
  }

  // interrupt the execute SLEEP()
  long_running_query.send_shutdown_event(
      mysql_harness::ProcessLauncher::ShutdownEvent::KILL);

  // wait until it is gone.
  for (auto start = clock_type::now();; std::this_thread::sleep_for(500ms)) {
    // the server will check that the query is killed every 5 seconds since it
    // started to SLEEP()
    ASSERT_LT(clock_type::now() - start, 12s);

    auto query_res =
        query_one_result(admin_cli,
                         "SELECT * FROM performance_schema.processlist "
                         "WHERE user = '" +
                             account.username + "'");
    ASSERT_NO_ERROR(query_res);

    if (query_res->empty()) break;
#if 0
    for (const auto &row : *query_res) {
      std::cerr << mysql_harness::join(row, " | ") << "\n";
    }
#endif
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ConnectionTestSlow,
                         ::testing::ValuesIn(distinct_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

static constexpr const char *default_auth_params[] = {
    "default",
    "mysql_native_password",
    "caching_sha2_password",
    "sha256_password",
};

struct ConnectTestParam {
  std::string scenario;
  SharedServer::Account account;

  std::function<int(ConnectionParam)> expected_error_code_func;
};

static ConnectTestParam connect_test_params[] = {
    // mysql_native_password
    //
    {"native_password_account_with_empty_password",
     SharedServer::native_empty_password_account(), [](auto) { return 0; }},
    {"native_password_account_with_empty_password_auth_with_wrong_password",
     SharedServer::Account{
         SharedServer::native_empty_password_account().username,
         "wrong-password",
         SharedServer::caching_sha2_empty_password_account().auth_method},
     [](auto) { return 1045; }},
    {"native_password_account_with_password",
     SharedServer::native_password_account(), [](auto) { return 0; }},
    {"native_password_account_with_password_auth_with_wrong_password",
     SharedServer::Account{SharedServer::native_password_account().username,
                           "wrong-password",
                           SharedServer::native_password_account().auth_method},
     [](auto) { return 1045; }},  // "Access denied for user ..."
    {"native_password_account_with_password_auth_with_empty_password",
     SharedServer::Account{SharedServer::native_password_account().username, "",
                           SharedServer::native_password_account().auth_method},
     [](auto) { return 1045; }},  // "Access denied for user ..."

    // caching_sha2_password
    //
    {"caching_sha2_password_account_with_empty_password",
     SharedServer::caching_sha2_empty_password_account(),
     [](auto) { return 0; }},
    {"caching_sha2_password_account_with_empty_password_auth_with_wrong_"
     "password",
     SharedServer::Account{
         SharedServer::caching_sha2_empty_password_account().username,
         "wrong-password",
         SharedServer::caching_sha2_empty_password_account().auth_method},
     [](auto connect_param) {
       return connect_param.client_ssl_mode == kDisabled ? 2061 : 1045;
     }},  // "Access denied for user ..."
    {"caching_sha2_password_account_with_password",
     SharedServer::caching_sha2_password_account(),
     [](auto connect_param) {
       return connect_param.client_ssl_mode == kDisabled ? 2061 : 0;
     }},
    {"caching_sha2_password_account_with_password_auth_with_wrong_password",
     SharedServer::Account{
         SharedServer::caching_sha2_password_account().username,
         "wrong-password",
         SharedServer::caching_sha2_password_account().auth_method},
     [](auto connect_param) {
       return connect_param.client_ssl_mode == kDisabled ? 2061 : 1045;
     }},  // "Access denied for user ..."
    {"caching_sha2_password_account_with_password_auth_with_empty_password",
     SharedServer::Account{
         SharedServer::caching_sha2_password_account().username, "",
         SharedServer::caching_sha2_password_account().auth_method},
     [](auto) { return 1045; }},  // "Access denied for user ..."

    // sha256_password
    //
    {"sha256_password_account_with_empty_password",
     SharedServer::sha256_empty_password_account(), [](auto) { return 0; }},
    {"sha256_password_account_with_empty_password_auth_with_wrong_password",
     SharedServer::Account{
         SharedServer::sha256_empty_password_account().username,
         "wrong-password",
         SharedServer::sha256_empty_password_account().auth_method},
     [](auto) { return 1045; }},  // "Access denied for user ..."
    {"sha256_password_account_with_password",
     SharedServer::sha256_password_account(),
     [](auto connect_param) {
       return (connect_param.client_ssl_mode == kDisabled &&
               (connect_param.server_ssl_mode == kPreferred ||
                connect_param.server_ssl_mode == kRequired))
                  ? 1045
                  : 0;
     }},
    {"sha256_password_account_with_password_auth_with_wrong_password",
     SharedServer::Account{SharedServer::sha256_password_account().username,
                           "wrong-password",
                           SharedServer::sha256_password_account().auth_method},
     [](auto) { return 1045; }},  // "Access denied for user ..."
    {"sha256_password_account_with_password_auth_with_empty_password",
     SharedServer::Account{SharedServer::sha256_password_account().username, "",
                           SharedServer::sha256_password_account().auth_method},
     [](auto) { return 1045; }},  // "Access denied for user ..."
};

class ConnectionConnectTest
    : public ConnectionTestBase,
      public ::testing::WithParamInterface<
          std::tuple<ConnectTestParam, ConnectionParam, const char *>> {};

TEST_P(ConnectionConnectTest, classic_protocol_connect) {
  auto [test_param, connect_param, default_auth] = GetParam();

  auto [test_name, account, expected_error_code_func] = test_param;

  if (account.auth_method == "caching_sha2_password") {
    for (auto &srv : shared_servers()) {
      srv->flush_privileges();  // reset the auth-cache
    }
  }

  MysqlClient cli;

  if (default_auth != "default"sv) {
    cli.set_option(MysqlClient::DefaultAuthentication(default_auth));
  }

  cli.username(account.username);
  cli.password(account.password);

  auto connect_res = cli.connect(shared_router()->host(),
                                 shared_router()->port(connect_param));

  auto expected_error_code = expected_error_code_func(connect_param);

  if (expected_error_code == 0) {
    ASSERT_NO_ERROR(connect_res);
  } else {
    ASSERT_ERROR(connect_res);
    EXPECT_EQ(connect_res.error().value(), expected_error_code);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, ConnectionConnectTest,
    ::testing::Combine(::testing::ValuesIn(connect_test_params),
                       ::testing::ValuesIn(connection_params),
                       ::testing::ValuesIn(default_auth_params)),
    [](auto &info) {
      return std::get<0>(info.param).scenario + "__via_" +
             std::get<1>(info.param).testname + "__default_auth_is_" +
             std::string(std::get<2>(info.param));
    });

struct BenchmarkParam {
  std::string testname;

  std::string stmt;
};

class Benchmark : public RouterComponentTest,
                  public ::testing::WithParamInterface<BenchmarkParam> {
 public:
  static constexpr const size_t kNumServers = 1;

  static void SetUpTestSuite() {
    for (const auto &svr : shared_servers()) {
      if (svr->mysqld_failed_to_start()) GTEST_SKIP();
    }

    TestWithSharedRouter::SetUpTestSuite(test_env->port_pool(),
                                         shared_servers());
  }

  static void TearDownTestSuite() { TestWithSharedRouter::TearDownTestSuite(); }

  static std::array<SharedServer *, kNumServers> shared_servers() {
    return test_env->servers();
  }

  static SharedRouter *shared_router() {
    return TestWithSharedRouter::router();
  }

  void SetUp() override {
    for (auto &s : shared_servers()) {
      // shared_server_ may be null if TestWithSharedServer::SetUpTestSuite
      // threw?
      if (s == nullptr || s->mysqld_failed_to_start()) {
        GTEST_SKIP() << "failed to start mysqld";
      }
    }
  }

  ~Benchmark() override {
    if (::testing::Test::HasFailure()) {
      shared_router()->process_manager().dump_logs();
    }
  }
};

namespace std {
template <class Rep, class Period>
std::ostream &operator<<(std::ostream &os,
                         std::chrono::duration<Rep, Period> dur) {
  std::ostringstream oss;
  oss.flags(os.flags());
  oss.imbue(os.getloc());
  oss.precision(os.precision());

  if (dur < 1us) {
    oss << std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(
               dur)
               .count()
        << " ns";
  } else if (dur < 1ms) {
    oss << std::chrono::duration_cast<
               std::chrono::duration<double, std::micro>>(dur)
               .count()
        << " us";
  } else if (dur < 1s) {
    oss << std::chrono::duration_cast<
               std::chrono::duration<double, std::milli>>(dur)
               .count()
        << " ms";
  } else {
    oss << std::chrono::duration_cast<
               std::chrono::duration<double, std::ratio<1>>>(dur)
               .count()
        << "  s";
  }

  os << oss.str();

  return os;
}
}  // namespace std

template <class Dur>
struct Throughput {
  uint64_t count;

  Dur duration;
};

template <class Dur>
Throughput(uint64_t count, Dur duration) -> Throughput<Dur>;

template <class Dur>
std::ostream &operator<<(std::ostream &os, Throughput<Dur> throughput) {
  std::ostringstream oss;
  oss.flags(os.flags());
  oss.imbue(os.getloc());
  oss.precision(os.precision());

  // normalize to per-second

  double bytes_per_second =
      throughput.count /
      std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(
          throughput.duration)
          .count();
  if (bytes_per_second < 1024) {
    oss << bytes_per_second << "  B/s";
  } else if (bytes_per_second < 1024 * 1024) {
    oss << (bytes_per_second / 1024) << " kB/s";
  } else if (bytes_per_second < 1024 * 1024 * 1024) {
    oss << (bytes_per_second / (1024 * 1024)) << " MB/s";
  } else {
    oss << (bytes_per_second / (1024 * 1024 * 1024)) << " GB/s";
  }

  os << oss.str();

  return os;
}

static void bench_stmt(MysqlClient &cli, std::string_view prefix,
                       std::string_view stmt) {
  using clock_type = std::chrono::steady_clock;

  constexpr const auto kMaxRuntime = 100ms;
  auto end_time = clock_type::now() + kMaxRuntime;

  size_t rounds{};
  uint64_t recved{};

  clock_type::duration query_duration{};
  clock_type::duration fetch_duration{};

  do {
    auto query_start = clock_type::now();
    auto send_query_res = cli.send_query(stmt);
    query_duration += clock_type::now() - query_start;
    ASSERT_NO_ERROR(send_query_res);

    recved += 4 + 10;  // Ok or Eof.

    auto fetch_start = clock_type::now();

    auto query_res = cli.read_query_result();
    ASSERT_NO_ERROR(query_res);

    for (const auto &result : *query_res) {
      auto field_count = result.field_count();
      for (const auto &row : result.rows()) {
        for (size_t ndx = 0; ndx < field_count; ++ndx) {
          recved += strlen(row[ndx]);
        }
      }
    }
    fetch_duration += clock_type::now() - fetch_start;

    ++rounds;
  } while (clock_type::now() < end_time);

  std::ostringstream oss;
  oss.precision(2);
  oss << std::left << std::setw(25) << prefix << " | "  //
      << std::right << std::setw(10) << std::fixed << (query_duration / rounds)
      << " | "  //
      << std::right << std::setw(10) << std::fixed << (fetch_duration / rounds)
      << " | "  //
      << std::right << std::setw(11) << Throughput{recved, fetch_duration}
      << "\n";
  std::cout << oss.str();
}

TEST_P(Benchmark, classic_protocol) {
  {
    std::ostringstream oss;
    oss << std::left << std::setw(25) << "name"
        << " | " << std::left << std::setw(7 + 3) << "query"
        << " | " << std::left << std::setw(7 + 3) << "fetch"
        << " | " << std::left << std::setw(7 + 4) << "throughput"
        << "\n";
    std::cout << oss.str();
  }
  {
    std::ostringstream oss;
    oss << std::right << std::setw(25) << std::setfill('-') << " no-ssl"
        << " | " << std::right << std::setw(7 + 3) << std::setfill('-') << ""
        << " | " << std::right << std::setw(7 + 3) << std::setfill('-') << ""
        << " | " << std::right << std::setw(7 + 4) << std::setfill('-') << ""
        << "\n";
    std::cout << oss.str();
  }

  SCOPED_TRACE("// connecting to server directly");
  {
    MysqlClient cli;

    auto *srv = shared_servers()[0];

    auto account = srv->admin_account();

    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));

    auto connect_res = cli.connect(srv->server_host(), srv->server_port());
    ASSERT_NO_ERROR(connect_res);

    bench_stmt(cli, "DIRECT_DISABLED", GetParam().stmt);
  }

  SCOPED_TRACE("// connecting to server through router");
  for (const auto &router_endpoint : connection_params) {
    if ((router_endpoint.client_ssl_mode != kDisabled &&
         router_endpoint.client_ssl_mode != kPassthrough) ||
        router_endpoint.redundant_combination()) {
      continue;
    }
    MysqlClient cli;

    cli.username("root");
    cli.password("");
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(router_endpoint)));

    bench_stmt(cli, router_endpoint.testname, GetParam().stmt);
  }

  {
    std::ostringstream oss;
    oss << std::right << std::setw(25) << std::setfill('-') << " ssl"
        << " | " << std::right << std::setw(7 + 3) << std::setfill('-') << ""
        << " | " << std::right << std::setw(7 + 3) << std::setfill('-') << ""
        << " | " << std::right << std::setw(7 + 4) << std::setfill('-') << ""
        << "\n";
    std::cout << oss.str();
  }

  {
    MysqlClient cli;

    auto *srv = shared_servers()[0];

    auto account = srv->admin_account();

    cli.username(account.username);
    cli.password(account.password);

    auto connect_res = cli.connect(srv->server_host(), srv->server_port());
    ASSERT_NO_ERROR(connect_res);

    bench_stmt(cli, "DIRECT_PREFERRED", GetParam().stmt);
  }

  SCOPED_TRACE("// connecting to server through router");
  for (const auto &router_endpoint : connection_params) {
    if (router_endpoint.client_ssl_mode == kDisabled ||
        router_endpoint.redundant_combination() ||
        router_endpoint.client_ssl_mode == kRequired) {
      // Required is the same as Preferred
      continue;
    }
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(router_endpoint)));

    bench_stmt(cli, router_endpoint.testname, GetParam().stmt);
  }
}

const BenchmarkParam benchmark_params[] = {
    {"tiny", "DO 1"},
    {"one_long_row", "SELECT REPEAT('*', 1024 * 1024)"},
    {"many_short_rows",
     "WITH RECURSIVE cte (n) AS ("
     "  SELECT 1 UNION ALL "
     "  SELECT n + 1 FROM cte LIMIT 100000) "
     "SELECT /*+ SET_VAR(cte_max_recursion_depth = 1M) */ * FROM cte;"},
};

INSTANTIATE_TEST_SUITE_P(Spec, Benchmark, ::testing::ValuesIn(benchmark_params),
                         [](auto &info) { return info.param.testname; });

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  TlsLibraryContext tls_lib_ctx;

  // env is owned by googletest
  test_env =
      dynamic_cast<TestEnv *>(::testing::AddGlobalTestEnvironment(new TestEnv));

  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
