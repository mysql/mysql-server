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
#include <chrono>
#include <fstream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock-more-matchers.h>
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/pointer.h>

#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysql/harness/tls_context.h"
#include "mysql/harness/utility/string.h"  // join
#include "mysqlrouter/classic_protocol_codec_frame.h"
#include "mysqlrouter/classic_protocol_codec_message.h"
#include "mysqlrouter/classic_protocol_frame.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "openssl_version.h"  // ROUTER_OPENSSL_VERSION
#include "process_manager.h"
#include "procs.h"
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "shared_server.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

static constexpr const auto kIdleServerConnectionsSleepTime{10ms};

static constexpr const std::string_view kDisabled{"DISABLED"};
static constexpr const std::string_view kRequired{"REQUIRED"};
static constexpr const std::string_view kPreferred{"PREFERRED"};
static constexpr const std::string_view kPassthrough{"PASSTHROUGH"};
static constexpr const std::string_view kAsClient{"AS_CLIENT"};

namespace std {
// pretty printer for std::chrono::duration<>

template <class T, class R>
std::ostream &operator<<(std::ostream &os,
                         const std::chrono::duration<T, R> &duration) {
  return os << std::chrono::duration_cast<std::chrono::milliseconds>(duration)
                   .count()
            << "ms";
}
}  // namespace std

std::ostream &operator<<(std::ostream &os, MysqlError e) {
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

}  // namespace

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

