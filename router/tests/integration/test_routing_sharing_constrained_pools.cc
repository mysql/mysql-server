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

#include <gmock/gmock-matchers.h>
#include <gmock/gmock-more-matchers.h>
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#include "my_rapidjson_size_t.h"

#include <rapidjson/pointer.h>

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
#include "mysqlrouter/utils.h"
#include "openssl_version.h"  // ROUTER_OPENSSL_VERSION
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
using ::testing::SizeIs;

static constexpr const auto kIdleServerConnectionsSleepTime{10ms};

static constexpr const std::string_view kDisabled{"DISABLED"};
static constexpr const std::string_view kRequired{"REQUIRED"};
static constexpr const std::string_view kPreferred{"PREFERRED"};
static constexpr const std::string_view kPassthrough{"PASSTHROUGH"};
static constexpr const std::string_view kAsClient{"AS_CLIENT"};

integration_tests::Procs procs;

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

class SharedRouter {
 public:
  SharedRouter(TcpPortPool &port_pool, uint64_t pool_size, bool split_routes)
      : port_pool_(port_pool),
        pool_size_{pool_size},
        rest_port_{port_pool_.get_next_available()},
        split_routes_{split_routes} {}

  integration_tests::Procs &process_manager() { return procs_; }

  template <size_t N>
  static std::vector<std::string> destinations_from_shared_servers(
      const std::array<SharedServer *, N> &servers) {
    std::vector<std::string> dests;
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

    std::vector<std::vector<std::string>> split_dests;

    if (split_routes_) {
      auto dest_it = destinations.begin();
      std::advance(dest_it, 1);
      split_dests.emplace_back(destinations.begin(), dest_it);  // 0
      split_dests.emplace_back(dest_it, destinations.end());    // 1 - ...
    } else {
      split_dests.push_back(destinations);
    }

    for (const auto &param : share_connection_params) {
      for (auto [route_ndx, dests] : stdx::views::enumerate(split_dests)) {
        auto port_key = std::make_tuple(param.client_ssl_mode,
                                        param.server_ssl_mode, route_ndx);
        auto ports_it = ports_.find(port_key);

        const auto port =
            ports_it == ports_.end()
                ? (ports_[port_key] = port_pool_.get_next_available())
                : ports_it->second;

        writer.section(
            "routing:classic_" + param.testname +
                (route_ndx == 0 ? "" : "_" + std::to_string(route_ndx)),
            {
                {"bind_port", std::to_string(port)},
                {"destinations", mysql_harness::join(dests, ",")},
                {"protocol", "classic"},
                {"routing_strategy", "round-robin"},

                {"client_ssl_mode", std::string(param.client_ssl_mode)},
                {"server_ssl_mode", std::string(param.server_ssl_mode)},

                {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
                {"client_ssl_cert",
                 SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
                {"connection_sharing", "1"},
                {"connection_sharing_delay", "0"},  //
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
                              size_t route_ndx = 0) const {
    return ports_.at(std::make_tuple(param.client_ssl_mode,
                                     param.server_ssl_mode, route_ndx));
  }

  [[nodiscard]] auto rest_port() const { return rest_port_; }
  [[nodiscard]] auto rest_user() const { return rest_user_; }
  [[nodiscard]] auto rest_pass() const { return rest_pass_; }

  void populate_connection_pool(const ShareConnectionParam &param) {
    // assuming round-robin: add one connection per destination of the route
    using pool_size_type = decltype(pool_size_);
    const pool_size_type num_destinations{3};

    for (pool_size_type ndx{}; ndx < num_destinations; ++ndx) {
      MysqlClient cli;

      cli.username("root");
      cli.password("");

      ASSERT_NO_ERROR(cli.connect(host(), port(param)));
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

  stdx::expected<void, std::error_code> retry_for(std::chrono::seconds timeout,
                                                  std::invocable auto func) {
    using clock_type = std::chrono::steady_clock;

    const auto end_time = clock_type::now() + timeout;
    do {
      auto res = func();
      if (!res) return stdx::unexpected(res.error());

      if (res.value()) return {};

      if (clock_type::now() > end_time) {
        return stdx::unexpected(make_error_code(std::errc::timed_out));
      }

      std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
    } while (true);
  }

  stdx::expected<void, std::error_code> wait_for_idle_server_connections(
      int expected_value, std::chrono::seconds timeout) {
    return retry_for(
        timeout,
        [this, expected_value]() -> stdx::expected<bool, std::error_code> {
          auto int_res = idle_server_connections();
          if (!int_res) return stdx::unexpected(int_res.error());

          return *int_res == expected_value;
        });
  }

  stdx::expected<void, std::error_code> wait_for_stashed_server_connections(
      int expected_value, std::chrono::seconds timeout) {
    int last_val{};
    auto res =
        retry_for(timeout,
                  [this, &last_val,
                   expected_value]() -> stdx::expected<bool, std::error_code> {
                    auto val_res = stashed_server_connections();
                    if (!val_res) return stdx::unexpected(val_res.error());

                    last_val = *val_res;

                    return *val_res == expected_value;
                  });

    if (!res) {
      EXPECT_EQ(expected_value, last_val);
    }

    return res;
  }

 private:
  integration_tests::Procs procs_;
  TcpPortPool &port_pool_;

  TempDirectory conf_dir_;

  static const constexpr char router_host_[] = "127.0.0.1";
  std::map<std::tuple<std::string_view, std::string_view, size_t>, uint16_t>
      ports_;

  uint64_t pool_size_;

  uint16_t rest_port_;

  IOContext rest_io_ctx_;
  RestClient rest_client_{rest_io_ctx_, "127.0.0.1", rest_port_, rest_user_,
                          rest_pass_};

  static constexpr const char rest_user_[] = "user";
  static constexpr const char rest_pass_[] = "pass";

  bool split_routes_;
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

      auto connect_res = cli->connect(s->server_host(), s->server_port());
      ASSERT_NO_ERROR(connect_res);

      ASSERT_NO_ERROR(
          SharedServer::local_install_plugin(*cli, "clone", "mysql_clone"));

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
                             uint64_t pool_size, bool split_routes) {
    for (const auto &s : servers) {
      if (s->mysqld_failed_to_start()) GTEST_SKIP();
    }

    if (shared_router_ == nullptr) {
      shared_router_ = new SharedRouter(port_pool, pool_size, split_routes);

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

/*
 * check if router behaves correctly if the server fails after a connection was
 * pooled.
 *
 * As killing (and restarting) servers is slow, an intermediate router is added
 * can be killed instead.
 *
 * C -> R -> I -> S
 *
 * C: client
 * R: router (under test)
 * I: router (intermediate)
 * S: server
 */
class ShareConnectionTestWithRestartedServer
    : public RouterComponentTest,
      public ::testing::WithParamInterface<ShareConnectionParam> {
 public:
  static constexpr const size_t kNumServers = 3;

  static void SetUpTestSuite() {
    // start servers.

    // start one intermediate router server.
    std::vector<std::string> router_dests;
    for (auto &inter : intermediate_routers_) {
      inter = std::make_unique<SharedRestartableRouter>(test_env->port_pool());

      router_dests.push_back(inter->host() + ":"s +
                             std::to_string(inter->port()));
    }

    shared_router_ =
        std::make_unique<SharedRouter>(test_env->port_pool(), 128, false);
  }

  static void TearDownTestSuite() {
    // stop and destroy all routers.
    shared_router_.reset();

    for (auto &inter : intermediate_routers_) {
      inter.reset();
    }
  }

  static std::array<SharedServer *, kNumServers> shared_servers() {
    auto s = test_env->servers();

    return {s[0], s[1], s[2]};
  }

  SharedRouter *shared_router() { return shared_router_.get(); }

  static std::array<SharedRestartableRouter *, kNumServers>
  intermediate_routers() {
    return {intermediate_routers_[0].get(), intermediate_routers_[1].get(),
            intermediate_routers_[2].get()};
  }

  void SetUp() override {
    if (!test_env->run_slow_tests() && GetParam().redundant_combination()) {
      GTEST_SKIP()
          << "skipped as RUN_SLOW_TESTS environment-variable is not set";
    }
    // start one intermediate ROUTER SERVER.
    std::vector<std::string> router_dests;
    for (auto &inter : intermediate_routers_) {
      router_dests.push_back(inter->host() + ":"s +
                             std::to_string(inter->port()));
    }

    shared_router_->spawn_router(router_dests);

    auto s = shared_servers();

    for (auto [ndx, inter] : stdx::views::enumerate(intermediate_routers_)) {
      if (!inter->is_running()) {
        auto &server = s[ndx];

        if (server->mysqld_failed_to_start()) GTEST_SKIP();

        this->start_intermediate_router_for_server(inter.get(), server);
      }
    }
  }

  void TearDown() override {
    for (auto &inter : intermediate_routers_) {
      if (!inter->is_running()) {
        if (::testing::Test::HasFatalFailure()) {
          inter->process_manager().dump_logs();
        }

        inter->process_manager().clear();
      }
    }

    shared_router_->process_manager().clear();
  }

  static void wait_stopped_intermediate_router(SharedRestartableRouter *inter) {
    ASSERT_NO_ERROR(inter->process_manager().wait_for_exit());

    inter->process_manager().clear();
  }

  static void stop_intermediate_router(SharedRestartableRouter *inter,
                                       bool wait_for_stopped = true) {
    inter->shutdown();

    if (wait_for_stopped) wait_stopped_intermediate_router(inter);
  }

  static void start_intermediate_router_for_server(
      SharedRestartableRouter *inter, SharedServer *s) {
    inter->spawn_router(
        {s->server_host() + ":"s + std::to_string(s->server_port())});
  }

  static void restart_intermediate_router(SharedRestartableRouter *inter,
                                          SharedServer *s) {
    stop_intermediate_router(inter);

    // and restart it again.
    start_intermediate_router_for_server(inter, s);
  }

  void wait_for_connections_to_server_expired(uint16_t srv_port) {
    // instead of purely waiting for the expiry, the intermediate router is
    // restarted which drops connections.
    for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
      if (s->server_port() == srv_port) {
        auto inter = intermediate_routers()[ndx];

        // stop the intermediate router to force a close of all connections
        // tested router had open.
        this->restart_intermediate_router(inter, s);
      }
    }

    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));
  }

 private:
  static std::array<std::unique_ptr<SharedRestartableRouter>, kNumServers>
      intermediate_routers_;
  static std::unique_ptr<SharedRouter> shared_router_;
};

std::unique_ptr<SharedRouter>
    ShareConnectionTestWithRestartedServer::shared_router_;
std::array<std::unique_ptr<SharedRestartableRouter>, 3>
    ShareConnectionTestWithRestartedServer::intermediate_routers_;

template <size_t S, size_t P, bool split_routes = false>
class ShareConnectionTestTemp
    : public RouterComponentTest,
      public ::testing::WithParamInterface<ShareConnectionParam> {
 public:
  static_assert(S > 0);
  static_assert(P >= 0);

  static constexpr const size_t kNumServers = S;
  static constexpr const size_t kMaxPoolSize = P;

  static void SetUpTestSuite() {
    for (const auto &s : shared_servers()) {
      if (s->mysqld_failed_to_start()) GTEST_SKIP();
    }

    TestWithSharedRouter::SetUpTestSuite(
        test_env->port_pool(), shared_servers(), kMaxPoolSize, split_routes);
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

  SharedRouter *shared_router() { return TestWithSharedRouter::router(); }

  void SetUp() override {
    for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
      // shared_server_ may be null if TestWithSharedServer::SetUpTestSuite
      // threw?
      if (s == nullptr || s->mysqld_failed_to_start()) {
        GTEST_SKIP() << "failed to start mysqld";
      } else {
        auto cli = admin_clis()[ndx];

        ASSERT_NO_ERROR(SharedServer::close_all_connections(*cli));
        SharedServer::reset_to_defaults(*cli);
      }
    }
  }

  ~ShareConnectionTestTemp() override {
    if (::testing::Test::HasFailure()) {
      shared_router()->process_manager().dump_logs();
    }
  }

 protected:
  const std::string valid_ssl_key_{SSL_TEST_DATA_DIR "/server-key-sha512.pem"};
  const std::string valid_ssl_cert_{SSL_TEST_DATA_DIR
                                    "/server-cert-sha512.pem"};
};

class Checker {
 public:
  virtual ~Checker() = default;

  virtual void apply_before_connect(MysqlClient & /* cli */) {}

  virtual void apply(MysqlClient &cli) { ASSERT_NO_ERROR(cli.ping()); }

  virtual std::function<void(MysqlClient &cli)> verifier() = 0;

  virtual void advance() {}
};

class EmptyResultChecker : public Checker {
 public:
  using test_values_type = std::vector<std::string>;

  EmptyResultChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {
    if (test_values_.empty()) {
      throw std::invalid_argument("test_values size must be != 0");
    }
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    auto stmt = test_values_[ndx_];

    return [stmt](MysqlClient &cli) {
      SCOPED_TRACE("// " + stmt);

      auto cmd_res = query_one_result(cli, stmt);
      ASSERT_NO_ERROR(cmd_res);

      EXPECT_THAT(*cmd_res, IsEmpty());
    };
  }

 private:
  size_t ndx_{};

  test_values_type test_values_;
};

class WarningResultChecker : public Checker {
 public:
  using test_values_type = std::vector<
      std::pair<std::string, std::vector<std::vector<std::string>>>>;

  WarningResultChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {
    if (test_values_.empty()) {
      throw std::invalid_argument("test_values size must be != 0");
    }
  }

  void apply(MysqlClient &cli) override {
    auto stmt = test_values_[ndx_].first;

    ASSERT_NO_ERROR(cli.query(stmt));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    auto expected_result = test_values_[ndx_].second;

    return [expected_result](MysqlClient &cli) {
      SCOPED_TRACE("// SHOW WARNINGS");
      {
        auto cmd_res = query_one_result(cli, "SHOW WARNINGS");
        ASSERT_NO_ERROR(cmd_res);

        EXPECT_EQ(*cmd_res, expected_result);
      }

      SCOPED_TRACE("// SHOW COUNT(*) WARNINGS");
      {
        auto cmd_res = query_one_result(cli, "SHOW COUNT(*) WARNINGS");
        ASSERT_NO_ERROR(cmd_res);

        EXPECT_EQ(*cmd_res, std::vector<std::vector<std::string>>{
                                {std::to_string(expected_result.size())}});
      }
    };
  }

 private:
  size_t ndx_{};

  test_values_type test_values_;
};

class ErrorResultChecker : public Checker {
 public:
  using test_values_type = std::vector<
      std::pair<std::string, std::vector<std::vector<std::string>>>>;

  ErrorResultChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {
    if (test_values_.empty()) {
      throw std::invalid_argument("test_values size must be != 0");
    }
  }

  void apply(MysqlClient &cli) override {
    auto stmt = test_values_[ndx_].first;

    ASSERT_ERROR(cli.query(stmt));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    auto expected_result = test_values_[ndx_].second;

    return [expected_result](MysqlClient &cli) {
      SCOPED_TRACE("// SHOW COUNT(*) ERRORS");
      {
        auto cmd_res = query_one_result(cli, "SHOW COUNT(*) ERRORS");
        ASSERT_NO_ERROR(cmd_res);

        EXPECT_EQ(*cmd_res, std::vector<std::vector<std::string>>{
                                {std::to_string(expected_result.size())}});
      }

      SCOPED_TRACE("// SHOW ERRORS");
      {
        auto cmd_res = query_one_result(cli, "SHOW ERRORS");
        ASSERT_NO_ERROR(cmd_res);

        EXPECT_EQ(*cmd_res, expected_result);
      }

      SCOPED_TRACE("// SHOW ERRORS LIMIT 0");
      {
        auto cmd_res = query_one_result(cli, "SHOW ERRORS LIMIT 0");
        ASSERT_NO_ERROR(cmd_res);

        EXPECT_EQ(*cmd_res, std::vector<std::vector<std::string>>{});
      }

      SCOPED_TRACE("// SHOW ERRORS LIMIT 0, 1");
      {
        auto cmd_res = query_one_result(cli, "SHOW ERRORS LIMIT 0, 1");
        ASSERT_NO_ERROR(cmd_res);

        if (expected_result.empty()) {
          EXPECT_EQ(*cmd_res, std::vector<std::vector<std::string>>{});
        } else {
          EXPECT_EQ(*cmd_res, std::vector<std::vector<std::string>>{
                                  expected_result.front()});
        }
      }
    };
  }

 private:
  size_t ndx_{};

  test_values_type test_values_;
};

class SelectWarningCountChecker : public Checker {
 public:
  using test_values_type = std::vector<
      std::pair<std::string, std::vector<std::vector<std::string>>>>;

  SelectWarningCountChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {
    if (test_values_.empty()) {
      throw std::invalid_argument("test_values size must be != 0");
    }
  }

  void apply(MysqlClient &cli) override {
    auto stmt = test_values_[ndx_].first;

    (void)cli.query(stmt);
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    auto expected_result = test_values_[ndx_].second;

    return [expected_result](MysqlClient &cli) {
      SCOPED_TRACE("// SHOW COUNT(*) WARNINGS");
      {
        auto cmd_res = query_one_result(cli, "SHOW COUNT(*) WARNINGS");
        ASSERT_NO_ERROR(cmd_res);

        EXPECT_EQ(*cmd_res, expected_result);
      }

      SCOPED_TRACE("// select @@warning_count");
      {
        auto cmd_res = query_one_result(cli, "select @@warning_count");
        ASSERT_NO_ERROR(cmd_res);

        EXPECT_EQ(*cmd_res, expected_result);
      }
    };
  }

 private:
  size_t ndx_{};

  test_values_type test_values_;
};

class SelectErrorCountChecker : public Checker {
 public:
  using test_values_type = std::vector<
      std::pair<std::string, std::vector<std::vector<std::string>>>>;

  SelectErrorCountChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {
    if (test_values_.empty()) {
      throw std::invalid_argument("test_values size must be != 0");
    }
  }

  void apply(MysqlClient &cli) override {
    auto stmt = test_values_[ndx_].first;

    (void)cli.query(stmt);
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    auto expected_result = test_values_[ndx_].second;

    return [expected_result](MysqlClient &cli) {
      SCOPED_TRACE("// SHOW COUNT(*) ERRORS");
      {
        auto cmd_res = query_one_result(cli, "SHOW COUNT(*) ERRORS");
        ASSERT_NO_ERROR(cmd_res);

        EXPECT_EQ(*cmd_res, expected_result);
      }

      SCOPED_TRACE("// select @@error_count");
      {
        auto cmd_res = query_one_result(cli, "select @@error_count");
        ASSERT_NO_ERROR(cmd_res);

        EXPECT_EQ(*cmd_res, expected_result);
      }
    };
  }

 private:
  size_t ndx_{};

  test_values_type test_values_;
};

using ShareConnectionTinyPoolOneServerTest = ShareConnectionTestTemp<1, 1>;
using ShareConnectionTinyPoolTwoServersTest = ShareConnectionTestTemp<2, 1>;
using ShareConnectionSmallPoolTwoServersTest = ShareConnectionTestTemp<2, 2>;
using ShareConnectionSmallPoolFourServersTest = ShareConnectionTestTemp<4, 2>;
using ShareConnectionNoPoolOneServersTest = ShareConnectionTestTemp<1, 0>;

/*
 * 1. cli1 connects and starts long running a query
 *    - connect
 *    - query (started)
 * 2. cli2 connects and starts short running query
 *    - connect
 *    - query
 *    - stashed.
 * 3. cli1 query finishes
 *    - stashed.
 * 4. cli3 connects and starts long running query
 *    - reuse from pool
 *    - start query
 * 5. cli2 runs a query
 *    - connect's a new connection
 *    - runs query
 */
TEST_P(ShareConnectionTinyPoolOneServerTest, overlapping_connections) {
  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  MysqlClient cli1;
  MysqlClient cli2;
  MysqlClient cli3;

  {
    auto account = SharedServer::caching_sha2_password_account();

    cli1.username(account.username);
    cli1.password(account.password);

    auto connect_res = cli1.connect(shared_router()->host(),
                                    shared_router()->port(GetParam()));

    if (!can_fetch_password) {
      ASSERT_ERROR(connect_res);
      // Authentication plugin 'caching_sha2_password' reported error:
      // Authentication requires secure connection.
      EXPECT_EQ(connect_res.error().value(), 2061);

      return;
    }
    ASSERT_NO_ERROR(connect_res);

    // wait until the connection is in the pool.
    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }
  }

  std::string cli1_connection_id{};
  {
    auto cmd_res = query_one_result(cli1, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    auto result = std::move(*cmd_res);
    ASSERT_THAT(result, SizeIs(1));     // 1 row
    ASSERT_THAT(result[0], SizeIs(1));  // first row, has 1 field.

    cli1_connection_id = result[0][0];

    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }
  }

  // - set-option
  // - add to stash
  // - do

  ASSERT_NO_ERROR(cli1.send_query("DO SLEEP(0.2)"));

  ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(0, 10s));

  {
    auto account = SharedServer::caching_sha2_password_account();

    cli2.username(account.username);
    cli2.password(account.password);

    ASSERT_NO_ERROR(cli2.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));

    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }
  }
  // - connect
  // - set-option
  // - add to stash

  std::string cli2_connection_id{};
  {
    auto cmd_res = query_one_result(cli2, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    auto result = std::move(*cmd_res);
    ASSERT_THAT(result, SizeIs(1));     // 1 row
    ASSERT_THAT(result[0], SizeIs(1));  // first row, has 1 field.

    cli2_connection_id = result[0][0];

    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }
  }

  // - do
  SCOPED_TRACE("// wait until 1st connection finished SLEEP()ing.");
  ASSERT_NO_ERROR(cli1.read_query_result());

  // cli2 should be stashed
  // cli1 should be stashed

  {
    auto events_res = changed_event_counters(cli1);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        // cli1: set-option
        // cli1: do (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/do", 1),
                                Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 1)));
      } else {
        // cli1: set-option
        // cli1: do (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/do", 1),
                                Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)));
      }
    } else {
      // no sharing possible, router is not injection SET statements.
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/do", 1),
                                           Pair("statement/sql/select", 1)));
    }
  }

  {
    auto events_res = changed_event_counters(cli2);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        // cli1: set-option
        // cli4: change-user + set-option
        // cli1: change-user + set-option (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 1)));
      } else {
        // cli1: set-option
        // cli1: (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)));
      }
    } else {
      // no sharing possible, router is not injection SET statements.
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/select", 1)));
    }
  }

  if (can_share && can_fetch_password) {
    ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(
        2, 10s));  // cli1, cli2

    // close the server-side of cli1.

    auto admin_cli_res = shared_servers()[0]->admin_cli();
    ASSERT_NO_ERROR(admin_cli_res);

    auto kill_res = admin_cli_res->query("KILL " + cli1_connection_id);
    ASSERT_NO_ERROR(kill_res);

    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));  // cli2
  }

  // cli3 takes connection from the stash from cli2
  {
    auto account = SharedServer::caching_sha2_empty_password_account();

    cli3.username(account.username);
    cli3.password(account.password);

    ASSERT_NO_ERROR(cli3.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));

    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }
  }

  // start a long running query, takes the connection from the stash.
  ASSERT_NO_ERROR(cli3.send_query("SELECT SLEEP(0.2), CONNECTION_ID()"));

  ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(0, 10s));

  // opens a new connection as cli3 grapped the pooled connection.
  {
    auto cmd_res = query_one_result(cli2, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    if (can_share) {
      if (can_fetch_password) {
        EXPECT_THAT(
            *cmd_res,
            ElementsAre(ElementsAre(::testing::Ne(cli2_connection_id))));

        ASSERT_NO_ERROR(
            shared_router()->wait_for_stashed_server_connections(1, 10s));
      }
    }
  }

  {
    auto cmd_res = cli3.read_query_result();
    ASSERT_NO_ERROR(cmd_res);

    auto results = result_as_vector(*cmd_res);
    ASSERT_THAT(results, ::testing::SizeIs(1));

    auto result = results.front();
    if (can_share) {
      if (can_fetch_password) {
        EXPECT_THAT(result,
                    ElementsAre(ElementsAre(::testing::_, cli2_connection_id)));
      }
    }
  }
}

