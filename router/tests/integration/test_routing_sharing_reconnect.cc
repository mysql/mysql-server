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

#include "hexify.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/stdx/ranges.h"   // enumerate
#include "mysql/harness/string_utils.h"  // split_string
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

using ::testing::ElementsAre;
using ::testing::Pair;

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

// convert a string to a number
static stdx::expected<uint64_t, std::error_code> from_string(
    std::string_view sv) {
  uint64_t num;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), num);

  if (ec != std::errc{}) return stdx::unexpected(make_error_code(ec));

  return num;
}

// get the pfs-events executed on a connection.
static stdx::expected<std::vector<std::pair<std::string, uint32_t>>, MysqlError>
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

  for (auto row : query_it->rows()) {
    auto num_res = from_string(row[1]);
    if (!num_res) {
      return stdx::unexpected(
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

class ShareConnectionReconnectTest
    : public ShareConnectionTestBase,
      public ::testing::WithParamInterface<
          std::tuple<ShareConnectionParam, bool>> {
 public:
  void SetUp() override {
    auto [param, is_tcp] = GetParam();
#ifdef _WIN32
    if (!is_tcp) {
      GTEST_SKIP() << "unix-sockets are not supported on windows.";
    }
#endif

    SharedServer::Account account{
        "onetime",
        "",  // no password.
        "caching_sha2_password",
    };

    for (auto *cli : admin_clis()) {
      ASSERT_NO_ERROR(cli->query("DROP USER IF EXISTS " + account.username));

      SharedServer::create_account(*cli, account);
      SharedServer::grant_access(*cli, account, "SELECT", "testing");
    }

    // close all connections between router and server as the test assumes that
    // connections are stolen from the stash.
    auto usernames = SharedServer::default_usernames();
    usernames.emplace_back(account.username);

    auto idle_res = shared_router()->idle_server_connections();
    ASSERT_NO_ERROR(idle_res);
    if (*idle_res > 0) {
      ASSERT_NO_FATAL_FAILURE(reset_router_connection_pool(usernames));
      ASSERT_NO_ERROR(
          shared_router()->wait_for_idle_server_connections(0, 10s));
    }

    const bool can_share = param.can_share();

    for (auto [ndx, cli] : stdx::views::enumerate(clis_)) {
      SCOPED_TRACE("// connection [" + std::to_string(ndx) + "]");

      cli.username(account.username);
      cli.password(account.password);
      cli.set_option(MysqlClient::GetServerPublicKey(true));

      ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                  shared_router()->port(param, is_tcp)));

      if (can_share) {
        // the 4th connection will steal the 1st connection.
        size_t expected_stashed_connections = ndx < 3 ? ndx + 1 : 3;

        // wait until the connection is stashed.
        ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(
            expected_stashed_connections, 10s));
      }
    }

    SCOPED_TRACE(
        "// change the password of the 'onetime' user to force a reauth fail.");
    for (auto *cli : admin_clis()) {
      ASSERT_NO_ERROR(cli->query("ALTER USER " + account.username +
                                 " IDENTIFIED BY 'someotherpass'"));
    }
  }

  void TearDown() override {
    for (auto &cli : clis_) cli.close();

    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(0, 10s));
  }

 protected:
  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 4> clis_;
};

TEST_P(ShareConnectionReconnectTest, ping) {
  auto [param, is_tcp] = GetParam();

  const bool can_share = param.can_share();

  SCOPED_TRACE("// check if a changed password has handled properly.");

  auto &cli = clis_[0];
  const auto cmd_res = cli.ping();
  if (can_share) {
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1045);
    EXPECT_THAT(cmd_res.error().message(),
                testing::HasSubstr("while reauthenticating"));
  } else {
    ASSERT_NO_ERROR(cmd_res);
  }
}

TEST_P(ShareConnectionReconnectTest, query) {
  auto [param, is_tcp] = GetParam();

  const bool can_share = param.can_share();

  SCOPED_TRACE("// check if a changed password has handled properly.");

  auto &cli = clis_[0];
  const auto cmd_res = cli.query("DO 1");
  if (can_share) {
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1045);
    EXPECT_THAT(cmd_res.error().message(),
                testing::HasSubstr("while reauthenticating"));
  } else {
    ASSERT_NO_ERROR(cmd_res);
  }
}

