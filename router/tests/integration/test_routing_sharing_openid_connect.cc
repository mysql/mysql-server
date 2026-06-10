/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include <algorithm>  // min
#include <charconv>
#include <chrono>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock-more-matchers.h>
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysql/harness/tls_context.h"
#include "mysql/harness/utility/string.h"  // join
#include "process_manager.h"
#include "procs.h"
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "shared_server.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

// using namespace std::string_literals;
using namespace std::chrono_literals;

using testing::ElementsAre;
using testing::Pair;

static constexpr const auto kIdleServerConnectionsSleepTime{10ms};

static constexpr const std::string_view kDisabled{"DISABLED"};
static constexpr const std::string_view kRequired{"REQUIRED"};
static constexpr const std::string_view kPreferred{"PREFERRED"};
static constexpr const std::string_view kPassthrough{"PASSTHROUGH"};
static constexpr const std::string_view kAsClient{"AS_CLIENT"};

std::ostream &operator<<(std::ostream &os, const MysqlError &e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
}

namespace {

stdx::expected<void, MysqlError> cli_connect(
    MysqlClient &cli, const mysql_harness::Destination &dest) {
  if (dest.is_local()) {
    const auto &local_dest = dest.as_local();
    return cli.connect(MysqlClient::unix_socket_t{}, local_dest.path());
  }

  const auto &tcp_dest = dest.as_tcp();
  return cli.connect(tcp_dest.hostname(), tcp_dest.port());
}

// query a single row and return an array of N std::strings.
template <size_t N>
stdx::expected<std::array<std::string, N>, MysqlError> query_one(
    MysqlClient &cli, std::string_view stmt) {
  auto cmd_res = cli.query(stmt);
  if (!cmd_res) return stdx::unexpected(cmd_res.error());

  auto results = *cmd_res;

  auto res_it = results.begin();
  if (!(res_it != results.end())) {
    return stdx::unexpected(MysqlError(1, "No results", "HY000"));
  }

  if (res_it->field_count() != N) {
    return stdx::unexpected(
        MysqlError(1, "field-count doesn't match", "HY000"));
  }

  auto rows = res_it->rows();
  auto rows_it = rows.begin();
  if (rows_it == rows.end()) {
    return stdx::unexpected(MysqlError(1, "No rows", "HY000"));
  }

  std::array<std::string, N> out;
  for (auto [ndx, f] : stdx::views::enumerate(out)) {
    f = (*rows_it)[ndx];
  }

  ++rows_it;
  if (rows_it != rows.end()) {
    return stdx::unexpected(MysqlError(1, "Too many rows", "HY000"));
  }

  ++res_it;
  if (res_it != results.end()) {
    return stdx::unexpected(MysqlError(1, "Too many results", "HY000"));
  }

  return out;
}

// convert a string to a number
stdx::expected<uint64_t, std::error_code> from_string(std::string_view sv) {
  uint64_t num;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), num);

  if (ec != std::errc{}) return stdx::unexpected(make_error_code(ec));

  return num;
}

// get the pfs-events executed on a connection.
stdx::expected<std::vector<std::pair<std::string, uint32_t>>, MysqlError>
changed_event_counters_impl(MysqlClient &cli, std::string_view stmt) {
  auto query_res = cli.query(stmt);
  if (!query_res) return stdx::unexpected(query_res.error());

  auto query_it = query_res->begin();

  if (!(query_it != query_res->end())) {
    return stdx::unexpected(MysqlError(1234, "No resultset", "HY000"));
  }

  if (2 != query_it->field_count()) {
    return stdx::unexpected(MysqlError(1234, "Expected two fields", "HY000"));
  }

  std::vector<std::pair<std::string, uint32_t>> events;

  for (auto *row : query_it->rows()) {
    auto num_res = from_string(row[1]);
    if (!num_res) {
      return stdx::unexpected(MysqlError(
          1234,
          "converting " + std::string(row[1] != nullptr ? row[1] : "<NULL>") +
              " to an <uint32_t> failed",
          "HY000"));
    }

    events.emplace_back(row[0], *num_res);
  }

  return events;
}