TEST_P(ShareConnectionTinyPoolOneServerTest,
       classic_protocol_set_option_reconnect) {
  RecordProperty(
      "Description",
      "check if the multi-statement flag is recovered after a reconnect");

  SCOPED_TRACE("// ensure the pool is empty");
  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));

  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  if (can_share) {
    // connection is in the pool
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  SCOPED_TRACE("// enable multi-statement support on 1st connection.");
  ASSERT_NO_ERROR(cli.set_server_option(MYSQL_OPTION_MULTI_STATEMENTS_ON));

  if (can_share) {
    // connection is in the pool
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  SCOPED_TRACE("// verify multi-statement works");
  {
    auto query_res = cli.query("DO /* client[0] = PASS */ 1; DO 2");
    if (can_share) {
      ASSERT_ERROR(query_res);

      // Multi-Statements are forbidden if connection-sharing is enabled.
      EXPECT_EQ(query_res.error().value(), 4501);
    } else {
      ASSERT_NO_ERROR(query_res);

      for (const auto &res [[maybe_unused]] : *query_res) {
      }
    }
  }

  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  SCOPED_TRACE(
      "// open a 2nd connection which should reuse the 1st connection.");
  MysqlClient cli_blocker;
  cli_blocker.username(account.username);
  cli_blocker.password(account.password);

  ASSERT_NO_ERROR(cli_blocker.connect(shared_router()->host(),
                                      shared_router()->port(GetParam())));

  {
    auto query_res = cli_blocker.query("DO /* client[1] = FAIL */ 1; DO 2");
    ASSERT_ERROR(query_res);
  }

  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  SCOPED_TRACE("// the 1st connection still is multi-statement");
  {
    auto query_res = cli.query("DO /* client[0] = PASS */ 1; DO 2");
    if (can_share) {
      ASSERT_ERROR(query_res);

      // Multi-Statements are forbidden if connection-sharing is enabled.
      EXPECT_EQ(query_res.error().value(), 4501);
    } else {
      ASSERT_NO_ERROR(query_res);

      for (const auto &res [[maybe_unused]] : *query_res) {
      }
    }
  }
}

TEST_P(ShareConnectionTinyPoolOneServerTest,
       overlapping_connections_different_accounts) {
  for (auto &s : shared_servers()) {
    s->flush_privileges();  // reset the auth-cache
  }

  MysqlClient cli1;
  MysqlClient cli2;
  MysqlClient cli3;

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  {
    auto account = SharedServer::caching_sha2_password_account();

    cli1.username(account.username);
    cli1.password(account.password);

    auto connect_res = cli1.connect(shared_router()->host(),
                                    shared_router()->port(GetParam()));

    if (!can_fetch_password) {
      ASSERT_ERROR(connect_res);

      // Authentication plugin 'caching_sha2_password' reported error:
      // Authentication requires secure connection.
      EXPECT_EQ(connect_res.error().value(), 2061);

      return;
    }

    ASSERT_NO_ERROR(connect_res);

    // wait until the connection is in the pool.
    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }
  }

  std::string cli1_connection_id{};
  {
    auto cmd_res = query_one_result(cli1, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    auto result = std::move(*cmd_res);
    ASSERT_THAT(result, SizeIs(1));     // 1 row
    ASSERT_THAT(result[0], SizeIs(1));  // first row, has 1 field.

    cli1_connection_id = result[0][0];

    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }
  }

  // - set-option
  // - add to stash

  SCOPED_TRACE("// block the 1st connection for a bit.");
  ASSERT_NO_ERROR(cli1.send_query("DO SLEEP(0.2)"));

  // - do
  ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(0, 10s));

  SCOPED_TRACE("// open a 2nd connection, that gets added to the pool.");
  {
    auto account = SharedServer::caching_sha2_password_account();

    cli2.set_option(MysqlClient::GetServerPublicKey(true));
    cli2.username(account.username);
    cli2.password(account.password);

    auto connect_res = cli2.connect(shared_router()->host(),
                                    shared_router()->port(GetParam()));
    if (GetParam().client_ssl_mode == kDisabled &&
        (GetParam().server_ssl_mode == kRequired ||
         GetParam().server_ssl_mode == kPreferred)) {
      // with client-ssl-mode DISABLED, router doesn't have a public-key or a
      // tls connection to the client.
      //
      // The client will ask for the server's public-key instead which the
      // server will treat as "password" and then fail to authenticate.
      ASSERT_ERROR(connect_res);
      GTEST_SKIP() << connect_res.error();
    }
    ASSERT_NO_ERROR(connect_res);

    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }
  }

  // - connect
  // - set-option
  // - add to stash

  SCOPED_TRACE("// check connection id of 2nd connection.");
  std::string cli2_connection_id{};
  {
    auto cmd_res = query_one_result(cli2, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    auto result = std::move(*cmd_res);
    ASSERT_THAT(result, SizeIs(1));
    ASSERT_THAT(result[0], SizeIs(1));
    cli2_connection_id = result[0][0];
  }

  if (can_share && can_fetch_password) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  // - reset-connection
  // - set-option
  // - select

  SCOPED_TRACE("// wait until 1st connection finished SLEEP()ing.");
  ASSERT_NO_ERROR(cli1.read_query_result());

  // cli2 should be stashed
  // cli1 should be stashed
  {
    auto events_res = changed_event_counters(cli1);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        // cli1: set-option
        // cli1: do (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/do", 1),
                                Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 1)));
      } else {
        // cli1: set-option
        // cli1: do (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/do", 1),
                                Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)));
      }
    } else {
      // no sharing possible, router is not injection SET statements.
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/do", 1),
                                           Pair("statement/sql/select", 1)));
    }
  }

  {
    auto events_res = changed_event_counters(cli2);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        // cli1: set-option
        // cli4: change-user + set-option
        // cli1: change-user + set-option (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 1)));
      } else {
        // cli1: set-option
        // cli1: (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)));
      }
    } else {
      // no sharing possible, router is not injection SET statements.
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/select", 1)));
    }
  }

  if (can_share && can_fetch_password) {
    ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(
        2, 10s));  // cli1, cli2

    // close the server-side of cli1.

    auto admin_cli_res = shared_servers()[0]->admin_cli();
    ASSERT_NO_ERROR(admin_cli_res);

    auto kill_res = admin_cli_res->query("KILL " + cli1_connection_id);
    ASSERT_NO_ERROR(kill_res);

    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));  // cli2
  }

  // cli3 takes connection from the stash.
  {
    auto account = SharedServer::caching_sha2_password_account();

    cli3.username(account.username);
    cli3.password(account.password);

    ASSERT_NO_ERROR(cli3.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));
  }

  if (can_share && can_fetch_password) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  SCOPED_TRACE(
      "// start a long running query, takes the connection from the pool.");
  ASSERT_NO_ERROR(cli3.send_query("SELECT SLEEP(0.2), CONNECTION_ID()"));

  ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(0, 10s));

  SCOPED_TRACE(
      "// opens a new connection as cli3 grapped the pooled connection.");
  {
    auto cmd_res = query_one_result(cli2, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    if (can_share) {
      if (can_fetch_password) {
        EXPECT_THAT(
            *cmd_res,
            ElementsAre(ElementsAre(::testing::Ne(cli2_connection_id))));

        ASSERT_NO_ERROR(
            shared_router()->wait_for_stashed_server_connections(1, 10s));
      }
    }
  }

  SCOPED_TRACE("// check that the 3rd connection was pooled.");
  {
    auto cmd_res = cli3.read_query_result();
    ASSERT_NO_ERROR(cmd_res);

    auto results = result_as_vector(*cmd_res);
    ASSERT_THAT(results, ::testing::SizeIs(1));

    auto result = results.front();
    if (can_share) {
      if (can_fetch_password) {
        EXPECT_THAT(result,
                    ElementsAre(ElementsAre(::testing::_, cli2_connection_id)));
      }
    }
  }
}