TEST_P(ShareConnectionReconnectTest, list_schema) {
  auto [param, is_tcp] = GetParam();

  const bool can_share = param.can_share();

  SCOPED_TRACE("// check if a changed password has handled properly.");

  auto &cli = clis_[0];
  const auto cmd_res = cli.list_dbs();
  if (can_share) {
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1045);
    EXPECT_THAT(cmd_res.error().message(),
                testing::HasSubstr("while reauthenticating"));
  } else {
    ASSERT_NO_ERROR(cmd_res);
  }
}

TEST_P(ShareConnectionReconnectTest, stat) {
  auto [param, is_tcp] = GetParam();

  const bool can_share = param.can_share();

  SCOPED_TRACE("// check if a changed password has handled properly.");

  auto &cli = clis_[0];
  const auto cmd_res = cli.stat();
  if (can_share) {
    // returns the error-msg as success ... mysql_stat() is a bit special.
    ASSERT_NO_ERROR(cmd_res);
    EXPECT_THAT(*cmd_res, testing::HasSubstr("while reauthenticating"));
  } else {
    ASSERT_NO_ERROR(cmd_res);
    EXPECT_THAT(*cmd_res,
                testing::Not(testing::HasSubstr("while reauthenticating")));
  }
}

TEST_P(ShareConnectionReconnectTest, init_schema) {
  auto [param, is_tcp] = GetParam();

  const bool can_share = param.can_share();

  SCOPED_TRACE("// check if a changed password has handled properly.");

  auto &cli = clis_[0];
  const auto cmd_res = cli.use_schema("testing");
  if (can_share) {
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1045);
    EXPECT_THAT(cmd_res.error().message(),
                testing::HasSubstr("while reauthenticating"));
  } else {
    ASSERT_NO_ERROR(cmd_res);
  }
}

TEST_P(ShareConnectionReconnectTest, reset_connection) {
  auto [param, is_tcp] = GetParam();

  const bool can_share = param.can_share();

  SCOPED_TRACE("// check if a changed password has handled properly.");

  auto &cli = clis_[0];
  const auto cmd_res = cli.reset_connection();
  if (can_share) {
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1045);
    EXPECT_THAT(cmd_res.error().message(),
                testing::HasSubstr("while reauthenticating"));
  } else {
    ASSERT_NO_ERROR(cmd_res);
  }
}

TEST_P(ShareConnectionReconnectTest, prepare_stmt) {
  auto [param, is_tcp] = GetParam();

  const bool can_share = param.can_share();

  SCOPED_TRACE("// check if a changed password has handled properly.");

  auto &cli = clis_[0];
  const auto cmd_res = cli.prepare("DO 1");
  if (can_share) {
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1045) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                testing::HasSubstr("while reauthenticating"));
  } else {
    ASSERT_NO_ERROR(cmd_res);
  }
}

TEST_P(ShareConnectionReconnectTest, change_user) {
  auto [param, is_tcp] = GetParam();

  SCOPED_TRACE("// check if a change_user has handled properly.");

  auto &cli = clis_[0];
  const auto cmd_res = cli.change_user("onetime", "someotherpass", "");
  if (param.client_ssl_mode == kDisabled &&
      (param.server_ssl_mode == kRequired ||
       param.server_ssl_mode == kPreferred || !is_tcp)) {
    // caching-sha2-password needs a secure-channel on the client side too if
    // the server side is secure (Required/Preferred/unix-socket)
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1045) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                testing::HasSubstr("while reauthenticating"));
  } else {
    ASSERT_NO_ERROR(cmd_res);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, ShareConnectionReconnectTest,
    ::testing::Combine(::testing::ValuesIn(share_connection_params),
                       ::testing::ValuesIn(is_tcp_values)),
    [](auto &info) {
      auto param = std::get<0>(info.param);
      auto is_tcp = std::get<1>(info.param);

      return "ssl_modes_" + param.testname + (is_tcp ? "_tcp" : "_socket");
    });

struct ChangeUserParam {
  std::string scenario;

  SharedServer::Account account;

  std::function<bool(bool, ShareConnectionParam)> expect_success;
};

static const ChangeUserParam change_user_params[] = {
    {"caching_sha2_empty_password",
     SharedServer::caching_sha2_empty_password_account(),
     [](bool, auto) { return true; }},
    {"caching_sha2_password", SharedServer::caching_sha2_password_account(),

     [](bool with_ssl, auto connect_param) {
       return with_ssl && connect_param.client_ssl_mode != kDisabled;
     }},
    {"sha256_empty_password", SharedServer::sha256_empty_password_account(),
     [](bool, auto) { return true; }},
    {"sha256_password", SharedServer::sha256_password_account(),

     [](bool, auto connect_param) {
       return connect_param.client_ssl_mode != kDisabled;
     }},
};