stdx::expected<std::vector<std::pair<std::string, uint32_t>>, MysqlError>
changed_event_counters(MysqlClient &cli) {
  return changed_event_counters_impl(cli, R"(SELECT EVENT_NAME, COUNT_STAR
 FROM performance_schema.events_statements_summary_by_thread_by_event_name AS e
 JOIN performance_schema.threads AS t ON (e.THREAD_ID = t.THREAD_ID)
WHERE t.PROCESSLIST_ID = CONNECTION_ID()
  AND COUNT_STAR > 0
ORDER BY EVENT_NAME)");
}

}  // namespace

struct ShareConnectionParam {
  std::string testname;

  std::string_view client_ssl_mode;
  std::string_view server_ssl_mode;

  [[nodiscard]] bool can_reuse() const {
    return !((client_ssl_mode == kPreferred && server_ssl_mode == kAsClient) ||
             client_ssl_mode == kPassthrough);
  }

  [[nodiscard]] bool can_pool_connection_at_close() const {
    return !(client_ssl_mode == kPassthrough);
  }

  [[nodiscard]] bool can_share() const {
    return !((client_ssl_mode == kPreferred && server_ssl_mode == kAsClient) ||
             client_ssl_mode == kPassthrough);
  }

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

const ShareConnectionParam share_connection_params[] = {
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

const std::array is_tcp_values = {
    true,
#ifndef _WIN32
    // no unix-socket support on windows.
    false,
#endif
};

class SharedRouter {
 public:
  SharedRouter(TcpPortPool &port_pool, uint64_t pool_size)
      : port_pool_(port_pool),
        pool_size_{pool_size},
        rest_port_{port_pool_.get_next_available()} {}

  integration_tests::Procs &process_manager() { return procs_; }

  template <size_t N>
  static std::vector<mysql_harness::Destination>
  tcp_destinations_from_shared_servers(
      const std::array<SharedServer *, N> &servers) {
    std::vector<mysql_harness::Destination> dests;

    dests.reserve(servers.size());

    for (const auto &srv : servers) {
      dests.push_back(srv->classic_tcp_destination());
    }

    return dests;
  }

  template <size_t N>
  static std::vector<mysql_harness::Destination>
  local_destinations_from_shared_servers(
      const std::array<SharedServer *, N> &servers) {
    std::vector<mysql_harness::Destination> dests;

    dests.reserve(servers.size());

    for (const auto &srv : servers) {
      dests.push_back(srv->classic_socket_destination());
    }

    return dests;
  }

  void spawn_router(
      const std::vector<mysql_harness::Destination> &tcp_destinations,
      const std::vector<mysql_harness::Destination> &local_destinations) {
    auto userfile = conf_dir_.file("userfile");
    {
      std::ofstream ofs(userfile);
      // user:pass
      ofs << "user:$5$Vh2PFa7xfiEyPgFW$gGRTa6Hr9mRGBpxm4ATyfrfIY5ghAnqa."
             "YJgciRvb69";
    }

    auto writer = process_manager().config_writer(conf_dir_.name());

    writer
        .section(
            "connection_pool",
            {
                // must be large enough for one connection per routing-section
                {"max_idle_server_connections", std::to_string(pool_size_)},
            })
        .section("rest_connection_pool",
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
        .section("http_server", {{"bind_address", "127.0.0.1"},
                                 {"port", std::to_string(rest_port_)}});

    auto make_destinations =
        [](const std::vector<mysql_harness::Destination> &destinations) {
          std::string dests;
          bool is_first = true;
          for (const auto &dest : destinations) {
            if (is_first) {
              is_first = false;
            } else {
              dests += ",";
            }

            if (dest.is_local()) {
              dests += "local:";
#ifdef _WIN32
              // the path is absolute and starts with the drive-letter, but the
              // URI's path needs to start with '/'
              dests += "/";
#endif
            }

            dests += dest.str();
          }
          return dests;
        };

    for (const auto &param : share_connection_params) {
      for (bool is_tcp : is_tcp_values) {
        auto port_key = std::make_tuple(param.client_ssl_mode,
                                        param.server_ssl_mode, is_tcp);
        auto ports_it = ports_.find(port_key);

        const auto port =
            ports_it == ports_.end()
                ? (ports_[port_key] = port_pool_.get_next_available())
                : ports_it->second;

        writer.section(
            "routing:classic_" + param.testname + (is_tcp ? "_tcp" : "_unix"),
            {
                {"bind_port", std::to_string(port)},
                {"destinations",
                 make_destinations(is_tcp ? tcp_destinations
                                          : local_destinations)},
                {"protocol", "classic"},
                {"routing_strategy", "round-robin"},

                {"client_ssl_mode", std::string(param.client_ssl_mode)},
                {"server_ssl_mode", std::string(param.server_ssl_mode)},

                {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
                {"client_ssl_cert",
                 SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
                {"connection_sharing", "1"},
                {"connection_sharing_delay", "0"},
                {"connect_retry_timeout", "0"},
            });
      }
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
      process_manager().dump_logs();

      GTEST_SKIP() << "router failed to start";
    }
  }

  [[nodiscard]] auto host() const { return router_host_; }

  [[nodiscard]] uint16_t port(const ShareConnectionParam &param,
                              bool is_tcp) const {
    return ports_.at(
        std::make_tuple(param.client_ssl_mode, param.server_ssl_mode, is_tcp));
  }

  [[nodiscard]] auto rest_port() const { return rest_port_; }
  [[nodiscard]] auto rest_user() const { return rest_user_; }
  [[nodiscard]] auto rest_pass() const { return rest_pass_; }

  void populate_connection_pool(const ShareConnectionParam &param,
                                bool is_tcp) {
    // assuming round-robin: add one connection per destination of the route
    using pool_size_type = decltype(pool_size_);
    const pool_size_type num_destinations{3};

    for (pool_size_type ndx{}; ndx < num_destinations; ++ndx) {
      MysqlClient cli;

      cli.username("root");
      cli.password("");

      ASSERT_NO_ERROR(cli.connect(host(), port(param, is_tcp)));
    }

    // wait for the connections appear in the pool.
    if (param.can_share()) {
      ASSERT_NO_ERROR(wait_for_idle_server_connections(
          std::min(num_destinations, pool_size_), 10s));
    }
  }

  stdx::expected<int, std::error_code> rest_get_int(
      const std::string &uri, const std::string &pointer) {
    JsonDocument json_doc;

    fetch_json(rest_client_, uri, json_doc);

    if (auto *v = JsonPointer(pointer).Get(json_doc)) {
      if (!v->IsInt()) {
        return stdx::unexpected(make_error_code(std::errc::invalid_argument));
      }
      return v->GetInt();
    }

    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  stdx::expected<int, std::error_code> idle_server_connections() {
    return rest_get_int(rest_api_basepath + "/connection_pool/main/status",
                        "/idleServerConnections");
  }

  stdx::expected<int, std::error_code> stashed_server_connections() {
    return rest_get_int(rest_api_basepath + "/connection_pool/main/status",
                        "/stashedServerConnections");
  }

  stdx::expected<void, std::error_code> wait_for_idle_server_connections(
      int expected_value, std::chrono::seconds timeout) {
    using clock_type = std::chrono::steady_clock;

    const auto end_time = clock_type::now() + timeout;
    do {
      auto int_res = idle_server_connections();
      if (!int_res) return stdx::unexpected(int_res.error());

      if (*int_res == expected_value) return {};

      if (clock_type::now() > end_time) {
        return stdx::unexpected(make_error_code(std::errc::timed_out));
      }

      std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
    } while (true);
  }

  stdx::expected<void, std::error_code> wait_for_stashed_server_connections(
      int expected_value, std::chrono::seconds timeout) {
    using clock_type = std::chrono::steady_clock;

    const auto end_time = clock_type::now() + timeout;
    do {
      auto int_res = stashed_server_connections();
      if (!int_res) return stdx::unexpected(int_res.error());

      if (*int_res == expected_value) return {};

      if (clock_type::now() > end_time) {
        std::cerr << "expected " << expected_value << ", got " << *int_res
                  << "\n";
        return stdx::unexpected(make_error_code(std::errc::timed_out));
      }

      std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
    } while (true);
  }

 private:
  integration_tests::Procs procs_;
  TcpPortPool &port_pool_;

  TempDirectory conf_dir_;

  static const constexpr char router_host_[] = "127.0.0.1";
  std::map<std::tuple<std::string_view, std::string_view, bool>, uint16_t>
      ports_;

  uint64_t pool_size_;

  uint16_t rest_port_;

  IOContext rest_io_ctx_;
  RestClient rest_client_{rest_io_ctx_, "127.0.0.1", rest_port_, rest_user_,
                          rest_pass_};

  static constexpr const char rest_user_[] = "user";
  static constexpr const char rest_pass_[] = "pass";
};

class SharedRestartableRouter {
 public:
  SharedRestartableRouter(TcpPortPool &port_pool)
      : port_(port_pool.get_next_available()) {}

  integration_tests::Procs &process_manager() { return procs_; }

  void spawn_router(const std::vector<std::string> &destinations) {
    auto writer = process_manager().config_writer(conf_dir_.name());

    writer.section("routing:intermediate",
                   {
                       {"bind_port", std::to_string(port_)},
                       {"destinations", mysql_harness::join(destinations, ",")},
                       {"protocol", "classic"},
                       {"routing_strategy", "round-robin"},

                       {"client_ssl_mode", "PASSTHROUGH"},
                       {"server_ssl_mode", "AS_CLIENT"},

                       {"connection_sharing", "0"},
                   });

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

    is_running_ = true;
  }

  auto host() const { return router_host_; }

  uint16_t port() const { return port_; }

  void shutdown() {
    procs_.shutdown_all();

    is_running_ = false;
  }

  [[nodiscard]] bool is_running() const { return is_running_; }

 private:
  integration_tests::Procs procs_;

  TempDirectory conf_dir_;

  static const constexpr char router_host_[] = "127.0.0.1";

  uint16_t port_{};

  bool is_running_{false};
};

/* test environment.
 *
 * spawns servers for the tests.
 */
class TestEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    auto account = SharedServer::admin_account();

    for (auto [ndx, s] : stdx::views::enumerate(shared_servers_)) {
      if (s != nullptr) continue;
      s = new SharedServer(port_pool_);
      s->prepare_datadir();
      s->spawn_server();

      if (s->mysqld_failed_to_start()) {
        GTEST_SKIP() << "mysql-server failed to start.";
      }

      auto *cli = new MysqlClient;

      cli->username(account.username);
      cli->password(account.password);

      auto connect_res = cli_connect(*cli, s->classic_tcp_destination());
      ASSERT_NO_ERROR(connect_res);

      // install plugin that will be used later with setup_mysqld_accounts.
      auto install_res = SharedServer::local_install_plugin(
          *cli, "authentication_openid_connect");
      if (install_res) s->has_openid_connect(true);

      if (s->has_openid_connect()) {
        ASSERT_NO_ERROR(SharedServer::local_set_openid_connect_config(*cli));

        auto account = SharedServer::openid_connect_account();

        ASSERT_NO_FATAL_FAILURE(SharedServer::create_account(*cli, account));
        ASSERT_NO_FATAL_FAILURE(SharedServer::grant_access(
            *cli, account, "SELECT", "performance_schema"));
      }

      SharedServer::setup_mysqld_accounts(*cli);

      admin_clis_[ndx] = cli;
    }

    run_slow_tests_ = std::getenv("RUN_SLOW_TESTS") != nullptr;
  }

  std::array<SharedServer *, 4> servers() { return shared_servers_; }
  std::array<MysqlClient *, 4> admin_clis() { return admin_clis_; }

  TcpPortPool &port_pool() { return port_pool_; }

  [[nodiscard]] bool run_slow_tests() const { return run_slow_tests_; }

  void TearDown() override {
    for (auto &cli : admin_clis_) {
      if (cli == nullptr) continue;

      delete cli;

      cli = nullptr;
    }

    for (auto &s : shared_servers_) {
      if (s == nullptr || s->mysqld_failed_to_start()) continue;

      EXPECT_NO_ERROR(s->shutdown());
    }

    for (auto &s : shared_servers_) {
      if (s == nullptr || s->mysqld_failed_to_start()) continue;

      EXPECT_NO_ERROR(s->process_manager().wait_for_exit());
    }

    for (auto &s : shared_servers_) {
      delete s;

      s = nullptr;
    }

    SharedServer::destroy_statics();
  }

 protected:
  TcpPortPool port_pool_;

  std::array<SharedServer *, 4> shared_servers_{};
  std::array<MysqlClient *, 4> admin_clis_{};

  bool run_slow_tests_{false};
};

TestEnv *test_env{};

/* test-suite with shared routers.
 */
class TestWithSharedRouter {
 public:
  template <size_t N>
  static void SetUpTestSuite(TcpPortPool &port_pool,
                             const std::array<SharedServer *, N> &servers,
                             uint64_t pool_size) {
    for (const auto &s : servers) {
      if (s->mysqld_failed_to_start()) GTEST_SKIP();
    }

    if (shared_router_ == nullptr) {
      shared_router_ = new SharedRouter(port_pool, pool_size);

      SCOPED_TRACE("// spawn router");
      shared_router_->spawn_router(
          SharedRouter::tcp_destinations_from_shared_servers(servers),
          SharedRouter::local_destinations_from_shared_servers(servers));
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

class ShareConnectionTestBase : public RouterComponentTest {
 public:
  static constexpr const size_t kNumServers = 3;
  static constexpr const size_t kMaxPoolSize = 128;

  static void SetUpTestSuite() {
    for (const auto &s : shared_servers()) {
      if (s->mysqld_failed_to_start()) GTEST_SKIP();
    }

    TestWithSharedRouter::SetUpTestSuite(test_env->port_pool(),
                                         shared_servers(), kMaxPoolSize);
  }

  static void TearDownTestSuite() { TestWithSharedRouter::TearDownTestSuite(); }

  static std::array<SharedServer *, kNumServers> shared_servers() {
    std::array<SharedServer *, kNumServers> o;

    // get a subset of the started servers
    for (auto [ndx, s] : stdx::views::enumerate(test_env->servers())) {
      if (ndx >= kNumServers) break;

      o[ndx] = s;
    }

    return o;
  }

  static std::array<MysqlClient *, kNumServers> admin_clis() {
    std::array<MysqlClient *, kNumServers> o;

    // get a subset of the started servers
    for (auto [ndx, s] : stdx::views::enumerate(test_env->admin_clis())) {
      if (ndx >= kNumServers) break;

      o[ndx] = s;
    }

    return o;
  }

  static void reset_caching_sha2_cache() {
    for (auto *cli : admin_clis()) {
      ASSERT_NO_FATAL_FAILURE(SharedServer::flush_privileges(*cli));
    }
  }

  static void reset_router_connection_pool() {
    for (auto *cli : admin_clis()) {  // reset the router's connection-pool
      ASSERT_NO_FATAL_FAILURE(SharedServer::close_all_connections(*cli));
    }
  }

  SharedRouter *shared_router() { return TestWithSharedRouter::router(); }

  ~ShareConnectionTestBase() override {
    if (::testing::Test::HasFailure()) {
      shared_router()->process_manager().dump_logs();
      for (auto &srv : shared_servers()) {
        srv->process_manager().dump_logs();
      }
    }
  }

  static bool can_auth_with_caching_sha2_password_with_password(
      const ShareConnectionParam &param, bool is_tcp) {
    if (!is_tcp) return !(param.client_ssl_mode == kDisabled);

    return !(param.client_ssl_mode == kDisabled &&
             (param.server_ssl_mode == kPreferred ||
              param.server_ssl_mode == kRequired));
  }

  // with client-ssl-mode DISABLED, router doesn't have a public-key or a
  // tls connection to the client.
  //
  // The client will ask for the server's public-key instead which the
  // server will treat as "password" and then fail to authenticate.
  static bool can_auth_with_sha256_password_with_password(
      const ShareConnectionParam &param, bool is_tcp) {
    if (!is_tcp) {
      return !(param.client_ssl_mode == kDisabled &&
               param.server_ssl_mode == kRequired);
    }

    return !(param.client_ssl_mode == kDisabled &&
             (param.server_ssl_mode == kPreferred ||
              param.server_ssl_mode == kRequired));
  }

  static bool can_auth(const SharedServer::Account &account, const auto &param,
                       bool is_tcp, bool client_is_secure = true) {
    if (account.auth_method == "caching_sha2_password") {
      if (!client_is_secure && !is_tcp &&
          param.client_ssl_mode == kPassthrough) {
        if (!account.password.empty()) {
          // client asks for public-key, but server side is encrypted and
          // will not provide a public-key
          return false;
        }
      }

      return account.password.empty() ||
             can_auth_with_caching_sha2_password_with_password(param, is_tcp);
    }

    if (account.auth_method == "sha256_password") {
      return account.password.empty() ||
             can_auth_with_sha256_password_with_password(param, is_tcp);
    }

    return true;
  }

 protected:
  const std::string valid_ssl_key_{SSL_TEST_DATA_DIR "/server-key-sha512.pem"};
  const std::string valid_ssl_cert_{SSL_TEST_DATA_DIR
                                    "/server-cert-sha512.pem"};

  const std::string wrong_password_{"wrong_password"};
  const std::string empty_password_;
};

class ShareConnectionTest : public ShareConnectionTestBase,
                            public ::testing::WithParamInterface<
                                std::tuple<ShareConnectionParam, bool>> {
 public:
  void SetUp() override {
#ifdef _WIN32
    auto is_tcp = std::get<1>(GetParam());

    if (!is_tcp) {
      GTEST_SKIP() << "unix-sockets are not supported on windows.";
    }
#endif

    for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
      // shared_server_ may be null if TestWithSharedServer::SetUpTestSuite
      // threw?
      if (s == nullptr || s->mysqld_failed_to_start()) {
        GTEST_SKIP() << "failed to start mysqld";
      } else {
        auto *cli = admin_clis()[ndx];

        // reset the router's connection-pool
        ASSERT_NO_ERROR(SharedServer::close_all_connections(*cli));
        SharedServer::reset_to_defaults(*cli);
      }
    }
  }

  static bool can_auth(const SharedServer::Account &account,
                       bool client_is_secure = true) {
    auto [param, is_tcp] = GetParam();

    return ShareConnectionTestBase::can_auth(account, param, is_tcp,
                                             client_is_secure);
  }

 protected:
  using clock_type = std::chrono::steady_clock;

  clock_type::time_point started_{clock_type::now()};
};

TEST_P(ShareConnectionTest,
       classic_protocol_share_after_connect_openid_connect) {
#ifdef SKIP_AUTHENTICATION_CLIENT_PLUGINS_TESTS
  GTEST_SKIP() << "built with WITH_AUTHENTICATION_CLIENT_PLUGINS=OFF";
#endif

  if (!shared_servers()[0]->has_openid_connect()) GTEST_SKIP();

  RecordProperty("Worklog", "16466");
  RecordProperty("Requirement", "FR5");
  RecordProperty("Description",
                 "check that connection via openid_connect can be shared if "
                 "the connection is encrypted, and fails otherwise.");

  SCOPED_TRACE("// create the JWT token for authentication.");
  TempDirectory jwtdir;
  auto id_token_res = create_openid_connect_id_token_file(
      "openid_user1",                  // subject
      "https://myissuer.com",          // ${identity_provider}.name
      120,                             // expiry in seconds
      CMAKE_SOURCE_DIR                 //
      "/router/tests/component/data/"  //
      "openid_key.pem",                // private-key of the identity-provider
      jwtdir.name()                    // out-dir
  );
  ASSERT_NO_ERROR(id_token_res);

  auto id_token = *id_token_res;

  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 4> clis;

  std::array accounts{SharedServer::openid_connect_account(),
                      SharedServer::openid_connect_account(),
                      SharedServer::openid_connect_account(),
                      SharedServer::openid_connect_account()};

  auto [param, is_tcp] = GetParam();

  const bool can_share = param.can_share();
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    auto account = accounts[ndx];

    cli.set_option(MysqlClient::PluginDir(plugin_output_directory().c_str()));

    SCOPED_TRACE("// set the JWT-token in the plugin.");
    // set the id-token-file path
    auto plugin_res = cli.find_plugin("authentication_openid_connect_client",
                                      MYSQL_CLIENT_AUTHENTICATION_PLUGIN);
    ASSERT_NO_ERROR(plugin_res) << "plugin not found :(";

    plugin_res->set_option(
        MysqlClient::Plugin::StringOption("id-token-file", id_token.c_str()));

    cli.username(account.username);
    cli.password(account.password);

    // wait until connection 0, 1, 2 are in the pool as 3 shall share with 0.
    if (ndx == 3 && can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(3, 10s));
    }

    auto connect_res = cli.connect(shared_router()->host(),
                                   shared_router()->port(param, is_tcp));

    if (param.client_ssl_mode == kDisabled ||
        (is_tcp && param.server_ssl_mode == kDisabled)) {
      // should fail as the connection is not secure.
      ASSERT_ERROR(connect_res);
      if (is_tcp && (param.server_ssl_mode == kDisabled ||
                     param.server_ssl_mode == kAsClient)) {
        EXPECT_EQ(connect_res.error().value(), 1045);
      } else {
        EXPECT_EQ(connect_res.error().value(), 2000);
      }

      return;
    }

    ASSERT_NO_ERROR(connect_res);

    // connection goes out of the pool and back to the pool again.
    if (ndx == 3 && can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(3, 10s));
    }
  }

  // cli[0] and [3] share the same backend
  //
  // as connection-attributes differ between the connections
  // (router adds _client_port = ...) a change-user is needed whenever
  // client-connection changes.
  {
    auto events_res = changed_event_counters(clis[0]);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      // cli[0]
      // - connect
      // - set-option
      // cli[3]
      // - change-user
      // - set-option
      // cli[0]
      // - change-user
      // - set-option
      // - (+ select)
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Change user", 2),
                              Pair("statement/sql/select", 2),
                              Pair("statement/sql/set_option", 3)));
    } else {
      EXPECT_THAT(*events_res, ::testing::IsEmpty());
    }
  }