/*
 * test the cmd_kill -> Ok path
 *
 * using one-server to ensure both connections end up on the same backend.
 */
TEST_P(ShareConnectionTinyPoolOneServerTest,
       classic_protocol_set_password_reconnect) {
  const bool can_share = GetParam().can_share();
  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);

  SCOPED_TRACE("// connecting to server");

  auto &admin_cli = *(admin_clis()[0]);
  ASSERT_NO_ERROR(admin_cli.query("DROP USER IF EXISTS changeme"));
  ASSERT_NO_ERROR(
      admin_cli.query("CREATE USER changeme IDENTIFIED WITH "
                      "caching_sha2_password BY 'changeme'"));

  MysqlClient cli;

  cli.username("changeme");
  cli.password("changeme");

  auto connect_res =
      cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
  if (!can_fetch_password) {
    ASSERT_ERROR(connect_res);

    // Authentication plugin 'caching_sha2_password' reported error:
    // Authentication requires secure connection.
    EXPECT_EQ(connect_res.error().value(), 2061);

    return;
  }
  ASSERT_NO_ERROR(connect_res);

  auto conn_id_res = query_one<1>(cli, "SELECT CONNECTION_ID()");
  ASSERT_NO_ERROR(conn_id_res);

  auto conn_num_res = from_string((*conn_id_res)[0]);
  ASSERT_NO_ERROR(conn_num_res);

  ASSERT_NO_ERROR(admin_cli.query("KILL " + std::to_string(*conn_num_res)));

  ASSERT_NO_ERROR(admin_cli.query("SET PASSWORD FOR changeme='changeme2'"));

  // should fail as connection is killed.
  {
    auto query_res = cli.query("DO 1");
    ASSERT_ERROR(query_res);

    if (can_share && can_fetch_password) {
      // reauth failed.
      EXPECT_EQ(query_res.error().value(), 1045) << query_res.error();
    } else {
      EXPECT_EQ(query_res.error().value(), 2013) << query_res.error();
    }
  }

  {
    auto query_res = cli.query("DO 1");
    ASSERT_ERROR(query_res);

    // connection is closed.
    EXPECT_EQ(query_res.error().value(), 2013) << query_res.error();
  }
}