/*
 * test combinations of "change-user".
 *
 * - client's --ssl-mode=DISABLED|PREFERRED
 * - router's client_ssl_mode,server_ssl_mode
 * - authentication-methods caching-sha2-password and sha256_password
 * - with and without a schema.
 *
 * reuses the connection to the router if all ssl-mode's stay the same.
 */
class ChangeUserTest
    : public ShareConnectionTestBase,
      public ::testing::WithParamInterface<std::tuple<
          bool, bool, ShareConnectionParam, ChangeUserParam, std::string>> {
 public:
  void SetUp() override {
#ifdef _WIN32
    auto is_tcp = std::get<1>(GetParam());

    if (!is_tcp) {
      GTEST_SKIP() << "unix-sockets are not supported on windows.";
    }
#endif

    for (auto &s : shared_servers()) {
      if (s->mysqld_failed_to_start()) {
        GTEST_SKIP() << "mysql-server failed to start.";
      }
    }
  }

  static void TearDownTestSuite() {
    cli_.reset();
    ShareConnectionTestBase::TearDownTestSuite();
  }

  static bool can_auth(const SharedServer::Account &account) {
    auto [client_is_secure, is_tcp, connect_param, test_param, schema] =
        GetParam();

    return ShareConnectionTestBase::can_auth(account, connect_param, is_tcp,
                                             client_is_secure);
  }

 protected:
  static std::unique_ptr<MysqlClient> cli_;
  static bool last_with_ssl_;
  static bool last_is_tcp_;
  static ShareConnectionParam last_connect_param_;
  static int expected_change_user_;
  static int expected_reset_connection_;
  static int expected_select_;
  static int expected_set_option_;
};

std::unique_ptr<MysqlClient> ChangeUserTest::cli_{};
bool ChangeUserTest::last_with_ssl_{};
bool ChangeUserTest::last_is_tcp_{};
ShareConnectionParam ChangeUserTest::last_connect_param_{};
int ChangeUserTest::expected_change_user_{0};
int ChangeUserTest::expected_reset_connection_{0};
int ChangeUserTest::expected_select_{0};
int ChangeUserTest::expected_set_option_{0};