  // a fresh connection to host2
  {
    auto events_res = changed_event_counters(clis[1]);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 1)));
    } else {
      EXPECT_THAT(*events_res, ::testing::IsEmpty());
    }
  }

  // a fresh connection to host3
  {
    auto events_res = changed_event_counters(clis[2]);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 1)));
    } else {
      EXPECT_THAT(*events_res, ::testing::IsEmpty());
    }
  }

  // shared with cli1 on host1
  {
    auto events_res = changed_event_counters(clis[3]);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      // cli[0]
      // - connect
      // - set-option
      // cli[3]
      // - change-user
      // - set-option
      // cli[0]
      // - change-user
      // - set-option
      // - select
      // cli[3]
      // - change-user
      // - set-option
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Change user", 3),
                              Pair("statement/sql/select", 3),
                              Pair("statement/sql/set_option", 4)));
    } else {
      EXPECT_THAT(*events_res, ::testing::IsEmpty());
    }
  }
}

TEST_P(ShareConnectionTest,
       classic_protocol_openid_connect_expired_at_reconnect) {
#ifdef SKIP_AUTHENTICATION_CLIENT_PLUGINS_TESTS
  GTEST_SKIP() << "built with WITH_AUTHENTICATION_CLIENT_PLUGINS=OFF";
#endif

  if (!shared_servers()[0]->has_openid_connect()) GTEST_SKIP();

  RecordProperty("Worklog", "16466");
  RecordProperty("Requirement", "FR5");
  RecordProperty("Description",
                 "check that connection via openid_connect fails properly if "
                 "sharing is enabled and the id-token expires.");

  SCOPED_TRACE("// create the JWT token for authentication.");
  TempDirectory jwtdir;
  auto id_token_res = create_openid_connect_id_token_file(
      "openid_user1",                  // subject
      "https://myissuer.com",          // ${identity_provider}.name
      2,                               // expiry in seconds
      CMAKE_SOURCE_DIR                 //
      "/router/tests/component/data/"  //
      "openid_key.pem",                // private-key of the identity-provider
      jwtdir.name()                    // out-dir
  );
  ASSERT_NO_ERROR(id_token_res);

  auto id_token = *id_token_res;

  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 4> clis;

  auto account = SharedServer::openid_connect_account();

  auto [param, is_tcp] = GetParam();

  const bool can_share = param.can_share();
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    // plugin-dir for the openid-connect client plugin.
    cli.set_option(MysqlClient::PluginDir(plugin_output_directory().c_str()));

    SCOPED_TRACE("// set the JWT-token in the plugin.");
    // set the id-token-file path
    auto plugin_res = cli.find_plugin("authentication_openid_connect_client",
                                      MYSQL_CLIENT_AUTHENTICATION_PLUGIN);
    ASSERT_NO_ERROR(plugin_res) << "pluging not found :(";

    plugin_res->set_option(
        MysqlClient::Plugin::StringOption("id-token-file", id_token.c_str()));

    cli.username(account.username);
    cli.password(account.password);

    // wait until connection 0, 1, 2 are in the pool as 3 shall share with 0.
    if (ndx == 3 && can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(3, 10s));
    }

    auto connect_res = cli.connect(shared_router()->host(),
                                   shared_router()->port(param, is_tcp));

    if (param.client_ssl_mode == kDisabled ||
        (is_tcp && param.server_ssl_mode == kDisabled)) {
      // should fail as the connection is not secure.
      ASSERT_ERROR(connect_res);
      if (is_tcp && (param.server_ssl_mode == kDisabled ||
                     param.server_ssl_mode == kAsClient)) {
        EXPECT_EQ(connect_res.error().value(), 1045);
      } else {
        EXPECT_EQ(connect_res.error().value(), 2000);
      }

      return;
    }

    ASSERT_NO_ERROR(connect_res);

    // connection goes out of the pool and back to the pool again.
    if (ndx == 3 && can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(3, 10s));
    }
  }

  // wait a bit to expire the id-token.
  std::this_thread::sleep_for(3s);

  // clis[0] and clis[3] share the same server-connection
  //
  // The connection is currently owned by clis[3], and clis[1] wants to have it
  // back, and needs to reauthenticate. ... which should fail with due to the
  // expired id-token.

  auto events_res = changed_event_counters(clis[0]);
  if (can_share) {
    ASSERT_ERROR(events_res);
    EXPECT_EQ(events_res.error().value(), 1045);
    EXPECT_THAT(events_res.error().message(),
                testing::HasSubstr("while reauthenticating"));
  } else {
    ASSERT_NO_ERROR(events_res);
    EXPECT_THAT(*events_res, ::testing::IsEmpty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, ShareConnectionTest,
    ::testing::Combine(::testing::ValuesIn(share_connection_params),
                       ::testing::ValuesIn(is_tcp_values)),
    [](const auto &info) {
      auto param = std::get<0>(info.param);
      auto is_tcp = std::get<1>(info.param);

      return "ssl_modes_" + param.testname + (is_tcp ? "_tcp" : "_socket");
    });

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  // init openssl as otherwise libmysqlxclient may fail at SSL_CTX_new
  TlsLibraryContext tls_lib_ctx;

  // env is owned by googltest
  test_env =
      dynamic_cast<TestEnv *>(::testing::AddGlobalTestEnvironment(new TestEnv));

  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