/*
 * run a binlog stream through the router.
 *
 * - register-replica
 * - binlog-dump
 *
 * expensive test.
 */
TEST_P(ShareConnectionTinyPoolOneServerTest,
       classic_protocol_register_replica_and_dump) {
  if (!(GetParam().client_ssl_mode == kRequired &&
        GetParam().server_ssl_mode == kRequired)) {
    GTEST_SKIP() << "skipped as RUN_SLOW_TESTS environment-variable is not set";
  }

  SCOPED_TRACE("// starting to replica server");
  SharedServer replica_server(test_env->port_pool());

  SCOPED_TRACE("// .. preparing datadir");
  replica_server.prepare_datadir();
  SCOPED_TRACE("// .. spawning replica");
  replica_server.spawn_server(
      {"--report-host=some_funky_host", "--server-id=2"});
  ASSERT_FALSE(replica_server.mysqld_failed_to_start());

  SCOPED_TRACE("// connecting to server");
  auto replica_res = replica_server.admin_cli();
  ASSERT_NO_ERROR(replica_res);

  auto replica = std::move(*replica_res);

  SCOPED_TRACE("// change the source of the replica");
  {
    auto cmd_res = replica.query(R"(CHANGE REPLICATION SOURCE TO
SOURCE_SSL = 1,
SOURCE_HOST = "127.0.0.1",
SOURCE_PORT = )" + std::to_string(shared_router()->port(GetParam())));
    ASSERT_NO_ERROR(cmd_res);
  }

  SCOPED_TRACE("// start the replica");
  {
    auto cmd_res = replica.query(R"(START REPLICA IO_THREAD
UNTIL SOURCE_LOG_FILE="binlog.000001", SOURCE_LOG_POS=100
USER = "root"
PASSWORD = ""
)");
    ASSERT_NO_ERROR(cmd_res);
  }

  SCOPED_TRACE("// wait for replica to register");

  using clock_type = std::chrono::steady_clock;
  for (auto cur = clock_type::now(), end = cur + 10s;;
       cur = clock_type::now()) {
    ASSERT_LT(cur, end) << "waited 10sec for replica to register.";

    auto cmd_res = query_one_result(replica, R"(SELECT
  r.service_state,
  t.name,
  t.processlist_command,
  t.processlist_state
 FROM performance_schema.replication_connection_status AS r
 JOIN performance_schema.threads AS t
WHERE t.thread_id = r.thread_id
)");
    ASSERT_NO_ERROR(cmd_res);

    auto result = *cmd_res;
    if (!result.empty()) {
      ASSERT_GT(result[0].size(), 3);

      if (result[0][3] == "Waiting for source to send event") {
        EXPECT_THAT(result, ElementsAre(ElementsAre(
                                "ON", "thread/sql/replica_io", "Connect",
                                "Waiting for source to send event")));
        break;
      }
    }

    std::this_thread::sleep_for(100ms);
  }

  auto &source = *(admin_clis()[0]);

  SCOPED_TRACE("// replica is registered.");
  // check that the replica registered to the source with the values provided
  // by '--report-host'.
  {
    auto cmd_res = query_one_result(source, "SHOW REPLICAS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res,
                ElementsAre(ElementsAre(
                    "2",                                           // replica-id
                    "some_funky_host",                             // host
                    std::to_string(replica_server.server_port()),  // port
                    "1",                                           // source-id
                    Not(IsEmpty())  // server-uuid
                    )));
  }

  SCOPED_TRACE("// stop the replica");

  ASSERT_NO_ERROR(replica.query("STOP REPLICA"));
  ASSERT_NO_ERROR(replica.query("RESET REPLICA"));
}