// query a single row and return an array of N std::strings.
template <size_t N>
stdx::expected<std::array<std::string, N>, MysqlError> query_one(
    MysqlClient &cli, std::string_view stmt) {
  auto cmd_res = cli.query(stmt);
  if (!cmd_res) return stdx::unexpected(cmd_res.error());

  auto results = std::move(*cmd_res);

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
    } else {
      return stdx::unexpected(
          make_error_code(std::errc::no_such_file_or_directory));
    }
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

      auto cli = new MysqlClient;

      cli->username(account.username);
      cli->password(account.password);

      auto connect_res = cli_connect(*cli, s->classic_tcp_destination());
      ASSERT_NO_ERROR(connect_res);

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
      if (s != nullptr) delete s;

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

  static void reset_router_connection_pool(
      const std::vector<std::string> &usernames) {
    for (auto *cli : admin_clis()) {  // reset the router's connection-pool
      ASSERT_NO_FATAL_FAILURE(
          SharedServer::close_all_connections(*cli, usernames));
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

// sharable statements.

struct Event {
  Event(std::string_view type, std::string_view stmt)
      : type_(type), stmt_(stmt) {}

  static Event sql_select(std::string_view stmt) {
    return {"statement/sql/select", stmt};
  }

  static Event sql_set_option(std::string_view stmt) {
    return {"statement/sql/set_option", stmt};
  }

  static Event sql_lock_tables(std::string_view stmt) {
    return {"statement/sql/lock_tables", stmt};
  }

  static Event sql_unlock_tables(std::string_view stmt) {
    return {"statement/sql/unlock_tables", stmt};
  }

  static Event sql_flush(std::string_view stmt) {
    return {"statement/sql/flush", stmt};
  }

  static Event sql_lock_instance(std::string_view stmt) {
    return {"statement/sql/lock_instance", stmt};
  }

  static Event com_reset_connection() {
    return {"statement/com/Reset Connection", "<NULL>"};
  }

  static Event sql_begin(std::string_view stmt) {
    return {"statement/sql/begin", stmt};
  }

  static Event sql_rollback(std::string_view stmt) {
    return {"statement/sql/rollback", stmt};
  }

  static Event sql_do(std::string_view stmt) {
    return {"statement/sql/do", stmt};
  }

  static Event sql_commit(std::string_view stmt) {
    return {"statement/sql/commit", stmt};
  }

  static Event sql_drop_table(std::string_view stmt) {
    return {"statement/sql/drop_table", stmt};
  }

  static Event sql_create_table(std::string_view stmt) {
    return {"statement/sql/create_table", stmt};
  }

  static Event sql_prepare_sql(std::string_view stmt) {
    return {"statement/sql/prepare_sql", stmt};
  }

  static Event sql_show_warnings(std::string_view stmt) {
    return {"statement/sql/show_warnings", stmt};
  }

  friend bool operator==(const Event &lhs, const Event &rhs) {
    return lhs.type_ == rhs.type_ && lhs.stmt_ == rhs.stmt_;
  }

  friend std::ostream &operator<<(std::ostream &os, const Event &ev) {
    os << testing::PrintToString(std::pair(ev.type_, ev.stmt_));

    return os;
  }

 private:
  std::string type_;
  std::string stmt_;
};

static stdx::expected<std::vector<Event>, MysqlError> statement_history(
    MysqlClient &cli) {
  auto hist_res = query_one_result(
      cli,
      "SELECT event_name, digest_text "
      "  FROM performance_schema.events_statements_history AS h"
      "  JOIN performance_schema.threads AS t "
      "    ON (h.thread_id = t.thread_id)"
      " WHERE t.processlist_id = CONNECTION_ID()"
      " ORDER BY event_id");

  std::vector<Event> res;

  for (auto row : *hist_res) {
    res.emplace_back(row[0], row[1]);
  }

  return res;
}

struct Stmt {
  static Event select_session_vars() {
    return Event::sql_select(
        "SELECT ? , @@SESSION . `collation_connection` UNION "
        "SELECT ? , @@SESSION . `character_set_client` UNION "
        "SELECT ? , @@SESSION . `sql_mode`");
  }

  static Event set_session_tracker() {
    return Event::sql_set_option(
        "SET "
        "@@SESSION . `session_track_system_variables` = ? , "
        "@@SESSION . `session_track_gtids` = ? , "
        "@@SESSION . `session_track_schema` = ? , "
        "@@SESSION . `session_track_state_change` = ? , "
        "@@SESSION . `session_track_transaction_info` = ?");
  }

  static Event restore_session_vars() {
    return Event::sql_set_option(
        "SET "
        "@@SESSION . `character_set_client` = ? , "
        "@@SESSION . `collation_connection` = ? , "
        "@@SESSION . `sql_mode` = ?");
  }

  static Event select_history() {
    return Event::sql_select(
        "SELECT `event_name` , `digest_text` "
        "FROM `performance_schema` . `events_statements_history` AS `h` "
        "JOIN `performance_schema` . `threads` AS `t` "
        "ON ( `h` . `thread_id` = `t` . `thread_id` ) "
        "WHERE `t` . `processlist_id` = `CONNECTION_ID` ( ) "
        "ORDER BY `event_id`");
  }

  static Event select_wait_gtid() {
    return Event::sql_select("SELECT NOT `WAIT_FOR_EXECUTED_GTID_SET` (...)");
  }
};

struct StatementSharableParam {
  std::string test_name;

  std::string requirement_id;

  struct Ctx {
    const ShareConnectionParam &connect_param;

    MysqlClient &cli;
    SharedRouter *shared_router;
  };

  std::function<void(Ctx &ctx)> result;
};

class StatementSharableTest
    : public ShareConnectionTestBase,
      public ::testing::WithParamInterface<
          std::tuple<StatementSharableParam, ShareConnectionParam, bool>> {
 public:
  static void SetUpTestSuite() {
    ShareConnectionTestBase::SetUpTestSuite();

    for (auto &srv : shared_servers()) {
      if (srv->mysqld_failed_to_start()) {
        GTEST_SKIP() << "mysql-server failed to start.";
      }

      auto admin_cli_res = srv->admin_cli();
      ASSERT_NO_ERROR(admin_cli_res);
      auto admin_cli = std::move(*admin_cli_res);

      ASSERT_NO_ERROR(admin_cli.query("DROP TABLE IF EXISTS testing.t1"));
      ASSERT_NO_ERROR(admin_cli.query("CREATE TABLE testing.t1 (id INT)"));
    }
  }

  void SetUp() override {
#ifdef _WIN32
    auto is_tcp = std::get<2>(GetParam());

    if (!is_tcp) {
      GTEST_SKIP() << "unix-sockets are not supported on windows.";
    }
#endif

    for (auto &srv : shared_servers()) {
      if (srv->mysqld_failed_to_start()) {
        GTEST_SKIP() << "mysql-server failed to start.";
      }

      srv->close_all_connections();  // reset the router's connection-pool
    }
  }

  static void TearDownTestSuite() {
    for (auto &srv : shared_servers()) {
      if (srv->mysqld_failed_to_start()) {
        GTEST_SKIP() << "mysql-server failed to start.";
      }

      auto admin_cli_res = srv->admin_cli();
      ASSERT_NO_ERROR(admin_cli_res);
      auto admin_cli = std::move(*admin_cli_res);

      ASSERT_NO_ERROR(admin_cli.query("DROP TABLE IF EXISTS testing.t1"));
    }

    ShareConnectionTestBase::TearDownTestSuite();
  }

 protected:
};

TEST_P(StatementSharableTest, check) {
  auto [test_param, connect_param, is_tcp] = GetParam();

  auto account = SharedServer::caching_sha2_empty_password_account();

  MysqlClient cli;

  cli.set_option(MysqlClient::GetServerPublicKey(true));
  cli.username(account.username);
  cli.password(account.password);

  auto connect_res = cli.connect(shared_router()->host(),
                                 shared_router()->port(connect_param, is_tcp));
  ASSERT_NO_ERROR(connect_res);

  if (connect_param.can_share()) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  StatementSharableParam::Ctx ctx{connect_param, cli, shared_router()};
  test_param.result(ctx);
}

static const StatementSharableParam statement_sharable_params[] = {
    {"get_diagnostics",  //
     "FR7.1",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::string stmt = "GET DIAGNOSTICS @p1 = NUMBER";

       if (connect_param.can_share()) {
         auto query_res = cli.query(stmt);
         ASSERT_ERROR(query_res);
         EXPECT_EQ(query_res.error().value(), 3566) << query_res.error();
       } else {
         auto query_res = query_one_result(cli, stmt);
         ASSERT_NO_ERROR(query_res);
       }

       {
         auto query_res =
             cli.query("START TRANSACTION WITH CONSISTENT SNAPSHOT");
         ASSERT_NO_ERROR(query_res);
       }

       {
         auto query_res = query_one_result(cli, stmt);
         ASSERT_NO_ERROR(query_res);
       }

       {
         auto query_res = cli.query("COMMIT");
         ASSERT_NO_ERROR(query_res);
       }

       if (connect_param.can_share()) {
         ASSERT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 10s));
       }
     }},
    {"select_last_insert_id",  //
     "FR7.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::string stmt = "SELECT LAST_INSERT_ID()";

       if (connect_param.can_share()) {
         auto query_res = cli.query(stmt);
         ASSERT_ERROR(query_res);
         EXPECT_EQ(query_res.error().value(), 3566) << query_res.error();
       } else {
         auto query_res = query_one_result(cli, stmt);
         ASSERT_NO_ERROR(query_res);
       }

       {
         auto query_res =
             cli.query("START TRANSACTION WITH CONSISTENT SNAPSHOT");
         ASSERT_NO_ERROR(query_res);
       }

       {
         auto query_res = query_one_result(cli, stmt);
         ASSERT_NO_ERROR(query_res);
       }

       {
         auto query_res = cli.query("COMMIT");
         ASSERT_NO_ERROR(query_res);
       }

       if (connect_param.can_share()) {
         ASSERT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 10s));
       }
     }},
    {"start_trx_consistent_snapshot_commit",  //
     "FR5.1",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res =
             cli.query("START TRANSACTION WITH CONSISTENT SNAPSHOT");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_begin("START TRANSACTION WITH CONSISTENT SNAPSHOT"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.query("DO 1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_do("DO ?"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.query("COMMIT");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_commit("COMMIT"));
       }

       if (connect_param.can_share()) {
         // after COMMIT, sharing is possible again.
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},
    {"start_trx_consistent_snapshot_rollback",  //
     "FR5.1",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res =
             cli.query("START TRANSACTION WITH CONSISTENT SNAPSHOT");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_begin("START TRANSACTION WITH CONSISTENT SNAPSHOT"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.query("DO 1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_do("DO ?"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.query("rollback");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_rollback("ROLLBACK"));
       }

       if (connect_param.can_share()) {
         // after ROLLBACK, sharing is possible again.
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},
    {"start_trx_commit",  //
     "FR5.1",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res = cli.query("START TRANSACTION");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_begin("START TRANSACTION"));
       }

       if (connect_param.can_share()) {
         // after START TRANSACTION the trx-state is captured, but the
         // connection is still sharable.
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.query("COMMIT");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_commit("COMMIT"));
       }

       if (connect_param.can_share()) {
         // after COMMIT, sharing is possible again.
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},
    {"lock_tables",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res = cli.query("LOCK TABLES testing.t1 READ");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_lock_tables("LOCK TABLES `testing` . `t1` READ"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.query("UNLOCK TABLES");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_unlock_tables("UNLOCK TABLES"));
       }

       if (connect_param.can_share()) {
         // after UNLOCK TABLES, sharing is possible again.
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"flush_all_tables_with_read_lock",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res = cli.query("FLUSH TABLES WITH READ LOCK");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_flush("FLUSH TABLES WITH READ LOCK"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},
    {"flush_all_tables_with_read_lock_and_unlock",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res = cli.query("FLUSH TABLES WITH READ LOCK");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_flush("FLUSH TABLES WITH READ LOCK"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.query("UNLOCK TABLES");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_unlock_tables("UNLOCK TABLES"));
       }

       // does not unlock sharing.
       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection does.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"flush_some_tables_with_read_lock",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res = cli.query("FLUSH TABLES testing.t1 WITH READ LOCK");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_flush("FLUSH TABLES `testing` . `t1` WITH READ LOCK"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.query("UNLOCK TABLES");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_unlock_tables("UNLOCK TABLES"));
       }

       if (connect_param.can_share()) {
         ASSERT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 10s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       // ... but reset-connection does.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"flush_some_tables_for_export",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res = cli.query("FLUSH TABLES testing.t1 FOR export");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_flush("FLUSH TABLES `testing` . `t1` FOR EXPORT"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.query("UNLOCK TABLES");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_unlock_tables("UNLOCK TABLES"));
       }

       // ... unblocks sharing.

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       // ... reset-connection does too.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"lock_instance_for_backup",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res = cli.query("LOCK instance for Backup");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_lock_instance("LOCK INSTANCE FOR BACKUP"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection does.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"set_user_var_rollback",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res = cli.query("SET @user := 1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_set_option("SET @? := ?"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = cli.query("ROLLBACK");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_rollback("ROLLBACK"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection does.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"set_user_var_eq_reset",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res = cli.query("SET @user = 1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_set_option("SET @? = ?"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection does.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"set_user_var_assign_reset",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       {
         auto query_res = cli.query("SET @user := 1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_set_option("SET @? := ?"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection does.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"select_user_var_reset",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       // SELECT user-var blocks sharing.
       {
         auto query_res = query_one_result(cli, "SELECT @user := 1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_select("SELECT @? := ?"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection unblocks it.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"select_into_user_var_and_reset",  //
     "FR5.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       // SELECT INTO user-var ...
       {
         auto query_res = query_one_result(cli, "SELECT 1 INTO @user");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_select("SELECT ? INTO @?"));
       }

       // ... blocks sharing
       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection unblocks it.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"get_lock",  //
     "FR6.1",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       // DO GET_LOCK(...) ...
       {
         auto query_res = query_one_result(cli, "DO get_lock('abc', 0)");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_do("DO `get_lock` (...)"));
       }

       // ... blocks sharing
       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection unblocks it.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"service_get_write_lock",  //
     "FR6.1",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       // DO SERVICE_GET_WRITE_LOCKS(...) ...
       {
         auto query_res = query_one_result(
             cli, "DO service_get_WRITE_locks('ns', 'abc', 0)");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_do("DO `service_get_WRITE_locks` (...)"));
       }

       // ... blocks sharing
       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection unblocks it.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"service_get_read_lock",  //
     "FR6.1",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       // ... SERVICE_GET_WRITE_LOCKS(...) ...
       {
         auto query_res = query_one_result(
             cli, "SELECT service_get_READ_locks('ns', 'abc', 0)");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT `service_get_READ_locks` (...)"));
       }

       // ... blocks sharing
       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection unblocks it.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"create_temp_table",  //
     "FR6.2",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());
         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       // ... SERVICE_GET_WRITE_LOCKS(...) ...
       {
         auto query_res = query_one_result(
             cli, "create temporary table testing.temp ( id int )");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_create_table(
             "CREATE TEMPORARY TABLE `testing` . `temp` ( `id` INTEGER )"));
       }

       // ... blocks sharing
       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.temp");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `temp`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection unblocks it.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"prepare_stmt_reset",  //
     "FR6.3",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       //
       {
         auto query_res = query_one_result(cli, "prepare stmt from 'select 1'");
         ASSERT_NO_ERROR(query_res);

         // the prepared stmt text is not really matching the original text
         // anymore.
         expected_stmts.emplace_back(
             Event::sql_prepare_sql("PREPARE SELECT ?"));
       }

       // ... blocks sharing
       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection unblocks it.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

    {"sql_calc_found_rows",  //
     "FR6.4",
     [](StatementSharableParam::Ctx &ctx) {
       auto &cli = ctx.cli;
       const auto &connect_param = ctx.connect_param;
       SharedRouter *shared_router = ctx.shared_router;

       std::vector<Event> expected_stmts;

       if (connect_param.can_share()) {
         expected_stmts.emplace_back(Stmt::set_session_tracker());

         expected_stmts.emplace_back(Stmt::select_session_vars());
       }

       // SQL_CALC_FOUND_ROWS
       {
         auto query_res = query_one_result(
             cli, "SELECT SQL_CALC_FOUND_ROWS * FROM testing.t1 LIMIT 0");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::sql_select(
             "SELECT SQL_CALC_FOUND_ROWS * FROM `testing` . `t1` LIMIT ?"));
       }

       // ... blocks sharing
       EXPECT_EQ(0, shared_router->stashed_server_connections());

       {
         auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(
             Event::sql_select("SELECT * FROM `testing` . `t1`"));
       }

       EXPECT_EQ(0, shared_router->stashed_server_connections());

       // ... but reset-connection unblocks it.
       {
         auto query_res = cli.reset_connection();
         ASSERT_NO_ERROR(query_res);

         expected_stmts.emplace_back(Event::com_reset_connection());

         if (connect_param.can_share()) {
           expected_stmts.emplace_back(Stmt::set_session_tracker());
           expected_stmts.emplace_back(Stmt::select_session_vars());
         }
       }

       if (connect_param.can_share()) {
         EXPECT_NO_ERROR(
             shared_router->wait_for_stashed_server_connections(1, 2s));
       } else {
         EXPECT_EQ(0, shared_router->stashed_server_connections());
       }

       auto stmt_hist_res = statement_history(cli);
       ASSERT_NO_ERROR(stmt_hist_res);

       EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(expected_stmts));

       expected_stmts.emplace_back(Stmt::select_history());
     }},

};

INSTANTIATE_TEST_SUITE_P(
    Spec, StatementSharableTest,
    ::testing::Combine(::testing::ValuesIn(statement_sharable_params),
                       ::testing::ValuesIn(share_connection_params),
                       ::testing::ValuesIn(is_tcp_values)),
    [](auto &info) {
      return std::get<0>(info.param).test_name + "_via_" +
             std::get<1>(info.param).testname +
             (std::get<2>(info.param) ? "_tcp" : "_socket");
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