TEST_P(ChangeUserTest, classic_protocol) {
  auto [with_ssl, is_tcp, connect_param, test_param, schema] = GetParam();

  auto [name, account, expect_success_func] = test_param;

  auto expect_success = expect_success_func(with_ssl, connect_param);

  const bool can_share = connect_param.can_share();
  // if the password is empty, it is known, always.
  //
  // otherwise it can be fetched at change-user if there is:
  //
  // - SSL or
  // - a public-key (!DISABLED)
  const bool can_fetch_password =
      (account.password.empty() || connect_param.client_ssl_mode != kDisabled);

  if (!with_ssl && connect_param.client_ssl_mode == kRequired) {
    // invalid combination.
    return;
  }

  // drop the connection if it doesn't match the "SSL" needs.
  if (cli_ &&
      (with_ssl != last_with_ssl_ ||
       last_connect_param_.client_ssl_mode != connect_param.client_ssl_mode ||
       last_connect_param_.server_ssl_mode != connect_param.server_ssl_mode ||
       is_tcp != last_is_tcp_)) {
    cli_.reset();
  }

  if (!cli_) {
    // flush the pool to ensure the test can for "wait_for_pooled_connection(1)"
    ASSERT_NO_FATAL_FAILURE(reset_router_connection_pool());

    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));

    cli_ = std::make_unique<MysqlClient>();

    cli_->set_option(MysqlClient::GetServerPublicKey(true));
    if (!with_ssl) {
      cli_->set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    }
    cli_->username("root");
    cli_->password("");
    last_with_ssl_ = with_ssl;
    last_connect_param_ = connect_param;
    last_is_tcp_ = is_tcp;

    ASSERT_NO_ERROR(cli_->connect(
        shared_router()->host(), shared_router()->port(connect_param, is_tcp)));

    expected_reset_connection_ = 0;
    expected_select_ = 0;
    expected_set_option_ = 0;
    expected_change_user_ = 0;

    if (can_share) {
      expected_set_option_ += 1;  // SET session-track-system-vars
      expected_select_ += 1;      // SELECT collation
    }
  }

  if (account.auth_method == "caching_sha2_password") {
    ASSERT_NO_FATAL_FAILURE(reset_caching_sha2_cache());
  }

  {
    auto cmd_res =
        cli_->change_user(account.username, account.password, schema);

    expected_change_user_ += 1;
    if (can_share) {
      expected_set_option_ += 1;  // SET session-track-system-vars
      if (can_fetch_password) {
        expected_select_ += 1;  // SELECT collation
      }
    }

    if (!can_auth(account)) {
      ASSERT_ERROR(cmd_res);

      cli_.reset();

      return;
    }

    ASSERT_NO_ERROR(cmd_res);

    {
      // no warnings.
      auto warning_res = cli_->warning_count();
      ASSERT_NO_ERROR(warning_res);
      EXPECT_EQ(*warning_res, 0);
    }

    if (can_share && expect_success) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }

    if (can_share && can_fetch_password) {
      // expected_reset_connection_ += 1;
      // expected_set_option_ += 1;
    }

    {
      auto cmd_res = query_one_result(*cli_, "SELECT USER(), SCHEMA()");
      ASSERT_NO_ERROR(cmd_res);

      EXPECT_THAT(*cmd_res,
                  ElementsAre(ElementsAre(account.username + "@localhost",
                                          schema.empty() ? "<NULL>" : schema)));
    }

    expected_select_ += 1;
  }

  {
    auto events_res = changed_event_counters(*cli_);
    ASSERT_NO_ERROR(events_res);

    if (can_share && can_fetch_password) {
      // expected_reset_connection_ += 1;
      // expected_set_option_ += 1;
    }

    if (expected_reset_connection_ > 0) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Change user", expected_change_user_),
                      Pair("statement/com/Reset Connection",
                           expected_reset_connection_),
                      Pair("statement/sql/select", expected_select_),
                      Pair("statement/sql/set_option", expected_set_option_)));
    } else if (expected_set_option_ > 0) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Change user", expected_change_user_),
                      Pair("statement/sql/select", expected_select_),
                      Pair("statement/sql/set_option", expected_set_option_)));
    } else {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Change user", expected_change_user_),
                      Pair("statement/sql/select", expected_select_)));
    }

    expected_select_ += 1;
  }

  // and change the user again.
  //
  // With caching_sha2_password this should be against the cached hand-shake.
  {
    auto cmd_res =
        cli_->change_user(account.username, account.password, schema);
    ASSERT_NO_ERROR(cmd_res);

    expected_change_user_ += 1;
    if (can_share) {
      expected_set_option_ += 1;  // SET session-track-system-vars
      if (can_fetch_password) {
        expected_select_ += 1;  // SELECT collation
      }
    }

    if (can_share && expect_success) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }
  }

  {
    auto events_res = changed_event_counters(*cli_);
    ASSERT_NO_ERROR(events_res);

    if (can_share && can_fetch_password) {
      // expected_reset_connection_ += 1;
      // expected_set_option_ += 1;
    }

    if (expected_reset_connection_ > 0) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Change user", expected_change_user_),
                      Pair("statement/com/Reset Connection",
                           expected_reset_connection_),
                      Pair("statement/sql/select", expected_select_),
                      Pair("statement/sql/set_option", expected_set_option_)));
    } else if (expected_set_option_ > 0) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Change user", expected_change_user_),
                      Pair("statement/sql/select", expected_select_),
                      Pair("statement/sql/set_option", expected_set_option_)));
    } else {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Change user", expected_change_user_),
                      Pair("statement/sql/select", expected_select_)));
    }

    expected_select_ += 1;
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, ChangeUserTest,
    ::testing::Combine(::testing::Bool(), ::testing::ValuesIn(is_tcp_values),
                       ::testing::ValuesIn(share_connection_params),
                       ::testing::ValuesIn(change_user_params),
                       ::testing::Values("", "testing")),
    [](auto &info) {
      auto schema = std::get<4>(info.param);
      return "with" + std::string(std::get<0>(info.param) ? "" : "out") +
             "_ssl_via_" + std::get<2>(info.param).testname + "_over" +
             (std::get<1>(info.param) ? "_tcp_" : "_socket_") +
             std::get<3>(info.param).scenario +
             (schema.empty() ? "_without_schema"s : ("_with_schema_" + schema));
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