/*
 * run a clone stream through the router.
 *
 * - clone opens two connections and both have to end up on the same host.
 * - stop the clone after a while.
 *
 * expensive test.
 */
TEST_P(ShareConnectionTinyPoolOneServerTest, classic_protocol_clone) {
  if (!(GetParam().client_ssl_mode == kRequired &&
        GetParam().server_ssl_mode == kRequired)) {
    GTEST_SKIP() << "skipped as RUN_SLOW_TESTS environment-variable is not set";
  }

  SCOPED_TRACE("// starting clone recipient");
  SharedServer recipient_server(test_env->port_pool());

  recipient_server.prepare_datadir();
  recipient_server.spawn_server();
  ASSERT_FALSE(recipient_server.mysqld_failed_to_start());

  SCOPED_TRACE("// connection to the recipient server directly");
  auto recipient_res = recipient_server.admin_cli();
  ASSERT_NO_ERROR(recipient_res);

  auto recipient = std::move(*recipient_res);
  SharedServer::local_install_plugin(recipient, "clone", "mysql_clone");
  {
    std::ostringstream oss;

    oss << "SET GLOBAL clone_valid_donor_list = " <<  //
        std::quoted(shared_router()->host() + ":"s +
                        std::to_string(shared_router()->port(GetParam())),
                    '\'');

    ASSERT_NO_ERROR(recipient.query(oss.str()));
  }

  TempDirectory clone_data_dir("router-mysqld-clone");
  // the directory must not exit.
  mysql_harness::delete_dir_recursive(clone_data_dir.name());

  SCOPED_TRACE("// start to clone through the router.");
  {
    std::ostringstream oss;

    auto account = SharedServer::admin_account();

    oss << "CLONE INSTANCE FROM " <<  //
        std::quoted(account.username, '\'') << "@"
        << std::quoted(shared_router()->host(), '\'') << ":"
        << shared_router()->port(GetParam()) <<  //
        " IDENTIFIED BY " << std::quoted(account.password, '\'')
        << " DATA DIRECTORY = " << std::quoted(clone_data_dir.name(), '\'');

    ASSERT_NO_ERROR(recipient.send_query(oss.str()));
  }

  bool clone_killed = false;
  SCOPED_TRACE("// wait 2 seconds for the clone to finish");
  {
    auto monitor_res = recipient_server.admin_cli();
    ASSERT_NO_ERROR(monitor_res);

    auto monitor = std::move(*monitor_res);

    std::this_thread::sleep_for(100ms);

    using clock_type = std::chrono::steady_clock;
    for (auto cur = clock_type::now(), end = cur + 2s;;
         cur = clock_type::now()) {
      ASSERT_LT(cur, end);

      auto cmd_res = query_one_result(monitor, R"(SELECT
  pid, state, error_no, error_message
 FROM performance_schema.clone_status
)");
      ASSERT_NO_ERROR(cmd_res);

      auto result = *cmd_res;
      ASSERT_THAT(result, Not(IsEmpty()));
      ASSERT_GT(result[0].size(), 2);

      ASSERT_THAT(result[0][1],
                  AnyOf("Not Started", "In Progress", "Complete", "Failed"));

      if (result[0][1] == "Complete") {
        // clone was fast, good.
        break;
      }

      // wait until the clone is in progress ... then stop it.
      if (result[0][1] == "In Progress") {
        ASSERT_NO_ERROR(monitor.query("KILL QUERY " + result[0][0]));
        clone_killed = true;
        break;
      }

      // if it hasn't been started yet, wait a bit.
      ASSERT_EQ(result[0][1], "Not Started")
          << mysql_harness::join(result[0], ", ");

      std::this_thread::sleep_for(100ms);
    }
  }

  if (clone_killed) {
    auto cmd_res = recipient.read_query_result();
    ASSERT_ERROR(cmd_res);
    // 1317: query execution was interrupted.
    // 1158: Got an error reading communication packets
    EXPECT_THAT(cmd_res.error().value(), ::testing::AnyOf(1317, 1158))
        << cmd_res.error();
  }
}

class SchemaChecker : public Checker {
 public:
  using test_values_type = std::vector<std::string>;

  SchemaChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {
    if (test_values_.empty()) {
      throw std::invalid_argument("schemas size must be != 0");
    }
  }

  void apply(MysqlClient &cli) override {
    auto schema = test_values_[ndx_];

    ASSERT_NO_ERROR(cli.use_schema(schema));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    auto schema = test_values_[ndx_];

    return [schema](MysqlClient &cli) {
      SCOPED_TRACE("// SELECT SCHEMA()");

      auto cmd_res = query_one_result(cli, "SELECT SCHEMA()");
      ASSERT_NO_ERROR(cmd_res);

      EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(schema)));
    };
  }

  void advance() override {
    ++ndx_;

    if (ndx_ >= test_values_.size()) {
      ndx_ = 0;
    }
  }

 private:
  size_t ndx_{};
  test_values_type test_values_;
};

/*
 * check that the initial-schema is restored.
 */
class InitialSchemaChecker : public Checker {
 public:
  using test_values_type = std::vector<std::string>;

  InitialSchemaChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {
    if (test_values_.empty()) {
      throw std::invalid_argument("schemas size must be != 0");
    }
  }

  void apply_before_connect(MysqlClient &cli) override {
    auto schema = test_values_[ndx_];

    ASSERT_NO_ERROR(cli.use_schema(schema));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    auto schema = test_values_[ndx_];

    return [schema](MysqlClient &cli) {
      SCOPED_TRACE("// SELECT SCHEMA()");

      auto cmd_res = query_one_result(cli, "SELECT SCHEMA()");
      ASSERT_NO_ERROR(cmd_res);

      EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(schema)));
    };
  }

  void advance() override {
    ++ndx_;

    if (ndx_ >= test_values_.size()) {
      ndx_ = 0;
    }
  }

 private:
  size_t ndx_{};
  test_values_type test_values_;
};

class SetSessionVarChecker : public Checker {
 public:
  using test_values_type = std::vector<std::pair<std::string, std::string>>;

  SetSessionVarChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {}

  void apply(MysqlClient &cli) override {
    auto [key, value] = test_values_[ndx_];

    ASSERT_NO_ERROR(cli.query("SET SESSION " + key + " = " + value));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    auto [key, value] = test_values_[ndx_];

    return [key = key, value = value](MysqlClient &cli) {
      auto cmd_res = query_one_result(cli, "SELECT @@SESSION." + key);
      ASSERT_NO_ERROR(cmd_res);

      EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(value)));
    };
  }

  void advance() override {
    ++ndx_;

    if (ndx_ >= test_values_.size()) {
      ndx_ = 0;
    }
  }

 private:
  size_t ndx_{};
  test_values_type test_values_;
};

class WarningsChecker : public Checker {
 public:
  using test_values_type = std::vector<std::pair<std::string, int>>;

  WarningsChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {}

  void apply(MysqlClient &cli) override {
    auto stmt = test_values_[ndx_].first;

    // send a statement with generates a warning or error.
    ASSERT_NO_ERROR(cli.query(stmt));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    auto error_code = test_values_[ndx_].second;

    return [error_code = error_code](MysqlClient &cli) {
      auto cmd_res = query_one_result(cli, "SHOW WARNINGS");
      ASSERT_NO_ERROR(cmd_res);

      EXPECT_THAT(*cmd_res,
                  ElementsAre(ElementsAre(
                      ::testing::_, std::to_string(error_code), ::testing::_)));
    };
  }

  void advance() override {
    ++ndx_;

    if (ndx_ >= test_values_.size()) {
      ndx_ = 0;
    }
  }

 private:
  size_t ndx_{};
  test_values_type test_values_;
};

class NoWarningsChecker : public Checker {
 public:
  using test_values_type = std::vector<std::string>;

  NoWarningsChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {}

  void apply(MysqlClient &cli) override {
    auto stmt = test_values_[ndx_];
    ASSERT_NO_ERROR(cli.query(stmt));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    return [](MysqlClient &cli) {
      auto cmd_res = query_one_result(cli, "SHOW WARNINGS");
      ASSERT_NO_ERROR(cmd_res);

      EXPECT_THAT(*cmd_res, ::testing::IsEmpty());
    };
  }

  void advance() override {
    ++ndx_;

    if (ndx_ >= test_values_.size()) {
      ndx_ = 0;
    }
  }

 private:
  size_t ndx_{};
  test_values_type test_values_;
};

class SessionAttributeChecker : public Checker {
 public:
  using test_values_type = std::vector<std::pair<std::string, std::string>>;

  SessionAttributeChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {}

  void apply_before_connect(MysqlClient &cli) override {
    auto test_value = test_values_[ndx_];

    ASSERT_NO_ERROR(cli.set_option(MysqlClient::ConnectAttributeAdd(
        test_value.first.c_str(), test_value.second.c_str())));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    auto [key, value] = test_values_[ndx_];

    return [key = key, value = value](MysqlClient &cli) {
      auto cmd_res = query_one_result(cli, R"(
SELECT ATTR_NAME, ATTR_VALUE
  FROM performance_schema.session_account_connect_attrs
 WHERE PROCESSLIST_ID = CONNECTION_ID()
   AND LEFT(ATTR_NAME, 1) != '_'
 ORDER BY ATTR_NAME)");
      ASSERT_NO_ERROR(cmd_res);

      EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(key, value)));
    };
  }

  void advance() override {
    ++ndx_;

    if (ndx_ >= test_values_.size()) {
      ndx_ = 0;
    }
  }

 private:
  size_t ndx_{};
  test_values_type test_values_;
};

class UsernameChecker : public Checker {
 public:
  std::function<void(MysqlClient &cli)> verifier() override {
    return [](MysqlClient &cli) {
      auto cmd_res = query_one_result(cli, "SELECT USER()");
      ASSERT_NO_ERROR(cmd_res);

      EXPECT_THAT(*cmd_res,
                  ElementsAre(ElementsAre(cli.username() + "@localhost")));
    };
  }
};

TEST_P(ShareConnectionTinyPoolOneServerTest, restore) {
  if (!test_env->run_slow_tests() && GetParam().redundant_combination()) {
    GTEST_SKIP() << "skipped as RUN_SLOW_TESTS environment-variable is not set";
  }

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  // checkers
  std::vector<std::pair<std::string, std::unique_ptr<Checker>>> checkers;

  checkers.emplace_back(
      "schema", std::make_unique<SchemaChecker>(SchemaChecker::test_values_type{
                    "testing", "performance_schema"}));

  checkers.emplace_back(
      "initial-schema",
      std::make_unique<InitialSchemaChecker>(
          SchemaChecker::test_values_type{"testing", "performance_schema"}));

  checkers.emplace_back(
      "set-session-var",
      std::make_unique<SetSessionVarChecker>(
          SetSessionVarChecker::test_values_type{{"timestamp", "1.500000"},
                                                 {"unique_checks", "0"}}));

  checkers.emplace_back(
      "warnings",
      std::make_unique<WarningsChecker>(WarningsChecker::test_values_type{
          {"DO 0/0", 1365}, {"DO _utf8''", 3719}}));

  checkers.emplace_back(
      "no-warnings", std::make_unique<NoWarningsChecker>(
                         NoWarningsChecker::test_values_type{"DO 1", "DO 2"}));

  checkers.emplace_back(
      "session-attributes",
      std::make_unique<SessionAttributeChecker>(
          SessionAttributeChecker::test_values_type{{"v1", "1"}, {"v2", "2"}}));

  checkers.emplace_back("username", std::make_unique<UsernameChecker>());

  // scenarios
  std::vector<std::pair<std::string, std::vector<SharedServer::Account>>>
      scenarios;

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::caching_sha2_password_account());

    scenarios.emplace_back("one account", std::move(accounts));
  }

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::caching_sha2_password_account());
    accounts.push_back(SharedServer::caching_sha2_password_account());

    scenarios.emplace_back("same account, twice", std::move(accounts));
  }

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::caching_sha2_password_account());
    accounts.push_back(SharedServer::caching_sha2_empty_password_account());
    accounts.push_back(SharedServer::sha256_password_account());
    accounts.push_back(SharedServer::sha256_empty_password_account());

    scenarios.emplace_back("different accounts", std::move(accounts));
  }

  for (const auto &[scenario_name, accounts] : scenarios) {
    SCOPED_TRACE("// scenario: " + scenario_name);
    for (auto &[checker_name, checker] : checkers) {
      SCOPED_TRACE("// checker: " + checker_name);
      for (auto clean_pool_before_verify : {false, true}) {
        SCOPED_TRACE("// clean_pool_before_verify: " +
                     std::to_string(clean_pool_before_verify));
        for (auto &admin_cli : admin_clis()) {
          // reset the auth-cache
          SharedServer::flush_privileges(*admin_cli);

          // reset the router's connection-pool
          ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
        }

        std::vector<MysqlClient> clis;

        std::vector<std::function<void(MysqlClient &)>> verifiers;

        for (const auto &account : accounts) {
          SCOPED_TRACE("// account: " + account.username);
          MysqlClient cli;
          {
            cli.set_option(MysqlClient::GetServerPublicKey(true));
            cli.username(account.username);
            cli.password(account.password);

            checker->apply_before_connect(cli);

            auto connect_res = cli.connect(shared_router()->host(),
                                           shared_router()->port(GetParam()));
            if (!connect_res) {
              // auth may fail with DISABLED as the router has no public-key
              // cert
              GTEST_SKIP() << connect_res.error();
            }
            ASSERT_NO_ERROR(connect_res);
            //
            ASSERT_NO_ERROR(cli.ping());
          }

          checker->apply(cli);

          // remember the checker's verifier.
          verifiers.emplace_back(checker->verifier());
          clis.push_back(std::move(cli));

          // move to the next checker-value for the next connection.
          checker->advance();
        }

        if (clean_pool_before_verify && can_share && can_fetch_password) {
          // wait until all connections are pooled.
          ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(
              kMaxPoolSize, 10s));

          // only close the connections that are expected to be in the pool.
          for (auto &admin_cli : admin_clis()) {
            ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
          }

          // wait until all connections are pooled.
          ASSERT_NO_ERROR(
              shared_router()->wait_for_idle_server_connections(0, 10s));
        }

        // verify variables that were set are reapplied.
        for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
          verifiers[ndx](cli);
        }

        if (!clean_pool_before_verify) {
          // check that all client connections used the same server connection.
          //
          // ... if the pool was cleaned.
          std::pair<std::string, std::string> cli_connection_id{};

          // check connection was pooled.
          for (auto &cli : clis) {
            auto cmd_res =
                query_one_result(cli, "SELECT @@port, CONNECTION_ID()");
            ASSERT_NO_ERROR(cmd_res);

            auto result = *cmd_res;
            ASSERT_THAT(result, ::testing::SizeIs(1));
            ASSERT_THAT(result[0], ::testing::SizeIs(2));

            if (!cli_connection_id.first.empty()) {
              if (can_share && can_fetch_password) {
                EXPECT_EQ(cli_connection_id,
                          std::make_pair(result[0][0], result[0][1]));
              } else {
                EXPECT_NE(cli_connection_id,
                          std::make_pair(result[0][0], result[0][1]));
              }
            } else {
              // remember the first connection's id.
              cli_connection_id = std::make_pair(result[0][0], result[0][1]);
            }
          }
        }
      }
    }
  }
}

static stdx::expected<void, MysqlError> try_until_connection_available(
    std::function<stdx::expected<void, MysqlError>()> f,
    const std::chrono::seconds &timeout = 10s) {
  const auto end_time = std::chrono::steady_clock::now() + timeout;

  while (true) {
    auto res = f();

    if (res) break;

    const auto ec = res.error();
    // 1040 is Too many connections.
    if (ec.value() == 1040 && std::chrono::steady_clock::now() < end_time) {
      std::this_thread::sleep_for(20ms);
      continue;
    } else {
      return res;
    }
  }

  return {};
}

/**
 * check max-connection errors are properly forwarded to the client at connect.
 *
 * There are two scenarios:
 *
 * 1. if there is NO SUPER connection, then max-connection fails after
 *    authentication as a SUPER connection would still be allowed.
 * 2. If there is a SUPER connection, then max-connection fails at greeting
 *    as neither SUPER nor normal connections are allowed.
 */
TEST_P(ShareConnectionTinyPoolOneServerTest,
       classic_protocol_server_greeting_error) {
  SCOPED_TRACE("// set max-connections = 2, globally");
  // close all connections that are currently in the pool to get a stable
  // baseline.
  for (auto admin_cli : admin_clis()) {
    SharedServer::close_all_connections(*admin_cli);

    // there is one admin connection connection all the time.
    ASSERT_NO_ERROR(admin_cli->query("SET GLOBAL max_connections = 2"));
  }

  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));

  Scope_guard restore_at_end{[]() {
    auto reset_globals = []() -> stdx::expected<void, MysqlError> {
      for (auto *admin_cli : admin_clis()) {
        auto query_res =
            admin_cli->query("SET GLOBAL max_connections = DEFAULT");
        if (!query_res) return stdx::unexpected(query_res.error());
      }

      return {};
    };

    // it may take a while until the last connection of the test is closed
    // before this admin connection can be opened to reset the globals again.
    ASSERT_NO_ERROR(
        try_until_connection_available([&]() { return reset_globals(); }));
  }};

  SCOPED_TRACE("// testing");
  {
    SCOPED_TRACE("// connecting, keep open and block sharing");

    auto account = SharedServer::caching_sha2_empty_password_account();

    MysqlClient cli;  // keep it open
    {
      cli.username(account.username);
      cli.password(account.password);

      auto connect_res = cli.connect(shared_router()->host(),
                                     shared_router()->port(GetParam()));
      ASSERT_NO_ERROR(connect_res);

      // block sharing
      ASSERT_NO_ERROR(cli.query("START TRANSACTION WITH CONSISTENT SNAPSHOT"));
    }

    // fails at auth as the a SUPER account could still connect
    SCOPED_TRACE("// connect, fail with 'max-connections reached'");
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

    SCOPED_TRACE("// connect as SUPER user, keep open, block sharing");
    MysqlClient cli_super;  // keep it open
    {
      auto admin_account = SharedServer::admin_account();

      cli_super.username(admin_account.username);
      cli_super.password(admin_account.password);

      auto connect_res = cli_super.connect(shared_router()->host(),
                                           shared_router()->port(GetParam()));
      ASSERT_NO_ERROR(connect_res);

      // block sharing
      ASSERT_NO_ERROR(
          cli_super.query("START TRANSACTION WITH CONSISTENT SNAPSHOT"));
    }

    // fails at connect at greeting, as SUPER and max_connections are connected.
    SCOPED_TRACE("// connect, fail with 'max-connections reached'");
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

    // closing the super connection would make it end up in the pool, but the
    // scope-guard needs it to be closed to open its own connection.
    cli_super.query("KILL CONNECTION_ID()");
  }

  SCOPED_TRACE("// cleanup");

  // close all connections that are currently in the pool to get a stable
  // baseline.
  for (auto *admin_cli : admin_clis()) {
    ASSERT_NO_ERROR(try_until_connection_available([&]() {
      // reset the router's connection-pool
      return SharedServer::close_all_connections(*admin_cli);
    }));
  }
  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));

  // calls Scope_guard
}

/**
 * check max-connection errors are properly forwarded to the client after query.
 *
 * There are two scenarios:
 *
 * 1. if there is NO SUPER connection, then max-connection fails after
 *    authentication as a SUPER connection would still be allowed.
 * 2. If there is a SUPER connection, then max-connection fails at greeting
 *    as neither SUPER nor normal connections are allowed.
 */
TEST_P(ShareConnectionTinyPoolOneServerTest,
       classic_protocol_server_greeting_error_at_query) {
  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// set max-connections = 2, globally");

  // close all connections that are currently in the pool to get a stable
  // baseline.
  for (auto *admin_cli : admin_clis()) {
    ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
  }

  for (auto admin_cli : admin_clis()) {
    SharedServer::close_all_connections(*admin_cli);

    // there is one admin connection connection all the time.
    ASSERT_NO_ERROR(admin_cli->query("SET GLOBAL max_connections = 2"));
  }

  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));

  Scope_guard restore_at_end{[]() {
    auto reset_globals = []() -> stdx::expected<void, MysqlError> {
      for (auto *admin_cli : admin_clis()) {
        auto query_res =
            admin_cli->query("SET GLOBAL max_connections = DEFAULT");
        if (!query_res) return stdx::unexpected(query_res.error());
      }

      return {};
    };

    // it may take a while until the last connection of the test is closed
    // before this admin connection can be opened to reset the globals again.
    ASSERT_NO_ERROR(
        try_until_connection_available([&]() { return reset_globals(); }));
  }};

  SCOPED_TRACE("// testing");
  {
    SCOPED_TRACE("// connecting, keep open and block sharing");

    auto account = SharedServer::caching_sha2_empty_password_account();

    MysqlClient cli1;  // keep it open
    {
      cli1.username(account.username);
      cli1.password(account.password);

      auto connect_res = cli1.connect(shared_router()->host(),
                                      shared_router()->port(GetParam()));
      ASSERT_NO_ERROR(connect_res);
    }

    if (can_share) {
      // wait until the connection is pooled.
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }

    SCOPED_TRACE("// connect, fail with 'max-connections reached'");
    {
      MysqlClient cli2;

      cli2.username(account.username);
      cli2.password(account.password);

      auto connect_res = cli2.connect(shared_router()->host(),
                                      shared_router()->port(GetParam()));
      if (can_share) {
        ASSERT_NO_ERROR(connect_res);

        // block the connection.
        ASSERT_NO_ERROR(
            cli2.query("START TRANSACTION WITH CONSISTENT SNAPSHOT"));

        // trigger a reconnect on the earlier connection should fail with
        // "max-connections"
        auto cmd_res = cli1.query("DO 1");
        ASSERT_ERROR(cmd_res);
        EXPECT_EQ(cmd_res.error().value(), 1040);
      } else {
        ASSERT_ERROR(connect_res);
        EXPECT_EQ(connect_res.error().value(), 1040);
      }
    }
  }

  SCOPED_TRACE("// cleanup");

  // close all connections that are currently in the pool to get a stable
  // baseline.
  for (auto *admin_cli : admin_clis()) {
    ASSERT_NO_ERROR(try_until_connection_available([&]() {
      // reset the router's connection-pool
      return SharedServer::close_all_connections(*admin_cli);
    }));
  }
  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));

  // calls Scope_guard
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionTinyPoolOneServerTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

TEST_P(ShareConnectionSmallPoolTwoServersTest, round_robin_all_in_pool) {
  for (auto &s : shared_servers()) {
    s->flush_privileges();  // reset the auth-cache
  }

  std::array<MysqlClient, 6> clis;

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  for (auto &cli : clis) {
    auto account = SharedServer::caching_sha2_password_account();

    cli.username(account.username);
    cli.password(account.password);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (!can_fetch_password) {
      ASSERT_ERROR(connect_res);

      // Authentication plugin 'caching_sha2_password' reported error:
      // Authentication requires secure connection.
      EXPECT_EQ(connect_res.error().value(), 2061);

      return;
    }
    ASSERT_NO_ERROR(connect_res);
    // wait for the server-connections to make it to the pool.
    //
    // after a connect() the router sends SET ... to the server before the next
    // statement is read from the client.
    ASSERT_NO_ERROR(cli.ping());
  }

  std::array<std::pair<std::string, std::string>, clis.size()>
      cli_connection_ids{};

  // get the connection-id and the port of the server each connection is
  // assigned to.
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    auto cmd_res = query_one_result(cli, "SELECT @@port, CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    auto result = std::move(*cmd_res);
    ASSERT_THAT(result, ::testing::SizeIs(1));
    ASSERT_THAT(result[0], ::testing::SizeIs(2));
    cli_connection_ids[ndx] = std::make_pair(result[0][0], result[0][1]);
  }

  // round-robin
  for (size_t ndx{}; ndx < clis.size() - 1; ++ndx) {
    EXPECT_NE(cli_connection_ids[ndx], cli_connection_ids[ndx + 1]);
  }

  for (size_t ndx{}; ndx < clis.size() - 2; ++ndx) {
    if (can_share && can_fetch_password) {
      // every 2nd connection uses the same server-connection.
      EXPECT_EQ(cli_connection_ids[ndx], cli_connection_ids[ndx + 2]);
    } else {
      EXPECT_NE(cli_connection_ids[ndx], cli_connection_ids[ndx + 2]);
    }
  }
}

TEST_P(ShareConnectionSmallPoolTwoServersTest, round_robin_all_in_pool_purge) {
  std::array<MysqlClient, 6> clis;

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  for (auto &cli : clis) {
    auto account = SharedServer::caching_sha2_password_account();

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    // wait for the server-connections to make it to the pool.
    //
    // after a connect() the router sends SET ... to the server before the next
    // statement is read from the client.
    ASSERT_NO_ERROR(cli.ping());

    if (can_share && can_fetch_password) {
      // purge
      for (auto *admin_cli : admin_clis()) {
        // reset the auth-cache
        SharedServer::flush_privileges(*admin_cli);
        // reset the router's connection-pool
        ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
      }
    }
  }

  std::array<std::pair<std::string, std::string>, clis.size()>
      cli_connection_ids{};

  // get the connection-id and the port of the server each connection is
  // assigned to.
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    {
      auto cmd_res = query_one_result(cli, "SELECT @@port, CONNECTION_ID()");
      ASSERT_NO_ERROR(cmd_res);

      auto result = std::move(*cmd_res);

      ASSERT_THAT(result, ::testing::SizeIs(1));
      ASSERT_THAT(result[0], ::testing::SizeIs(2));
      cli_connection_ids[ndx] = std::make_pair(result[0][0], result[0][1]);
    }

    if (can_share && can_fetch_password) {
      for (auto *admin_cli : admin_clis()) {
        // reset the router's connection-pool
        ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
      }
    }

    // running it again should:
    //
    // - connect to the same server, but the same port.
    {
      auto cmd_res = query_one_result(clis[ndx], "SELECT @@port");
      ASSERT_NO_ERROR(cmd_res);

      auto result = std::move(*cmd_res);
      ASSERT_THAT(result, ::testing::SizeIs(1));
      ASSERT_THAT(result[0], ::testing::SizeIs(1));

      // connection-id will change, but port should be the same.
      EXPECT_EQ(cli_connection_ids[ndx].first, result[0][0]);
    }
  }

  // round-robin
  for (size_t ndx{}; ndx < clis.size() - 1; ++ndx) {
    EXPECT_NE(cli_connection_ids[ndx], cli_connection_ids[ndx + 1]);
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionSmallPoolTwoServersTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

/*
 * pool-size: 1
 *
 * routes:
 * - [0]: s[0]
 * - [1]: s[1..2] (round-robin)
 */
using ShareConnectionTinyPoolTwoRoutesTest =
    ShareConnectionTestTemp<3, 1, true>;

/*
 * check if multiple routes don't harm sharing.
 *
 * pool-size: 1
 *
 * routes:
 * - [0]: s[0]
 * - [1]: s[1..2] (round-robin)
 *
 * connections:
 * 1. route[1] -> s[1] - new, to-stash
 * 2. route[1] -> s[2] - new, to-stash
 * 3. route[1] -> s[1] - from-stash, to-stash
 * 4. route[1] -> s[2] - from-stash, full-stash
 */
TEST_P(ShareConnectionTinyPoolTwoRoutesTest, round_robin_one_route) {
  for (auto &s : shared_servers()) {
    s->flush_privileges();  // reset the auth-cache
  }

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  constexpr const int max_clients = 6;

  std::array<MysqlClient, max_clients> clis{};

  const auto account = SharedServer::caching_sha2_password_account();

  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    SCOPED_TRACE("// ndx = " + std::to_string(ndx));

    cli.username(account.username);
    cli.password(account.password);

    const size_t route_ndx = 1;

    auto connect_res = cli.connect(
        shared_router()->host(), shared_router()->port(GetParam(), route_ndx));

    if (!can_fetch_password) {
      ASSERT_ERROR(connect_res);

      // Authentication plugin 'caching_sha2_password' reported error:
      // Authentication requires secure connection.
      EXPECT_EQ(connect_res.error().value(), 2061);

      return;
    }
    ASSERT_NO_ERROR(connect_res);

    if (can_share && can_fetch_password) {
      int expected_stashed = 0;
      switch (ndx) {
        case 0:  // route[1][0] - s[1]
          expected_stashed = 1;
          break;
        case 1:  // route[1][1] - s[2]
        case 2:  // route[1][0] - s[1] - from-stash
        case 3:  // route[1][1] - s[2] - from-stash
        case 4:  // route[1][0] - s[1] - from-stash
        case 5:  // route[1][1] - s[2] - from-stash
          expected_stashed = 2;
          break;
      }

      EXPECT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(
          expected_stashed, 1s));
    }
  }

  // no connection was closed -> nothing idles.
  EXPECT_EQ(0, shared_router()->idle_server_connections());

  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    SCOPED_TRACE("// ndx = " + std::to_string(ndx));

    cli.close();

    if (can_share && can_fetch_password) {
      int expected_pooled = 0;
      switch (ndx) {
        case 0:  // route[1][0] - s[1] - no server-side
        case 1:  // route[0][0] - s[2] - no server-side
        case 2:  // route[1][1] - s[1] - no server-side
        case 3:  // route[0][0] - s[2] - no server-side
          expected_pooled = 0;
          break;
        case 4:  // route[1][0] - s[1] - pooled
        case 5:  // route[1][1] - s[2] - closed
          expected_pooled = 1;
          break;
      }

      // one connection gets pooled, and stays there.
      EXPECT_NO_ERROR(shared_router()->wait_for_idle_server_connections(
          expected_pooled, 1s));
    }
  }

  SCOPED_TRACE("// the connection from the pool can be reused.");
  {
    MysqlClient cli;

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam(), 1)));

    if (can_share && can_fetch_password) {
      // the connection to s[1] should be in the pool, ... and now reused.
      EXPECT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 1s));
    }
  }
}

/*
 * check if multiple routes access the same pool.
 *
 * pool-size: 1
 *
 * routes:
 * - [0]: s[0]
 * - [1]: s[1..2] (round-robin)
 *
 * connections:
 * 1. route[1] -> s[1] - new, to-stash
 * 2. route[0] -> s[0] - new, to-stash
 * 3. route[1] -> s[2] - new, to-stash
 * 4. route[0] -> s[0] - from-stash, to-stash
 * 5. route[1] -> s[1] - from-stash, to-stash
 * 6. route[1] -> s[2] - from-stash, to-stash
 */
TEST_P(ShareConnectionTinyPoolTwoRoutesTest, round_robin_two_routes) {
  for (auto &s : shared_servers()) {
    s->flush_privileges();  // reset the auth-cache
  }

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  constexpr const int max_clients = 6;

  std::array<MysqlClient, max_clients> clis{};

  const auto account = SharedServer::caching_sha2_password_account();

  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    SCOPED_TRACE("// ndx = " + std::to_string(ndx));

    cli.username(account.username);
    cli.password(account.password);

    const size_t route_ndx =
        (ndx == 0 || ndx == 2 || ndx == 4 || ndx == 5) ? 1 : 0;

    auto connect_res = cli.connect(
        shared_router()->host(), shared_router()->port(GetParam(), route_ndx));

    if (!can_fetch_password) {
      ASSERT_ERROR(connect_res);

      // Authentication plugin 'caching_sha2_password' reported error:
      // Authentication requires secure connection.
      EXPECT_EQ(connect_res.error().value(), 2061);

      return;
    }
    ASSERT_NO_ERROR(connect_res);

    if (can_share && can_fetch_password) {
      int expected_stashed = 0;
      switch (ndx) {
        case 0:  // route[1][0] - s[1]
          expected_stashed = 1;
          break;
        case 1:  // route[0][0] - s[0]
          expected_stashed = 2;
          break;
        case 2:  // route[1][1] - s[2]
        case 3:  // route[0][0] - s[0] - from-stash
        case 4:  // route[1][0] - s[1] - from-stash
        case 5:  // route[1][1] - s[2] - from-stash
          expected_stashed = 3;
          break;
      }

      EXPECT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(
          expected_stashed, 1s));
    }
  }

  // no connection was closed -> nothing idles.
  EXPECT_EQ(0, shared_router()->idle_server_connections());

  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    SCOPED_TRACE("// ndx = " + std::to_string(ndx));

    cli.close();

    if (can_share && can_fetch_password) {
      int expected_pooled = 0;
      switch (ndx) {
        case 0:  // route[1][0] - s[1] - no server-side
        case 1:  // route[0][0] - s[0] - no server-side
        case 2:  // route[1][1] - s[2] - no server-side
          expected_pooled = 0;
          break;
        case 3:  // route[0][0] - s[0] - pooled
        case 4:  // route[1][0] - s[1] - closed
        case 5:  // route[1][1] - s[2] - closed
          expected_pooled = 1;
          break;
      }

      // one connection gets pooled, and stays there.
      EXPECT_NO_ERROR(shared_router()->wait_for_idle_server_connections(
          expected_pooled, 1s));
    }
  }

  SCOPED_TRACE("// the connection from the pool can be reused.");
  {
    MysqlClient cli;

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam(), 0)));

    if (can_share && can_fetch_password) {
      // the connection to s[0] should be in the pool, ... and now reused.
      EXPECT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 1s));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionTinyPoolTwoRoutesTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

TEST_P(ShareConnectionNoPoolOneServersTest, classic_protocol_tls_resumption) {
  std::string last_hits;

  auto account = SharedServer::caching_sha2_empty_password_account();

  for (int round = 0; round < 10; ++round) {
    SCOPED_TRACE("// connecting to server - round " + std::to_string(round));
    std::string hits;

    {
      MysqlClient cli;

      cli.username(account.username);
      cli.password(account.password);

      SCOPED_TRACE("// connect");
      ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                  shared_router()->port(GetParam())));

      SCOPED_TRACE("// checking TLS resumptions with the server.");

      auto hits_res = query_one_result(
          cli,
          "SELECT variable_value "
          "  FROM performance_schema.session_status "
          " WHERE variable_name LIKE 'Ssl_session_cache_hits'");
      ASSERT_NO_ERROR(hits_res);
      ASSERT_THAT(*hits_res, SizeIs(testing::Eq(1)));
      std::string hits = (*hits_res)[0][0];

      ASSERT_THAT(hits, Not(IsEmpty()));

      if (!last_hits.empty()) {
        // first round.
        // with TLS on the server-side there should be TLS resumption.
        if (GetParam().server_ssl_mode == kPreferred ||
            GetParam().server_ssl_mode == kRequired ||
            (GetParam().server_ssl_mode == kAsClient &&
             (GetParam().client_ssl_mode == kPreferred ||
              GetParam().client_ssl_mode == kRequired))) {
          // the hits should increase.
          //
          // As the metadata-cache also connects to the backends and resumes TLS
          // connections we don't know
          EXPECT_NE(last_hits, hits);
        }
      }
    }

    last_hits = hits;

    // as the connections to the servers are closed asynchronously,
    // wait until all user connections are closed on the server side.
    auto end = std::chrono::steady_clock::now() + 1s;
    do {
      auto user_connections_ids_res =
          SharedServer::user_connection_ids(*(admin_clis()[0]));
      ASSERT_NO_ERROR(user_connections_ids_res);

      if (user_connections_ids_res->empty()) break;

      ASSERT_LT(std::chrono::steady_clock::now(), end);

      std::this_thread::sleep_for(10ms);
    } while (true);
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionNoPoolOneServersTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
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
