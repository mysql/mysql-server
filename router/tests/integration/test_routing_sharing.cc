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
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

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
#include "scope_guard.h"
#include "shared_server.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StartsWith;

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
std::string find_executable_path(const std::string &name) {
  std::string path(getenv("PATH"));

#ifdef _WIN32
  const char path_sep = ';';
#else
  const char path_sep = ':';
#endif

  for (const auto &subpath : mysql_harness::split_string(path, path_sep)) {
    // the path can end with the separator so the last value can be ""
    if (!subpath.empty()) {
      auto fn = mysql_harness::Path(subpath).join(name);
      if (fn.exists()) return fn.str();
    }
  }

  return {};
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
changed_event_counters(MysqlClient &cli, uint64_t connection_id) {
  return changed_event_counters_impl(
      cli,
      "SELECT EVENT_NAME, COUNT_STAR FROM "
      "performance_schema.events_statements_summary_by_thread_by_event_name AS "
      "e JOIN performance_schema.threads AS t ON (e.THREAD_ID = t.THREAD_ID) "
      "WHERE t.PROCESSLIST_ID = " +
          std::to_string(connection_id) +
          " AND COUNT_STAR > 0 ORDER BY EVENT_NAME");
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
  SharedRouter(TcpPortPool &port_pool, uint64_t pool_size)
      : port_pool_(port_pool),
        pool_size_{pool_size},
        rest_port_{port_pool_.get_next_available()} {}

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

    for (const auto &param : share_connection_params) {
      auto port_key =
          std::make_tuple(param.client_ssl_mode, param.server_ssl_mode);
      auto ports_it = ports_.find(port_key);

      const auto port =
          ports_it == ports_.end()
              ? (ports_[port_key] = port_pool_.get_next_available())
              : ports_it->second;

      writer.section(
          "routing:classic_" + param.testname,
          {
              {"bind_port", std::to_string(port)},
              {"destinations", mysql_harness::join(destinations, ",")},
              {"protocol", "classic"},
              {"routing_strategy", "round-robin"},

              {"client_ssl_mode", std::string(param.client_ssl_mode)},
              {"server_ssl_mode", std::string(param.server_ssl_mode)},

              {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
              {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
              {"connection_sharing", "1"},
              {"connection_sharing_delay", "0"},
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

  [[nodiscard]] uint16_t port(const ShareConnectionParam &param) const {
    return ports_.at(
        std::make_tuple(param.client_ssl_mode, param.server_ssl_mode));
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
  std::map<std::tuple<std::string_view, std::string_view>, uint16_t> ports_;

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

      auto connect_res = cli->connect(s->server_host(), s->server_port());
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
  if (!query_res) return stdx::unexpected(query_res.error());

  // get the first field, of the first row of the first resultset.
  for (const auto &result : *query_res) {
    if (result.field_count() == 0) {
      return stdx::unexpected(MysqlError(1, "not a resultset", "HY000"));
    }

    for (auto row : result.rows()) {
      auto connection_id = strtoull(row[0], nullptr, 10);

      return connection_id;
    }
  }

  return stdx::unexpected(MysqlError(1, "no rows", "HY000"));
}

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

  SharedRouter *shared_router() { return TestWithSharedRouter::router(); }

  ~ShareConnectionTestBase() override {
    if (::testing::Test::HasFailure()) {
      shared_router()->process_manager().dump_logs();
      for (auto &srv : shared_servers()) {
        srv->process_manager().dump_logs();
      }
    }
  }

 protected:
  const std::string valid_ssl_key_{SSL_TEST_DATA_DIR "/server-key-sha512.pem"};
  const std::string valid_ssl_cert_{SSL_TEST_DATA_DIR
                                    "/server-cert-sha512.pem"};

  const std::string wrong_password_{"wrong_password"};
  const std::string empty_password_{""};
};

class ShareConnectionTest
    : public ShareConnectionTestBase,
      public ::testing::WithParamInterface<ShareConnectionParam> {
 public:
#if 0
#define TRACE(desc) trace(__func__, __LINE__, (desc))
#else
#define TRACE(desc)
#endif

  void SetUp() override {
    TRACE("");

    for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
      // shared_server_ may be null if TestWithSharedServer::SetUpTestSuite
      // threw?
      if (s == nullptr || s->mysqld_failed_to_start()) {
        GTEST_SKIP() << "failed to start mysqld";
      } else {
        auto cli = admin_clis()[ndx];

        // reset the router's connection-pool
        ASSERT_NO_ERROR(SharedServer::close_all_connections(*cli));
        SharedServer::reset_to_defaults(*cli);
      }
    }
    TRACE("");
  }

  void trace(std::string_view func_name, int line, std::string_view desc) {
    std::ostringstream oss;

    oss << func_name << "." << line << ": " << (clock_type::now() - started_)
        << ": " << desc << "\n";

    std::cerr << oss.str();
  }

 protected:
  using clock_type = std::chrono::steady_clock;

  clock_type::time_point started_{clock_type::now()};
};

/**
 * check connections can be shared after the connection is established.
 *
 * - connect
 * - wait for connection be pooled
 * - connect a 2nd connection to same backend
 * - check they share the same connection
 */
TEST_P(ShareConnectionTest, classic_protocol_share_after_connect_same_user) {
  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 4> clis;

  std::array<SharedServer::Account, clis.size()> accounts{
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account()};

  const bool can_share = GetParam().can_share();
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    auto account = accounts[ndx];

    cli.username(account.username);
    cli.password(account.password);

    // wait until connection 0, 1, 2 are in the pool as 3 shall share with 0.
    if (ndx == 3 && can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(3, 10s));
    }

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

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

  const bool can_share = GetParam().can_share();
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

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));

    if (GetParam().client_ssl_mode == kDisabled ||
        GetParam().server_ssl_mode == kDisabled) {
      // should fail as the connection is not secure.
      ASSERT_ERROR(connect_res);
      if (GetParam().server_ssl_mode == kDisabled ||
          GetParam().server_ssl_mode == kAsClient) {
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

  const bool can_share = GetParam().can_share();
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

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));

    if (GetParam().client_ssl_mode == kDisabled ||
        GetParam().server_ssl_mode == kDisabled) {
      // should fail as the connection is not secure.
      ASSERT_ERROR(connect_res);
      if (GetParam().server_ssl_mode == kDisabled ||
          GetParam().server_ssl_mode == kAsClient) {
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

/**
 * check connections get routed to different backends even if the pool is
 * purged.
 *
 * - connect
 * - wait for connection be pooled
 * - connect a 2nd connection to same backend
 * - check they share the same connection
 */
TEST_P(ShareConnectionTest, classic_protocol_purge_after_connect_same_user) {
  TRACE("start");

  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 7> clis;

  std::array<SharedServer::Account, clis.size()> accounts{
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account()};

  std::array<std::pair<uint16_t, uint64_t>, clis.size()> cli_ids{};

  const bool can_share = GetParam().can_share();
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    auto account = accounts[ndx];

    cli.username(account.username);
    cli.password(account.password);

    TRACE("connect");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    TRACE("connected " + std::to_string(ndx));

    // wait until the connection is in the pool.
    if (can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
      TRACE("waited is-idle " + std::to_string(ndx));
    }

    // find it on one of the servers and kill it.
    for (auto [s_ndx, s] : stdx::views::enumerate(shared_servers())) {
      auto *srv_cli = admin_clis()[s_ndx];

      auto ids_res = SharedServer::user_connection_ids(*srv_cli);
      ASSERT_NO_ERROR(ids_res);

      auto ids = *ids_res;

      if (ids.empty()) continue;

      EXPECT_THAT(ids, ::testing::SizeIs(1));

      for (auto id : ids) {
        ASSERT_NO_ERROR(srv_cli->query("KILL " + std::to_string(id)));

        cli_ids[ndx] = std::make_pair(s->server_port(), id);
      }
    }

    TRACE("killed connection for " + std::to_string(ndx));

    // wait until it is gone from the pool.
    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));
  }

  TRACE("check result");

  // check that no connection is reused ...
  EXPECT_THAT(cli_ids,
              ::testing::AllOf(::testing::Contains(cli_ids[0]).Times(1),
                               ::testing::Contains(cli_ids[1]).Times(1),
                               ::testing::Contains(cli_ids[2]).Times(1),
                               ::testing::Contains(cli_ids[3]).Times(1),
                               ::testing::Contains(cli_ids[4]).Times(1),
                               ::testing::Contains(cli_ids[5]).Times(1),
                               ::testing::Contains(cli_ids[6]).Times(1)));

  EXPECT_EQ(cli_ids[0].first, cli_ids[3].first);
  EXPECT_EQ(cli_ids[0].first, cli_ids[6].first);
  EXPECT_EQ(cli_ids[1].first, cli_ids[4].first);
  EXPECT_EQ(cli_ids[2].first, cli_ids[5].first);
}

/*
 * check connections get routed to different backends if the connection
 * pool is pooled.
 */
TEST_P(ShareConnectionTest, classic_protocol_pool_after_connect_same_user) {
  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 7> clis;

  std::array<SharedServer::Account, clis.size()> accounts{
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_empty_password_account()};

  std::array<std::pair<uint16_t, uint64_t>, clis.size()> cli_ids{};

  std::map<std::pair<uint16_t, uint64_t>,
           std::vector<std::pair<std::string, uint32_t>>>
      last_events{};

  const bool can_share = GetParam().can_share();
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    SCOPED_TRACE("// connection [" + std::to_string(ndx) + "]");

    auto account = accounts[ndx];

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    // wait until the connection is in the pool.
    if (can_share) {
      size_t expected_pooled_connections = ndx < 3 ? ndx + 1 : 3;

      ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(
          expected_pooled_connections, 10s));
    }

    // find the server which received the connection attempt.
    for (auto [s_ndx, s] : stdx::views::enumerate(shared_servers())) {
      auto *srv_cli = admin_clis()[s_ndx];

      auto ids_res = SharedServer::user_connection_ids(*srv_cli);
      ASSERT_NO_ERROR(ids_res);

      auto ids = *ids_res;

      if (can_share) {
        EXPECT_THAT(ids, SizeIs(::testing::Lt(2)));
      }

      for (auto id : ids) {
        auto events_res = changed_event_counters(*srv_cli, id);
        ASSERT_NO_ERROR(events_res);

        auto connection_id = std::make_pair(s->server_port(), id);
        auto last_it = last_events.find(connection_id);

        if (can_share) {
          // it should at least change a set-option-event.
          if (*events_res != last_events[connection_id]) {
            cli_ids[ndx] = connection_id;
            last_events[connection_id] = *events_res;
          }
        } else {
          // find the one that's new
          if (last_it == last_events.end()) {
            cli_ids[ndx] = connection_id;
            last_events[connection_id] = *events_res;
          }
        }
      }
    }
  }

  if (can_share) {
    // check that connections are reused ...
    EXPECT_THAT(cli_ids,
                ::testing::AllOf(::testing::Contains(cli_ids[0]).Times(3),
                                 ::testing::Contains(cli_ids[1]).Times(2),
                                 ::testing::Contains(cli_ids[2]).Times(2)));
  } else {
    EXPECT_THAT(cli_ids,
                ::testing::AllOf(::testing::Contains(cli_ids[0]).Times(1),
                                 ::testing::Contains(cli_ids[1]).Times(1),
                                 ::testing::Contains(cli_ids[2]).Times(1),
                                 ::testing::Contains(cli_ids[3]).Times(1),
                                 ::testing::Contains(cli_ids[4]).Times(1),
                                 ::testing::Contains(cli_ids[5]).Times(1),
                                 ::testing::Contains(cli_ids[6]).Times(1)));
  }

  // ... and connected to different hosts
  EXPECT_EQ(cli_ids[0].first, cli_ids[3].first);
  EXPECT_EQ(cli_ids[0].first, cli_ids[6].first);
  EXPECT_EQ(cli_ids[1].first, cli_ids[4].first);
  EXPECT_EQ(cli_ids[2].first, cli_ids[5].first);
}

TEST_P(ShareConnectionTest, classic_protocol_share_password_changed_query) {
  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 4> clis;

  SharedServer::Account account{
      "onetime",
      "",  // no password.
      "caching_sha2_password",
  };

  for (auto *srv : shared_servers()) {
    auto cli_res = srv->admin_cli();
    ASSERT_NO_ERROR(cli_res);

    auto cli = std::move(*cli_res);

    ASSERT_NO_ERROR(cli.query("DROP USER IF EXISTS " + account.username));

    SharedServer::create_account(cli, account);
  }

  const bool can_share = GetParam().can_share();

  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    SCOPED_TRACE("// connection [" + std::to_string(ndx) + "]");

    cli.username(account.username);
    cli.password(account.password);
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    // wait until the connection is in the pool.
    if (can_share) {
      size_t expected_pooled_connections = ndx < 3 ? ndx + 1 : 3;

      ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(
          expected_pooled_connections, 10s));
    }
  }

  SCOPED_TRACE(
      "// change the password of the 'onetime' user to force a reauth fail.");
  for (auto *srv : shared_servers()) {
    auto cli_res = srv->admin_cli();
    ASSERT_NO_ERROR(cli_res);

    auto cli = std::move(*cli_res);

    ASSERT_NO_ERROR(cli.query("ALTER USER " + account.username +
                              " IDENTIFIED BY 'someotherpass'"));
  }

  SCOPED_TRACE("// check if a changed password has handled properly.");
  {
    auto &cli = clis[0];
    auto cmd_res = cli.query("DO 1");
    if (can_share) {
      ASSERT_ERROR(cmd_res);
      EXPECT_EQ(cmd_res.error().value(), 1045);
      EXPECT_THAT(cmd_res.error().message(),
                  testing::HasSubstr("while reauthenticating"));
    } else {
      ASSERT_NO_ERROR(cmd_res);
    }
  }
}

/**
 * check connections can be shared after the connection is established.
 *
 * - connect
 * - wait for connection be pooled
 * - connect a 2nd connection to same backend
 * - check they share the same connection
 */
TEST_P(ShareConnectionTest,
       classic_protocol_share_after_connect_different_user) {
  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 4> clis;

  std::array<SharedServer::Account, clis.size()> accounts{
      SharedServer::caching_sha2_empty_password_account(),
      SharedServer::caching_sha2_password_account(),
      SharedServer::caching_sha2_password_account(),
      SharedServer::caching_sha2_empty_password_account()};

  const bool can_share = GetParam().can_share();
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    auto account = accounts[ndx];

    SCOPED_TRACE("// connect[" + std::to_string(ndx) + "] as " +
                 account.username);

    cli.username(account.username);
    cli.password(account.password);

    // wait until connection 0, 1, 2 are in the pool as 3 shall share with 0.
    if (ndx == 3 && can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(3, 10s));
    }

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (GetParam().client_ssl_mode == kDisabled &&
        account.username ==
            SharedServer::caching_sha2_password_account().username) {
      // 2061 Authentication plugin requires secure connection.
      ASSERT_ERROR(connect_res);
      GTEST_SKIP() << connect_res.error();
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

/*
 * check connections get routed to the same backends if the connection lost.
 */
TEST_P(ShareConnectionTest, classic_protocol_connection_is_sticky_purged) {
  // 4 connections are needed as router does round-robin over 3 endpoints
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  const bool can_share = GetParam().can_share();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  std::array<std::string, 2> connection_id{};

  for (size_t round = 0; round < 3; ++round) {
    {
      auto cmd_res = query_one<2>(cli, "SELECT @@port, CONNECTION_ID()");
      ASSERT_NO_ERROR(cmd_res);

      // expect the same port
      if (round > 0) {
        EXPECT_EQ(connection_id[0], (*cmd_res)[0]);
        if (can_share) {
          // but different connection-ids (as the connection got killed)
          EXPECT_NE(connection_id[1], (*cmd_res)[1]);
        }
      }

      connection_id = *cmd_res;
    }

    // wait until the connection is in the pool ... and kill it.
    if (can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));

      for (auto *admin_cli : admin_clis()) {
        ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
      }

      ASSERT_NO_ERROR(
          shared_router()->wait_for_idle_server_connections(0, 10s));
    }
  }
}

/*
 * check connections get routed to the same backends if the connection pooled.
 */
TEST_P(ShareConnectionTest, classic_protocol_connection_is_sticky_pooled) {
  // 4 connections are needed as router does round-robin over 3 endpoints
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  const bool can_share = GetParam().can_share();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  std::array<std::string, 2> connection_id{};

  for (size_t round = 0; round < 3; ++round) {
    {
      auto cmd_res = query_one<2>(cli, "SELECT @@port, CONNECTION_ID()");
      ASSERT_NO_ERROR(cmd_res);

      // expect the same port and connection-id
      if (round > 0) {
        EXPECT_EQ(connection_id[0], (*cmd_res)[0]);
        EXPECT_EQ(connection_id[1], (*cmd_res)[1]);
      }

      connection_id = *cmd_res;
    }

    // wait until the connection is in the pool ... and kill it.
    if (can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    }
  }
}

/**
 * two connections using the same shared server connection.
 */
TEST_P(ShareConnectionTest, classic_protocol_share_same_user) {
  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 4> clis;

  const bool can_share = GetParam().can_share();
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    if (can_share) {
      if (ndx == 0) {
        ASSERT_NO_ERROR(
            shared_router()->wait_for_stashed_server_connections(1, 10s));
      } else if (ndx == 3) {
        ASSERT_NO_ERROR(
            shared_router()->wait_for_stashed_server_connections(3, 10s));
      }
    }
  }

  //
  {
    auto events_res = changed_event_counters(clis[0]);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Change user", 2),
                              Pair("statement/sql/select", 2),
                              Pair("statement/sql/set_option", 3)));
    } else {
      EXPECT_THAT(*events_res, IsEmpty());
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
      EXPECT_THAT(*events_res, IsEmpty());
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
      EXPECT_THAT(*events_res, IsEmpty());
    }
  }

  // shared with cli[0] on host1
  {
    auto events_res = changed_event_counters(clis[3]);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Change user", 3),
                              Pair("statement/sql/select", 3),
                              Pair("statement/sql/set_option", 4)));
    } else {
      EXPECT_THAT(*events_res, IsEmpty());
    }
  }
}

/**
 * two connections using the same shared server connection.
 */
TEST_P(ShareConnectionTest, classic_protocol_share_different_accounts) {
  // 4 connections are needed as router does round-robin over 3 endpoints
  MysqlClient cli1, cli2, cli3, cli4;

  // if the router has no cert, it can't provide a public-key over plaintext
  // channels.
  //
  //
  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  {
    auto account = SharedServer::caching_sha2_password_account();

    cli1.set_option(MysqlClient::GetServerPublicKey(true));
    cli1.username(account.username);
    cli1.password(account.password);

    ASSERT_NO_ERROR(cli1.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));
  }

  {
    auto account = SharedServer::sha256_password_account();

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
  }

  {
    auto account = SharedServer::caching_sha2_password_account();

    cli3.set_option(MysqlClient::GetServerPublicKey(true));
    cli3.username(account.username);
    cli3.password(account.password);

    ASSERT_NO_ERROR(cli3.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));
  }

  // wait a bit until all connections are moved to the pool to ensure that cli4
  // can share with cli1
  if (can_share && can_fetch_password) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(3, 10s));
  }

  // shares with cli1
  {
    auto account = SharedServer::caching_sha2_empty_password_account();

    cli4.set_option(MysqlClient::GetServerPublicKey(true));
    cli4.username(account.username);
    cli4.password(account.password);

    ASSERT_NO_ERROR(cli4.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));
  }

  // wait a bit until the connection cli4 is moved to the pool.
  if (can_share && can_fetch_password) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(3, 10s));
  }

  // shared between cli1 and cli4
  {
    auto events_res = changed_event_counters(cli1);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        // cli1: set-option
        // cli4: change-user + set-option
        // cli1: change-user + set-option (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Change user", 2),
                                Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 3)));
      } else {
        // cli1: set-option
        // cli1: (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/set_option", 1)));
      }
    } else {
      // no sharing possible, router is not injection SET statements.
      EXPECT_THAT(*events_res, ::testing::IsEmpty());
    }
  }

  // cli2
  {
    auto events_res = changed_event_counters(cli2);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)));
      } else {
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/set_option", 1)));
      }
    } else {
      EXPECT_THAT(*events_res, ::testing::IsEmpty());
    }
  }

  // cli3
  {
    auto events_res = changed_event_counters(cli3);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)));
      } else {
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/set_option", 1)));
      }
    } else {
      EXPECT_THAT(*events_res, ::testing::IsEmpty());
    }
  }

  // shared with cli1 on host1
  {
    auto events_res = changed_event_counters(cli4);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        // cli1: set-option
        // cli4: change-user + set-option
        // cli1: change-user + set-option + select
        // cli4: change-user + set-option (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Change user", 3),
                                Pair("statement/sql/select", 3),
                                Pair("statement/sql/set_option", 4)));
      } else {
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)));
      }
    } else {
      // cli4: (+ select)
      EXPECT_THAT(*events_res, ::testing::IsEmpty());
    }
  }

  // shared with cli1 on host1
  {
    auto events_res = changed_event_counters(cli4);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        // cli1: set-option
        // cli4: change-user + set-option
        // cli1: change-user + set-option + select
        // cli4: change-user + set-option + select
        // cli4: reset-connection + set-option (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Change user", 3),
                                Pair("statement/sql/select", 4),
                                Pair("statement/sql/set_option", 4)));
      } else {
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 1)));
      }
    } else {
      // cli4: select
      // cli4: (+ select)
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/select", 1)));
    }
  }

  // shared with cli4 on host1
  {
    auto events_res = changed_event_counters(cli1);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        // cli1: set-option
        // cli4: change-user + set-option
        // cli1: change-user + set-option + select
        // cli4: change-user + set-option + select
        // cli4: reset-connection + set-option + select
        // cli1: change-user + set-option (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Change user", 4),
                                Pair("statement/sql/select", 5),
                                Pair("statement/sql/set_option", 5)));
      } else {
        // cli1: set-option
        // cli1: select
        // cli1: (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)));
      }
    } else {
      // cli1: select
      // cli1: (+ select)
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/select", 1)));
    }
  }
}

TEST_P(ShareConnectionTest, classic_protocol_ping_with_pool) {
  shared_router()->populate_connection_pool(GetParam());

  SCOPED_TRACE("// fill the pool with connections.");

  {
    MysqlClient cli1, cli2;

    cli1.username("root");
    cli1.password("");

    cli2.username("root");
    cli2.password("");

    ASSERT_NO_ERROR(cli1.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));

    ASSERT_NO_ERROR(cli2.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));

    // should pool
    ASSERT_NO_ERROR(cli1.ping());

    // should pool
    ASSERT_NO_ERROR(cli2.ping());
  }
}

TEST_P(ShareConnectionTest, classic_protocol_debug_with_pool) {
  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 4> clis;

  const bool can_share = GetParam().can_share();

  auto account = SharedServer::admin_account();

  for (auto [ndx, cli] : stdx::ranges::views::enumerate(clis)) {
    cli.username(account.username);
    cli.password(account.password);

    if (ndx == 3 && can_share) {
      // before the 4th connection, wait until all 3 connections are in the
      // pool.
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(3, 10s));
    }

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    ASSERT_NO_ERROR(connect_res);
  }

  // wait a bit until the connection clis[3] is moved to the pool.
  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(3, 10s));
  }

  // shared between 0 and 3
  {
    auto events_res = changed_event_counters(clis[0]);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Change user", 2),
                              Pair("statement/sql/select", 2),
                              Pair("statement/sql/set_option", 3)));
    } else {
      // no sharing possible, router is not injecting any statements.
      EXPECT_THAT(*events_res, ::testing::IsEmpty());
    }
  }

  {
    // should pool
    ASSERT_NO_ERROR(clis[0].dump_debug_info());
  }

  // shared between 0 and 3
  {
    auto events_res = changed_event_counters(clis[0]);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Change user", 2),
                              Pair("statement/com/Debug", 1),
                              Pair("statement/sql/select", 3),
                              Pair("statement/sql/set_option", 3)));
    } else {
      // no sharing possible, router is not injecting any statements.
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Debug", 1),
                                           Pair("statement/sql/select", 1)));
    }
  }
}

TEST_P(ShareConnectionTest, classic_protocol_server_status_after_command) {
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // ignore the session-state-changed as it differs if the statement comes from
  // the router or the server due to the 'statement-id' that's not generated for
  // queries from the router.
  auto status_ignore_mask = ~(SERVER_SESSION_STATE_CHANGED);

  auto show_warnings_status_mask = SERVER_STATUS_IN_TRANS |
                                   SERVER_STATUS_IN_TRANS_READONLY |
                                   SERVER_STATUS_AUTOCOMMIT;

  SCOPED_TRACE("// server status and warnings after connect()");
  {
    auto server_status_res = cli.server_status();
    ASSERT_NO_ERROR(server_status_res);

    auto server_status = *server_status_res & status_ignore_mask;
    EXPECT_EQ(server_status, SERVER_STATUS_AUTOCOMMIT)
        << std::bitset<32>(server_status);

    auto warning_count_res = cli.warning_count();
    ASSERT_NO_ERROR(warning_count_res);
    EXPECT_EQ(*warning_count_res, 0);
  }

  {
    auto warnings_res = cli.query("SHOW Warnings");
    ASSERT_NO_ERROR(warnings_res);

    for (const auto &result [[maybe_unused]] : *warnings_res) {
      // drain the warnings.
    }

    SCOPED_TRACE("// server status after SHOW WARNINGS");
    {
      auto server_status_res = cli.server_status();
      ASSERT_NO_ERROR(server_status_res);

      auto server_status = *server_status_res & status_ignore_mask;
      EXPECT_EQ(server_status, SERVER_STATUS_AUTOCOMMIT)
          << std::bitset<32>(server_status);

      auto warning_count_res = cli.warning_count();
      ASSERT_NO_ERROR(warning_count_res);
      EXPECT_EQ(*warning_count_res, 0);
    }
  }

  using TestData = std::tuple<std::string_view, int, unsigned int>;

  for (auto [stmt, expected_warning_count, expected_status_code] : {
           TestData{"DO 0/0 -- outside transaction", 1,
                    SERVER_STATUS_AUTOCOMMIT},
           TestData{"BEGIN", 0,
                    SERVER_STATUS_AUTOCOMMIT | SERVER_STATUS_IN_TRANS},
           TestData{"DO 0/0 -- in transaction", 1,
                    SERVER_STATUS_AUTOCOMMIT | SERVER_STATUS_IN_TRANS},
           TestData{"ROLLBACK", 0, SERVER_STATUS_AUTOCOMMIT},
           TestData{"SET autocommit = 0", 0, 0},
           TestData{"DO 0/0 -- after autocommit", 1, 0},
           TestData{"SELECT * FROM mysql.user", 0,
                    SERVER_STATUS_IN_TRANS | SERVER_QUERY_NO_INDEX_USED},
           TestData{"START TRANSACTION READ ONLY", 0,
                    SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY},
           TestData{"DO 0 -- after read only trans", 0,
                    SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY},
       }) {
    SCOPED_TRACE(stmt);
    {
      auto cmd_res = cli.query(stmt);
      ASSERT_NO_ERROR(cmd_res);
      for (const auto &result [[maybe_unused]] : *cmd_res) {
        // drain the warnings.
      }

      SCOPED_TRACE("// server status and warning count after query");
      {
        auto server_status_res = cli.server_status();
        ASSERT_NO_ERROR(server_status_res);

        auto server_status = *server_status_res & status_ignore_mask;
        EXPECT_EQ(server_status, expected_status_code)
            << std::bitset<32>(server_status);

        auto warning_count_res = cli.warning_count();
        ASSERT_NO_ERROR(warning_count_res);
        EXPECT_EQ(*warning_count_res, expected_warning_count);
      }

      SCOPED_TRACE("// warnings after query");
      {
        auto warnings_res = cli.query("SHOW Warnings");
        ASSERT_NO_ERROR(warnings_res);

        for (const auto &result [[maybe_unused]] : *warnings_res) {
          // drain the warnings.
        }

        SCOPED_TRACE("// server status and warning count after show warnings");
        {
          auto server_status_res = cli.server_status();
          ASSERT_NO_ERROR(server_status_res);

          auto server_status = *server_status_res & status_ignore_mask;

          // no flags outside the expected set.
          EXPECT_EQ(server_status & ~show_warnings_status_mask, 0)
              << std::bitset<32>(server_status);
          // ensure the connection's flags are remembered
          EXPECT_EQ(server_status & show_warnings_status_mask,
                    expected_status_code & show_warnings_status_mask)
              << std::bitset<32>(server_status);

          auto warning_count_res = cli.warning_count();
          ASSERT_NO_ERROR(warning_count_res);
          EXPECT_EQ(*warning_count_res, 0);
        }
      }
    }
  }
}

TEST_P(ShareConnectionTest, classic_protocol_kill_via_select) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.query("BEGIN"));

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

TEST_P(ShareConnectionTest, classic_protocol_list_dbs) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.list_dbs());
}

TEST_P(ShareConnectionTest,
       classic_protocol_change_user_caching_sha2_with_attributes_with_pool) {
  // reset auth-cache for caching-sha2-password
  for (auto admin_cli : admin_clis()) {
    SharedServer::flush_privileges(*admin_cli);
  }

  shared_router()->populate_connection_pool(GetParam());

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.set_option(MysqlClient::GetServerPublicKey(true));

  cli.username("root");
  cli.password("");

  // add one attribute that we'll find again.
  cli.set_option(MysqlClient::ConnectAttributeAdd("foo", "bar"));

  // connect
  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // check that user, schema are what is expected.
  {
    auto query_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("root@localhost",  // USER()
                                                    "<NULL>"  // SCHEMA()
                                                    )));
  }

  // check connection attributes.
  {
    auto query_res = query_one_result(cli, R"(
SELECT ATTR_NAME, ATTR_VALUE
  FROM performance_schema.session_account_connect_attrs
 WHERE PROCESSLIST_ID = CONNECTION_ID()
 ORDER BY ATTR_NAME)");
    ASSERT_NO_ERROR(query_res);

    if (GetParam().client_ssl_mode == kPassthrough) {
      // passthrough does not add _client_ip or _client_port
      EXPECT_THAT(
          *query_res,
          AllOf(IsSupersetOf({ElementsAre("_client_name", "libmysql"),
                              ElementsAre("foo", "bar")}),
                Not(Contains(ElementsAre("_client_ip", ::testing::_))),
                Not(Contains(ElementsAre("_client_port", ::testing::_)))));
    } else if (GetParam().client_ssl_mode == kDisabled) {
      // DISABLED adds _client_ip|_port, but not _client_ssl_cipher|_version
      EXPECT_THAT(
          *query_res,
          AllOf(
              IsSupersetOf({ElementsAre("_client_name", "libmysql"),
                            ElementsAre("_client_ip", "127.0.0.1"),
                            ElementsAre("foo", "bar")}),
              Contains(ElementsAre("_client_port", Not(IsEmpty()))),
              Not(Contains(ElementsAre("_client_ssl_cipher", ::testing::_)))));
    } else {
      EXPECT_THAT(*query_res,
                  AllOf(IsSupersetOf({ElementsAre("_client_name", "libmysql"),
                                      ElementsAre("_client_ip", "127.0.0.1"),
                                      ElementsAre("foo", "bar")}),
                        IsSupersetOf({
                            ElementsAre("_client_port", Not(IsEmpty())),
                            ElementsAre("_client_ssl_version", Not(IsEmpty())),
                            ElementsAre("_client_ssl_cipher", Not(IsEmpty())),
                        })));
    }
  }

  auto expect_success = !(GetParam().client_ssl_mode == kDisabled &&
                          (GetParam().server_ssl_mode == kRequired ||
                           GetParam().server_ssl_mode == kPreferred));

  // check the user of the client-connection
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

  // ... and the same checks again:
  //
  // - username
  // - schema
  // - connection attributes.
  if (expect_success) {
    {
      auto query_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
      ASSERT_NO_ERROR(query_res);

      EXPECT_THAT(*query_res, ElementsAre(ElementsAre(
                                  account.username + "@localhost", "testing")));
    }
    {
      auto query_res = query_one_result(cli, R"(
SELECT ATTR_NAME, ATTR_VALUE
  FROM performance_schema.session_account_connect_attrs
 WHERE PROCESSLIST_ID = CONNECTION_ID()
 ORDER BY ATTR_NAME
)");
      ASSERT_NO_ERROR(query_res);

      if (GetParam().client_ssl_mode == kPassthrough) {
        // passthrough does not add _client_ip or _client_port
        EXPECT_THAT(
            *query_res,
            AllOf(IsSupersetOf({ElementsAre("_client_name", "libmysql"),
                                ElementsAre("foo", "bar")}),
                  Not(Contains(ElementsAre("_client_ip", ::testing::_))),
                  Not(Contains(ElementsAre("_client_port", ::testing::_)))));
      } else if (GetParam().client_ssl_mode == kDisabled) {
        EXPECT_THAT(*query_res,
                    AllOf(IsSupersetOf({ElementsAre("_client_name", "libmysql"),
                                        ElementsAre("_client_ip", "127.0.0.1"),
                                        ElementsAre("foo", "bar")}),
                          Contains(ElementsAre("_client_port", Not(IsEmpty()))),
                          Not(Contains(ElementsAre("_client_ssl_cipher",
                                                   ::testing::_)))));
      } else {
        EXPECT_THAT(
            *query_res,
            AllOf(IsSupersetOf({ElementsAre("_client_name", "libmysql"),
                                ElementsAre("_client_ip", "127.0.0.1"),
                                ElementsAre("foo", "bar")}),
                  IsSupersetOf({
                      ElementsAre("_client_port", Not(IsEmpty())),
                      ElementsAre("_client_ssl_version", Not(IsEmpty())),
                      ElementsAre("_client_ssl_cipher", Not(IsEmpty())),
                  })));
      }
    }
  }
}

TEST_P(ShareConnectionTest, classic_protocol_statistics) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  EXPECT_NO_ERROR(cli.stat());

  EXPECT_NO_ERROR(cli.stat());
}

TEST_P(ShareConnectionTest, classic_protocol_reset_connection) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  EXPECT_NO_ERROR(cli.reset_connection());

  EXPECT_NO_ERROR(cli.reset_connection());
}

TEST_P(ShareConnectionTest, classic_protocol_query_no_result) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.query("DO 1"));
}

TEST_P(ShareConnectionTest, classic_protocol_query_with_result) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto query_res = cli.query("SELECT * FROM sys.version");
  ASSERT_NO_ERROR(query_res);
}

TEST_P(ShareConnectionTest, classic_protocol_query_call) {
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

TEST_P(ShareConnectionTest, classic_protocol_query_fail) {
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

TEST_P(ShareConnectionTest, classic_protocol_query_load_data_local_infile) {
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

TEST_P(ShareConnectionTest,
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

TEST_P(ShareConnectionTest, classic_protocol_use_schema_fail) {
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

TEST_P(ShareConnectionTest, classic_protocol_use_schema) {
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

TEST_P(ShareConnectionTest, classic_protocol_initial_schema) {
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

TEST_P(ShareConnectionTest,
       classic_protocol_initial_schema_pool_new_connection) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto query_res = query_one_result(cli, "SELECT SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("testing")));
  }

  {
    auto res = cli.use_schema("sys");
    ASSERT_NO_ERROR(res);
  }

  {
    auto query_res = query_one_result(cli, "SELECT SCHEMA()");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("sys")));
  }

  // close all connections to force a new connection.
  for (auto *admin_cli : admin_clis()) {
    ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
  }

  // check if the new connection has the same schema.
  {
    auto query_res = query_one_result(cli, "SELECT SCHEMA()");
    if (can_share) {
      ASSERT_NO_ERROR(query_res);

      EXPECT_THAT(*query_res, ElementsAre(ElementsAre("sys")));
    } else {
      // the connection wasn't in the pool and got killed.
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(), 2013) << query_res.error();
      EXPECT_THAT(query_res.error().message(),
                  ::testing::StartsWith("Lost connection to MySQL server"))
          << query_res.error();
    }
  }
}

TEST_P(ShareConnectionTest, classic_protocol_initial_schema_fail) {
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

TEST_P(ShareConnectionTest, classic_protocol_initial_schema_fail_with_pool) {
  shared_router()->populate_connection_pool(GetParam());

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

/**
 * connect
 */
TEST_P(ShareConnectionTest, classic_protocol_use_schema_pool_new_connection) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  // initial-schema is empty
  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // switch to 'sys' at runtime ... and pool
  {
    auto res = cli.use_schema("sys");
    ASSERT_NO_ERROR(res);
  }

  // reconnect, check if schema is intact.
  {
    auto query_res =
        query_one_result(cli, "SELECT SCHEMA() -- after init-schema");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("sys"  // SCHEMA()
                                                    )));
  }

  // close the pooled server-connection.
  for (auto *admin_cli : admin_clis()) {
    ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
  }

  {
    auto query_res =
        query_one_result(cli, "SELECT SCHEMA() -- after reconnect");

    if (can_share) {
      // a new connection should be opened and the schema should be carried
      // over.
      ASSERT_NO_ERROR(query_res);

      EXPECT_THAT(*query_res, ElementsAre(ElementsAre("sys"  // SCHEMA()
                                                      )));
    } else {
      // the connection wasn't in the pool and got killed.
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(), 2013) << query_res.error();
      EXPECT_THAT(query_res.error().message(),
                  ::testing::StartsWith("Lost connection to MySQL server"))
          << query_res.error();
    }
  }
}

TEST_P(ShareConnectionTest, classic_protocol_use_schema_drop_schema) {
  shared_router()->populate_connection_pool(GetParam());

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

TEST_P(ShareConnectionTest, classic_protocol_set_vars) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

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

  // reset, set_option (+ select)
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/set_option", 1)));
    }
  }

  // reset, set_option (+ select)
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

  // reset, set_option (+ select)
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/select", 3),
                              Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/select", 2),
                              Pair("statement/sql/set_option", 1)));
    }
  }
}

TEST_P(ShareConnectionTest, classic_protocol_set_uservar) {
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

TEST_P(ShareConnectionTest, classic_protocol_set_uservar_via_select) {
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
 * FR6.2: create temp-table fails, sharing not disabled.
 */
TEST_P(ShareConnectionTest, classic_protocol_temporary_table_fails_can_share) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // should fail
  ASSERT_ERROR(
      cli.query("CREATE TEMPORARY TABLE foo\n"
                "  (ID no_such_type)"));

  ASSERT_NO_ERROR(cli.query("DO 1"));

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/sql/do", 1),
                      Pair("statement/sql/error", 1),         // CREATE TABLE
                      Pair("statement/sql/select", 1),        //
                      Pair("statement/sql/set_option", 1),    //
                      Pair("statement/sql/show_warnings", 1)  // CREATE TABLE
                      ));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/do", 1),
                              Pair("statement/sql/error", 1)  // CREATE TABLE
                              ));
    }
  }
}

/**
 * FR2.2: SHOW WARNINGS
 */
TEST_P(ShareConnectionTest, classic_protocol_show_warnings_after_connect) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ::testing::IsEmpty());
  }
}

/**
 * SHOW WARNINGS
 */
TEST_P(ShareConnectionTest, classic_protocol_show_warnings) {
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
#if 0
  // LIMIT ... no number.
  {
    auto cmd_res = cli.query("SHOW WARNINGS LIMIT");
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1064);  // parse error
  }
#endif

  // no errors
  {
    auto cmd_res = query_one_result(cli, "SHOW ERRORS");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, IsEmpty());
  }
}

/**
 * SHOW WARNINGS + reset-connection.
 *
 * after a reset-connection the cached warnings should be empty.
 */
TEST_P(ShareConnectionTest, classic_protocol_show_warnings_and_reset) {
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
TEST_P(ShareConnectionTest, classic_protocol_show_warnings_and_change_user) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

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

    if (can_share) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/sql/create_table", 1),
                      Pair("statement/sql/insert_select", 1),
                      Pair("statement/sql/select", 1),
                      Pair("statement/sql/set_option", 1),    // init-trackers
                      Pair("statement/sql/show_warnings", 1)  // injected
                      ));
    } else {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/sql/create_table", 1),
                      Pair("statement/sql/insert_select", 1),
                      Pair("statement/sql/select", 2),  // SHOW COUNT(*) ...
                      Pair("statement/sql/show_errors", 1),
                      Pair("statement/sql/show_warnings", 1)));
    }
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
TEST_P(ShareConnectionTest,
       classic_protocol_show_warnings_without_server_connection) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.query("DO 0/0"));

  if (can_share) {
    // if the server-side connection is not stashed away when the
    // "close_all_connection()" is called, the close of the server-side
    // connection will also close the client-side connection.
    //
    // But for this test, we want the client connection to stay alive when the
    // server-connection goes away. -> wait until it is stashed away.
    //
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  for (auto *admin_cli : admin_clis()) {
    ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
  }

  {
    auto cmd_res = query_one_result(cli, "SHOW WARNINGS");
    if (can_share) {
      ASSERT_NO_ERROR(cmd_res);

      EXPECT_THAT(*cmd_res,
                  ElementsAre(ElementsAre("Warning", "1365", "Division by 0")));
    } else {
      // the connection wasn't in the pool and got killed.
      ASSERT_ERROR(cmd_res);
      EXPECT_EQ(cmd_res.error().value(), 2013) << cmd_res.error();
      EXPECT_THAT(cmd_res.error().message(),
                  ::testing::StartsWith("Lost connection to MySQL server"))
          << cmd_res.error();
    }
  }
}

/**
 * SHOW ERRORS
 */
TEST_P(ShareConnectionTest, classic_protocol_show_errors_after_connect) {
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

/**
 * check 'USE' via COM_QUERY changes schema and doesn't block sharing.
 */
TEST_P(ShareConnectionTest, classic_protocol_use_schema_via_query) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(cli.query("USE testing"));

  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  {
    auto cmd_res = query_one_result(cli, "SELECT USER(), SCHEMA()");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre(
                              account.username + "@localhost", "testing")));
  }
}

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

/*
 * check errors and warnings are handled correctly.
 */
TEST_P(ShareConnectionTest, classic_protocol_warnings_and_errors) {
  TRACE("start");

  const bool can_share = GetParam().can_share();
  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);

  SCOPED_TRACE("// connecting to server");

  std::vector<std::pair<std::string, std::unique_ptr<Checker>>> checkers;

  checkers.emplace_back(
      "show-warning-after-connect",
      std::make_unique<EmptyResultChecker>(
          EmptyResultChecker::test_values_type{"ShoW warnings"}));

  checkers.emplace_back(
      "show-errors-after-connect",
      std::make_unique<EmptyResultChecker>(
          EmptyResultChecker::test_values_type{"ShoW errors"}));

  checkers.emplace_back("show-warings-no-warning",
                        std::make_unique<WarningResultChecker>(
                            WarningResultChecker::test_values_type{
                                {"DO 0",
                                 {
                                     // no warnings
                                 }},
                            }));

  checkers.emplace_back("show-warnings-one-warning",
                        std::make_unique<WarningResultChecker>(
                            WarningResultChecker::test_values_type{
                                {"DO 0/0",
                                 {
                                     {"Warning", "1365", "Division by 0"},
                                 }},
                            }));

  checkers.emplace_back(
      "show-errors-one-error",
      std::make_unique<ErrorResultChecker>(ErrorResultChecker::test_values_type{
          {"DO",
           {
               {"Error", "1064",
                "You have an error in your SQL syntax; check the manual that "
                "corresponds to your MySQL server version for the right syntax "
                "to use near '' at line 1"},
           }},
      }));

  checkers.emplace_back("select-warning-count-one-warning",
                        std::make_unique<SelectWarningCountChecker>(
                            SelectWarningCountChecker::test_values_type{
                                {"DO 0/0",
                                 {
                                     {"1"},
                                 }},
                            }));

  checkers.emplace_back("select-warning-count-one-error",
                        std::make_unique<SelectWarningCountChecker>(
                            SelectWarningCountChecker::test_values_type{
                                {"DO",
                                 {
                                     {"1"},
                                 }},
                            }));

  checkers.emplace_back("select-error-count-one-warning",
                        std::make_unique<SelectErrorCountChecker>(
                            SelectErrorCountChecker::test_values_type{
                                {"DO 0/0",
                                 {
                                     {"0"},
                                 }},
                            }));

  checkers.emplace_back("select-error-count-one-error",
                        std::make_unique<SelectErrorCountChecker>(
                            SelectErrorCountChecker::test_values_type{
                                {"DO",
                                 {
                                     {"1"},
                                 }},
                            }));

  for (auto &[checker_name, checker] : checkers) {
    TRACE("running" + checker_name);

    SCOPED_TRACE("// checker: " + checker_name);
    for (const bool close_connection_before_verify : {false, true}) {
      TRACE("close connection before verify " +
            std::to_string(close_connection_before_verify));

      SCOPED_TRACE("// close-connection-before verify: " +
                   std::to_string(close_connection_before_verify));

      for (auto *admin_cli : admin_clis()) {
        ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
      }

      TRACE("closed all connections");
      SCOPED_TRACE("// close-connection-before verify: " +
                   std::to_string(close_connection_before_verify));

      MysqlClient cli;

      auto account = SharedServer::caching_sha2_empty_password_account();

      cli.username(account.username);
      cli.password(account.password);

      ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                  shared_router()->port(GetParam())));

      TRACE("connected");

      ASSERT_NO_FATAL_FAILURE(checker->apply(cli));

      TRACE("checked");

      if (can_share && can_fetch_password) {
        ASSERT_NO_ERROR(
            shared_router()->wait_for_stashed_server_connections(1, 10s));
      }

      if (close_connection_before_verify) {
        for (auto *admin_cli : admin_clis()) {
          ASSERT_NO_ERROR(SharedServer::close_all_connections(*admin_cli));
        }
      }

      if (can_share && can_fetch_password) {
        ASSERT_NO_FATAL_FAILURE(checker->verifier()(cli));
      }

      TRACE("verified");
    }
  }
}

/*
 * quoted warning-count: SELECT @@`warning_count`;
 */
TEST_P(ShareConnectionTest, classic_protocol_select_warning_count_quoted) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("DO");  // syntax error
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1064) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                ::testing::StartsWith("You have an error in your SQL"))
        << cmd_res.error();
  }

  {
    auto cmd_res = query_one_result(cli, "select @@`warning_count`");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("1")));
  }
}

/*
 * quoted error-count: SELECT @@`error_count`;
 */
TEST_P(ShareConnectionTest, classic_protocol_select_error_count_quoted) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("DO");  // syntax error
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1064) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                ::testing::StartsWith("You have an error in your SQL"))
        << cmd_res.error();
  }

  {
    auto cmd_res = query_one_result(cli, "select @@`error_count`");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, ElementsAre(ElementsAre("1")));
  }
}

/**
 * FRx.x: disabling session-trackers fails.
 */
TEST_P(ShareConnectionTest, classic_protocol_set_session_trackers) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  std::vector<std::string> set_stmts{
      "Set session_track_gtids = OFF",
      "set autocommit = 0, session_track_gtids = 0",
      "set session session_track_gtids = 'off'",
      "set @@session.sEssION_track_gtids = 'off'",
      "set local session_track_gtids = 'off'",
      "set @@LOCAL.session_track_gtids= 0",
      "set session_track_transaction_info = 0",
      "set session_track_state_change = 0",
      "set session_track_system_variables = ''"};

  // SET session-trackers MUST fail
  for (auto const &stmt : set_stmts) {
    SCOPED_TRACE("// " + stmt + " should fail");
    auto cmd_res = cli.query(stmt);
    if (can_share) {
      ASSERT_ERROR(cmd_res);
      EXPECT_EQ(cmd_res.error().value(), 1766);
      EXPECT_THAT(cmd_res.error().message(), StartsWith("The system variable"));
    } else {
      ASSERT_NO_ERROR(cmd_res);
    }
  }

  // inside a Transaction too.
  {
    auto cmd_res = cli.query("START TRANSACTION");
    ASSERT_NO_ERROR(cmd_res);
  }

  for (auto const &stmt : set_stmts) {
    SCOPED_TRACE("// " + stmt + " should fail");
    auto cmd_res = cli.query(stmt);
    if (can_share || (stmt.find("_gtids") != stmt.npos)) {
      ASSERT_ERROR(cmd_res);
      EXPECT_EQ(cmd_res.error().value(), 1766);
      // "The system variable ... cannot be set when there is an ongoing
      // transaction."
      EXPECT_THAT(cmd_res.error().message(), StartsWith("The system variable"));
    } else {
      ASSERT_NO_ERROR(cmd_res);
    }
  }

  {
    auto cmd_res = cli.query("ROLLBACK");
    ASSERT_NO_ERROR(cmd_res);
  }
}

/**
 * FR3.5: SET NAMES should work with connnection-sharing.
 */
TEST_P(ShareConnectionTest, classic_protocol_set_names) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // set-trackers,
  // select,
  // set-names
  {
    auto cmd_res = cli.query("SET NAMES 'utf8mb4'");
    ASSERT_NO_ERROR(cmd_res);
  }

  // select
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

  // ... after ...
  // select
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/select", 3),
                              Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 1)));
    }
  }
}

/**
 * FR5.2: LOCK TABLES
 */
TEST_P(ShareConnectionTest, classic_protocol_lock_tables_and_reset) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));
  // set-trackers

  {
    auto query_res = cli.query("CREATE TABLE testing.tbl (ID INT)");
    ASSERT_NO_ERROR(query_res);
  }  // stashed

  {
    auto cmd_res = cli.query("LOCK TABLES testing.tbl READ");
    ASSERT_NO_ERROR(cmd_res);
  }  // LOCK TABLES disables sharing.

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

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/create_table", 1),
                              Pair("statement/sql/lock_tables", 1),
                              Pair("statement/sql/select", 3),
                              Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/create_table", 1),
                              Pair("statement/sql/lock_tables", 1),
                              Pair("statement/sql/select", 1)));
    }
  }

  {
    auto cmd_res = query_one_result(cli, "SELECT * FROM testing.tbl");
    ASSERT_NO_ERROR(cmd_res);

    EXPECT_THAT(*cmd_res, IsEmpty());
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/create_table", 1),
                              Pair("statement/sql/lock_tables", 1),
                              Pair("statement/sql/select", 5),
                              Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/create_table", 1),
                              Pair("statement/sql/lock_tables", 1),
                              Pair("statement/sql/select", 3)));
    }
  }

  // cleanup
  {
    auto query_res = cli.query("DROP TABLE testing.tbl");
    ASSERT_NO_ERROR(query_res);
  }
}

/**
 * FR6.1: GET_LOCK(), no-share until reset
 */
TEST_P(ShareConnectionTest, classic_protocol_get_lock) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(cli.query("DO GET_LOCK('abc', 0)"));

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/do", 1),         // DO ...()
                              Pair("statement/sql/select", 1),     // at connect
                              Pair("statement/sql/set_option", 1)  // at connect
                              ));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/do", 1)  // DO ...()
                              ));
    }
  }

  ASSERT_NO_ERROR(cli.reset_connection());

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // explicit
                                   Pair("statement/com/Reset Connection", 1),
                                   // DO GET_LOCK()
                                   Pair("statement/sql/do", 1),
                                   // events
                                   Pair("statement/sql/select", 3),
                                   // connect, explicit
                                   Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // explicit
                                   Pair("statement/com/Reset Connection", 1),
                                   // DO GET_LOCK()
                                   Pair("statement/sql/do", 1),
                                   // events
                                   Pair("statement/sql/select", 1)));
    }
  }
}

/**
 * FR6.1: GET_LOCK(), no-share until, in transaction.
 *
 * GET_LOCK() should taint the session and even block sharing
 * when called outside a transaction.
 */
TEST_P(ShareConnectionTest, classic_protocol_get_lock_in_transaction) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("START TRANSACTION");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto cmd_res = cli.query("DO GET_LOCK('lock1', 0)");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto cmd_res = cli.query("ROLLBACK");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/sql/begin", 1),      // START TRANSACTION
                      Pair("statement/sql/do", 1),         // DO ...()
                      Pair("statement/sql/rollback", 1),   // ROLLBACK
                      Pair("statement/sql/select", 1),     // at connect
                      Pair("statement/sql/set_option", 1)  // at connect
                      ));
    } else {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/sql/begin", 1),    // START TRANSACTION
                      Pair("statement/sql/do", 1),       // DO ...()
                      Pair("statement/sql/rollback", 1)  // ROLLBACK
                      ));
    }
  }

  ASSERT_NO_ERROR(cli.reset_connection());

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // explicit
                                   Pair("statement/com/Reset Connection", 1),
                                   // START TRANSACTION
                                   Pair("statement/sql/begin", 1),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // ROLLBACK
                                   Pair("statement/sql/rollback", 1),
                                   // events
                                   Pair("statement/sql/select", 3),
                                   // connect, explicit
                                   Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // explicit
                                   Pair("statement/com/Reset Connection", 1),
                                   // START TRANSACTION
                                   Pair("statement/sql/begin", 1),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // ROLLBACK
                                   Pair("statement/sql/rollback", 1),
                                   // events
                                   Pair("statement/sql/select", 1)));
    }
  }
}

/**
 * FR6.1: SERVICE_GET_WRITE_LOCKS(), no-share until reset
 */
TEST_P(ShareConnectionTest, classic_protocol_service_get_write_locks) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("DO SERVICE_GET_WRITE_LOCKS('ns', 'lock1', 0)");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/do", 1),         // DO ...()
                              Pair("statement/sql/select", 1),     // connect
                              Pair("statement/sql/set_option", 1)  // connect
                              ));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/do", 1)));
    }
  }

  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res,
                ElementsAre(ElementsAre("LOCKING SERVICE", "ns", "lock1",
                                        "EXCLUSIVE", "GRANTED")));
  }

  ASSERT_NO_ERROR(cli.reset_connection());

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // explicit
                                   Pair("statement/com/Reset Connection", 1),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // events, metadata-locks
                                   Pair("statement/sql/select", 4),
                                   // connect, explicit
                                   Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // explicit
                                   Pair("statement/com/Reset Connection", 1),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // events, metadata-locks
                                   Pair("statement/sql/select", 2)));
    }
  }

  // reset-connection should clear the locks.
  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ::testing::IsEmpty());
  }
}

/**
 * FR6.1: SERVICE_GET_WRITE_LOCKS(), no-share until, in transaction.
 *
 * SERVICE_GET_WRITE_LOCKS() should taint the session and even block sharing
 * when called outside a transaction.
 */
TEST_P(ShareConnectionTest,
       classic_protocol_service_get_write_locks_in_transaction) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("START TRANSACTION");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto cmd_res = cli.query("DO SERVICE_GET_WRITE_LOCKS('ns', 'lock1', 0)");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto cmd_res = cli.query("ROLLBACK");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/sql/begin", 1),      // START TRANSACTION
                      Pair("statement/sql/do", 1),         // DO ...()
                      Pair("statement/sql/rollback", 1),   // ROLLBACK
                      Pair("statement/sql/select", 1),     //
                      Pair("statement/sql/set_option", 1)  // connect
                      ));
    } else {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/sql/begin", 1),    // START TRANSACTION
                      Pair("statement/sql/do", 1),       // DO ...()
                      Pair("statement/sql/rollback", 1)  // ROLLBACK
                      ));
    }
  }

  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res,
                ElementsAre(ElementsAre("LOCKING SERVICE", "ns", "lock1",
                                        "EXCLUSIVE", "GRANTED")));
  }

  ASSERT_NO_ERROR(cli.reset_connection());

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // explicit
                                   Pair("statement/com/Reset Connection", 1),
                                   // START TRANSACTION
                                   Pair("statement/sql/begin", 1),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // ROLLBACK
                                   Pair("statement/sql/rollback", 1),
                                   // events, metadata-locks
                                   Pair("statement/sql/select", 4),
                                   // connect, explicit
                                   Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // from-pool
                                   Pair("statement/com/Reset Connection", 1),
                                   // START TRANSACTION
                                   Pair("statement/sql/begin", 1),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // ROLLBACK
                                   Pair("statement/sql/rollback", 1),
                                   // events, metadata-locks
                                   Pair("statement/sql/select", 2)));
    }
  }

  // reset-connection should clear the locks.
  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ::testing::IsEmpty());
  }
}

/**
 * FR6.1: SERVICE_GET_READ_LOCKS(), no-share until reset
 */
TEST_P(ShareConnectionTest, classic_protocol_service_get_read_locks) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// check if connection is available for sharing");
  {
    if (can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    } else {
      auto stashed_res = shared_router()->stashed_server_connections();
      ASSERT_NO_ERROR(stashed_res);
      EXPECT_EQ(*stashed_res, 0);
    }
  }

  SCOPED_TRACE("// DO SERVICE_GET_READ_LOCKS()");
  {
    auto cmd_res = cli.query("DO SERVICE_GET_READ_LOCKS('ns', 'lock1', 0)");
    ASSERT_NO_ERROR(cmd_res);
  }

  SCOPED_TRACE("// check if connection is available for sharing");
  {
    auto stashed_res = shared_router()->stashed_server_connections();
    ASSERT_NO_ERROR(stashed_res);
    // the connection should NOT be stashed as SERVICE_GET_READ_LOCKS blocks
    // sharing.
    EXPECT_EQ(*stashed_res, 0);
  }

  SCOPED_TRACE("// check if locks are in place");
  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res,
                ElementsAre(ElementsAre("LOCKING SERVICE", "ns", "lock1",
                                        "SHARED", "GRANTED")));
  }

  {
    auto stashed_res = shared_router()->stashed_server_connections();
    ASSERT_NO_ERROR(stashed_res);
    // the connection should NOT be stashed as SERVICE_GET_READ_LOCKS blocks
    // sharing.
    EXPECT_EQ(*stashed_res, 0);
  }

  SCOPED_TRACE("// reset the connection to remove the locks");
  ASSERT_NO_ERROR(cli.reset_connection());

  SCOPED_TRACE("// check if connection is available for sharing");
  if (can_share) {
    // wait a bit for the connection to be stashed.
    //
    // after reset_connection finished for the client, the router may still
    // initialize the session-trackers.
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  } else {
    auto stashed_res = shared_router()->stashed_server_connections();
    ASSERT_NO_ERROR(stashed_res);
    EXPECT_EQ(*stashed_res, 0);
  }

  SCOPED_TRACE("// reset-connection should clear the locks.");
  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ::testing::IsEmpty());
  }
}

/**
 * FR6.1: SERVICE_GET_READ_LOCKS(), no-share until, in transaction.
 *
 * SERVICE_GET_READ_LOCKS() should taint the session and even block sharing
 * when called outside a transaction.
 */
TEST_P(ShareConnectionTest,
       classic_protocol_service_get_read_locks_in_transaction) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// check if connection is available for sharing");
  {
    if (can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    } else {
      auto stashed_res = shared_router()->stashed_server_connections();
      ASSERT_NO_ERROR(stashed_res);
      EXPECT_EQ(*stashed_res, 0);
    }
  }

  SCOPED_TRACE("// start transaction.");
  {
    auto cmd_res = cli.query("START TRANSACTION");
    ASSERT_NO_ERROR(cmd_res);
  }

  SCOPED_TRACE("// check if connection is available for sharing");
  {
    if (can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    } else {
      auto stashed_res = shared_router()->stashed_server_connections();
      ASSERT_NO_ERROR(stashed_res);
      EXPECT_EQ(*stashed_res, 0);
    }
  }

  SCOPED_TRACE("// get a read-lock");
  {
    auto cmd_res = cli.query("DO SERVICE_GET_READ_LOCKS('ns', 'lock1', 0)");
    ASSERT_NO_ERROR(cmd_res);
  }

  SCOPED_TRACE("// check if connection is not available for sharing");
  {
    auto stashed_res = shared_router()->stashed_server_connections();
    ASSERT_NO_ERROR(stashed_res);
    EXPECT_EQ(*stashed_res, 0);
  }

  SCOPED_TRACE("// ROLLBACK to clear transaction state.");
  {
    auto cmd_res = cli.query("ROLLBACK");
    ASSERT_NO_ERROR(cmd_res);
  }

  SCOPED_TRACE("// check if connection is still not available for sharing");
  {
    auto stashed_res = shared_router()->stashed_server_connections();
    ASSERT_NO_ERROR(stashed_res);
    EXPECT_EQ(*stashed_res, 0);
  }

  SCOPED_TRACE("// check if locks are in place");
  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res,
                ElementsAre(ElementsAre("LOCKING SERVICE", "ns", "lock1",
                                        "SHARED", "GRANTED")));
  }

  SCOPED_TRACE("// reset the connection to remove the locks");
  ASSERT_NO_ERROR(cli.reset_connection());

  SCOPED_TRACE("// check if connection is available for sharing");
  if (can_share) {
    // wait a bit for the connection to be stashed.
    //
    // after reset_connection finished for the client, the router may still
    // initialize the session-trackers.
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  } else {
    auto stashed_res = shared_router()->stashed_server_connections();
    ASSERT_NO_ERROR(stashed_res);
    EXPECT_EQ(*stashed_res, 0);
  }

  SCOPED_TRACE("// reset-connection should clear the locks.");
  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ::testing::IsEmpty());
  }
}

/**
 * FR6.1: VERSION_TOKENS_LOCK_SHARED(), no-share until reset
 */
TEST_P(ShareConnectionTest, classic_protocol_version_tokens_lock_shared) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  SCOPED_TRACE("// connect");
  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// check if connection is available for sharing");
  {
    if (can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    } else {
      auto stashed_res = shared_router()->stashed_server_connections();
      ASSERT_NO_ERROR(stashed_res);
      EXPECT_EQ(*stashed_res, 0);
    }
  }

  SCOPED_TRACE("// get a version-token");
  {
    auto cmd_res = cli.query("DO VERSION_TOKENS_LOCK_SHARED('token1', 0)");
    ASSERT_NO_ERROR(cmd_res);
  }

  SCOPED_TRACE("// check if connection is not available for sharing");
  {
    auto stashed_res = shared_router()->stashed_server_connections();
    ASSERT_NO_ERROR(stashed_res);
    EXPECT_EQ(*stashed_res, 0);
  }

  SCOPED_TRACE("// check if locks are in place");
  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(
                                "LOCKING SERVICE", "version_token_locks",
                                "token1", "SHARED", "GRANTED")));
  }

  SCOPED_TRACE("// reset the connection to remove the locks");
  ASSERT_NO_ERROR(cli.reset_connection());

  if (can_share) {
    // wait a bit for the connection to be stashed.
    //
    // after reset_connection finished for the client, the router may still
    // initialize the session-trackers.
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  } else {
    auto stashed_res = shared_router()->stashed_server_connections();
    ASSERT_NO_ERROR(stashed_res);
    EXPECT_EQ(*stashed_res, 0);
  }

  SCOPED_TRACE("// reset-connection should clear the locks.");
  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ::testing::IsEmpty());
  }
}

/**
 * FR6.1: VERSION_TOKENS_LOCK_EXCLUSIVE(), no-share until reset
 */
TEST_P(ShareConnectionTest, classic_protocol_version_tokens_lock_exclusive) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// check if connection is available for sharing");
  {
    if (can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    } else {
      auto stashed_res = shared_router()->stashed_server_connections();
      ASSERT_NO_ERROR(stashed_res);
      EXPECT_EQ(*stashed_res, 0);
    }
  }

  SCOPED_TRACE("// get a lock");
  {
    auto cmd_res = cli.query("DO VERSION_TOKENS_LOCK_EXCLUSIVE('token1', 0)");
    ASSERT_NO_ERROR(cmd_res);
  }

  SCOPED_TRACE("// check if connection is not available for sharing");
  {
    auto stashed_res = shared_router()->stashed_server_connections();
    ASSERT_NO_ERROR(stashed_res);
    EXPECT_EQ(*stashed_res, 0);
  }

  SCOPED_TRACE("// check if locks are in place");
  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(
                                "LOCKING SERVICE", "version_token_locks",
                                "token1", "EXCLUSIVE", "GRANTED")));
  }

  SCOPED_TRACE("// reset the connection to remove the locks");
  ASSERT_NO_ERROR(cli.reset_connection());

  SCOPED_TRACE("// check if connection is available for sharing");
  if (can_share) {
    // wait a bit for the connection to be stashed.
    //
    // after reset_connection finished for the client, the router may still
    // initialize the session-trackers.
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  } else {
    auto stashed_res = shared_router()->stashed_server_connections();
    ASSERT_NO_ERROR(stashed_res);
    EXPECT_EQ(*stashed_res, 0);
  }

  SCOPED_TRACE("// reset-connection should clear the locks.");
  {
    auto query_res =
        query_one_result(cli,
                         "SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,\n"
                         "       LOCK_TYPE, LOCK_STATUS\n"
                         "  FROM performance_schema.metadata_locks\n"
                         " WHERE OBJECT_TYPE = 'LOCKING SERVICE'");
    ASSERT_NO_ERROR(query_res);

    EXPECT_THAT(*query_res, ::testing::IsEmpty());
  }
}

TEST_P(ShareConnectionTest, classic_protocol_prepare_fail) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connect");
  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// check if connection is available for sharing");
  {
    if (can_share) {
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    } else {
      auto stashed_res = shared_router()->stashed_server_connections();
      ASSERT_NO_ERROR(stashed_res);
      EXPECT_EQ(*stashed_res, 0);
    }
  }

  SCOPED_TRACE("// prepare a broken SQL statement.");
  {
    auto res = cli.prepare("SEL ?");
    ASSERT_ERROR(res);
    EXPECT_EQ(res.error().value(), 1064) << res.error();  // Syntax Error
  }

  SCOPED_TRACE("// check if connection is available for sharing");
  {
    if (can_share) {
      // wait for the connection to be stashed.
      //
      // The connection may not be stashed after prepare() returns as the router
      // injects a SHOW WARNINGS before stashing.
      ASSERT_NO_ERROR(
          shared_router()->wait_for_stashed_server_connections(1, 10s));
    } else {
      auto stashed_res = shared_router()->stashed_server_connections();
      ASSERT_NO_ERROR(stashed_res);
      EXPECT_EQ(*stashed_res, 0);
    }
  }
}

/**
 * FR6.3: successful prepared statement: disable sharing until reset-connection
 */
TEST_P(ShareConnectionTest, classic_protocol_prepare_execute) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.prepare("SELECT ?");
  ASSERT_NO_ERROR(res);

  auto stmt = std::move(res.value());

  std::array<MYSQL_BIND, 1> params{
      NullParam{},
  };
  ASSERT_NO_ERROR(stmt.bind_params(params));

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

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Execute", 1),
                              Pair("statement/com/Prepare", 1),
                              // connect
                              Pair("statement/sql/select", 1),     //
                              Pair("statement/sql/set_option", 1)  //
                              ));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Execute", 1),
                                           Pair("statement/com/Prepare", 1)));
    }
  }

  ASSERT_NO_ERROR(cli.reset_connection());

  // share again.
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Execute", 1),
                              Pair("statement/com/Prepare", 1),
                              // events
                              Pair("statement/com/Reset Connection", 1),
                              // events
                              Pair("statement/sql/select", 3),
                              Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Execute", 1),
                              Pair("statement/com/Prepare", 1),
                              // explicit
                              Pair("statement/com/Reset Connection", 1),
                              // events
                              Pair("statement/sql/select", 1)));
    }
  }
}

TEST_P(ShareConnectionTest, classic_protocol_prepare_execute_fetch) {
  const bool can_share = GetParam().can_share();

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

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Execute", 1),
                              Pair("statement/com/Fetch", 2),
                              Pair("statement/com/Prepare", 1),
                              Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 1)));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Execute", 1),
                                           Pair("statement/com/Fetch", 2),
                                           Pair("statement/com/Prepare", 1)));
    }
  }
}

TEST_P(ShareConnectionTest, classic_protocol_prepare_append_data_execute) {
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
    EXPECT_TRUE(exec_res) << exec_res.error();
  }
}

TEST_P(ShareConnectionTest,
       classic_protocol_prepare_append_data_reset_execute) {
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
TEST_P(ShareConnectionTest, classic_protocol_prepare_execute_no_result) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

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

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Execute", 1),
                              Pair("statement/com/Prepare", 1),
                              Pair("statement/sql/select", 1),
                              // connect
                              Pair("statement/sql/set_option", 1)  //
                              ));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Execute", 1),
                                           Pair("statement/com/Prepare", 1)));
    }
  }

  SCOPED_TRACE("// reset the connection to allow sharing again.");
  ASSERT_NO_ERROR(cli.reset_connection());

  // share again.
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Execute", 1),
                              Pair("statement/com/Prepare", 1),
                              // events
                              Pair("statement/com/Reset Connection", 1),
                              // events
                              Pair("statement/sql/select", 3),
                              Pair("statement/sql/set_option", 2)));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Execute", 1),
                              Pair("statement/com/Prepare", 1),
                              // explicit
                              Pair("statement/com/Reset Connection", 1),
                              // events
                              Pair("statement/sql/select", 1)));
    }
  }
}

/*
 * stmt-execute -> stored-procedure
 */
TEST_P(ShareConnectionTest, classic_protocol_prepare_execute_call) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

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

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Execute", 1),
                              Pair("statement/com/Prepare", 1),
                              Pair("statement/sp/stmt", 2),
                              Pair("statement/sql/select", 1),
                              // connect
                              Pair("statement/sql/set_option", 1)  //
                              ));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Execute", 1),
                                           Pair("statement/com/Prepare", 1),
                                           Pair("statement/sp/stmt", 2)));
    }
  }

  SCOPED_TRACE("// reset the connection to allow sharing again.");
  ASSERT_NO_ERROR(cli.reset_connection());

  // share again.
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Execute", 1),
                              Pair("statement/com/Prepare", 1),
                              // from-pool, events
                              Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sp/stmt", 2),
                              // events
                              Pair("statement/sql/select", 3),
                              Pair("statement/sql/set_option", 2)));
    } else {
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
}

/*
 * com-stmt-reset -> error
 *
 * COM_STMT_RESET fails for unknown stmt-ids
 */
TEST_P(ShareConnectionTest, classic_protocol_stmt_reset_fail) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  // disable SSL as raw packets will be sent.
  cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));

  auto connect_res =
      cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
  if (GetParam().client_ssl_mode == kRequired) {
    ASSERT_ERROR(connect_res);
    GTEST_SKIP() << connect_res.error();
  }
  ASSERT_NO_ERROR(connect_res);

  // don't share the connection.
  ASSERT_NO_ERROR(cli.query("SET @block_this_connection = 1"));

  // send a stmt-reset with a unknown stmt-id
  std::vector<uint8_t> buf;

  // caps for the error-packet parser
  auto caps = classic_protocol::capabilities::protocol_41;
  {
    auto encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
        classic_protocol::message::client::StmtReset>>(
        {0, {0}}, caps, net::dynamic_buffer(buf));
    ASSERT_NO_ERROR(encode_res);

    auto send_res =
        net::impl::socket::send(cli.native_handle(), buf.data(), buf.size(), 0);
    ASSERT_NO_ERROR(send_res);
    EXPECT_EQ(*send_res, buf.size());
  }

  // recv the error-msg
  {
    buf.resize(1024);  // should be large enough.

    auto recv_res =
        net::impl::socket::recv(cli.native_handle(), buf.data(), buf.size(), 0);
    ASSERT_NO_ERROR(recv_res);

    buf.resize(*recv_res);

    ASSERT_GT(buf.size(), 5) << mysql_harness::hexify(buf);
    ASSERT_EQ(buf[4], 0xff) << mysql_harness::hexify(buf);

    auto decode_res = classic_protocol::decode<classic_protocol::frame::Frame<
        classic_protocol::message::server::Error>>(net::buffer(buf), caps);
    ASSERT_NO_ERROR(decode_res);

    auto decoded = std::move(*decode_res);
    auto frame = decoded.second;

    auto msg = frame.payload();

    // unknown prepared statement
    EXPECT_EQ(msg.error_code(), 1243);
  }
}

/*
 * com-register-replica -> error
 */
TEST_P(ShareConnectionTest, classic_protocol_register_replica_fail) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();
  cli.username(account.username);
  cli.password(account.password);

  // disable SSL as raw packets will be sent.
  cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));

  {
    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (GetParam().client_ssl_mode == kRequired) {
      ASSERT_ERROR(connect_res);
      GTEST_SKIP() << connect_res.error();
    }
    ASSERT_NO_ERROR(connect_res);
  }

  ASSERT_NO_ERROR(cli.query("SET @block_this_connection = 1"));

  std::vector<uint8_t> buf;

  // caps for the error-packet parser
  auto caps = classic_protocol::capabilities::protocol_41;
  {
    auto encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
        classic_protocol::message::client::RegisterReplica>>(
        {0, {0, "", "", "", 0, 0, 0}}, caps, net::dynamic_buffer(buf));
    ASSERT_NO_ERROR(encode_res);

    auto send_res =
        net::impl::socket::send(cli.native_handle(), buf.data(), buf.size(), 0);
    ASSERT_NO_ERROR(send_res);
    EXPECT_EQ(*send_res, buf.size());
  }

  {
    buf.resize(1024);  // should be large enough.

    auto recv_res =
        net::impl::socket::recv(cli.native_handle(), buf.data(), buf.size(), 0);
    ASSERT_NO_ERROR(recv_res);

    buf.resize(*recv_res);

    ASSERT_GT(buf.size(), 5) << mysql_harness::hexify(buf);
    ASSERT_EQ(buf[4], 0xff) << mysql_harness::hexify(buf);

    auto decode_res = classic_protocol::decode<classic_protocol::frame::Frame<
        classic_protocol::message::server::Error>>(net::buffer(buf), caps);
    ASSERT_NO_ERROR(decode_res);

    auto decoded = std::move(*decode_res);
    auto frame = decoded.second;

    auto msg = frame.payload();

    // Access Denied for native_empty ...
    EXPECT_EQ(msg.error_code(), 1045) << msg.message();
  }
}

/*
 * com-register-replica -> no-connection
 */
TEST_P(ShareConnectionTest, classic_protocol_register_replica_no_connection) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();
  cli.username(account.username);
  cli.password(account.password);

  // disable SSL as raw packets will be sent.
  cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));

  {
    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (GetParam().client_ssl_mode == kRequired) {
      ASSERT_ERROR(connect_res);
      GTEST_SKIP() << connect_res.error();
    }
    ASSERT_NO_ERROR(connect_res);
  }

  std::vector<uint8_t> buf;

  // caps for the error-packet parser
  auto caps = classic_protocol::capabilities::protocol_41;
  {
    auto encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
        classic_protocol::message::client::RegisterReplica>>(
        {0, {0, "", "", "", 0, 0, 0}}, caps, net::dynamic_buffer(buf));
    ASSERT_NO_ERROR(encode_res);

    auto send_res =
        net::impl::socket::send(cli.native_handle(), buf.data(), buf.size(), 0);
    ASSERT_NO_ERROR(send_res);
    EXPECT_EQ(*send_res, buf.size());
  }

  {
    buf.resize(1024);  // should be large enough.

    auto recv_res =
        net::impl::socket::recv(cli.native_handle(), buf.data(), buf.size(), 0);
    ASSERT_NO_ERROR(recv_res);

    buf.resize(*recv_res);

    ASSERT_GT(buf.size(), 5) << mysql_harness::hexify(buf);
    ASSERT_EQ(buf[4], 0xff) << mysql_harness::hexify(buf);

    auto decode_res = classic_protocol::decode<classic_protocol::frame::Frame<
        classic_protocol::message::server::Error>>(net::buffer(buf), caps);
    ASSERT_NO_ERROR(decode_res);

    auto decoded = std::move(*decode_res);
    auto frame = decoded.second;

    auto msg = frame.payload();

    // Access Denied for native_empty ...
    EXPECT_EQ(msg.error_code(), 1045) << msg.message();
  }
}

/*
 * com-set-option -> no-connection
 */
TEST_P(ShareConnectionTest, classic_protocol_set_option_no_connection) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();
  cli.username(account.username);
  cli.password(account.password);

  // disable SSL as raw packets will be sent.
  cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
  {
    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (GetParam().client_ssl_mode == kRequired) {
      ASSERT_ERROR(connect_res);
      GTEST_SKIP() << connect_res.error();
    }
    ASSERT_NO_ERROR(connect_res);
  }

  std::vector<uint8_t> buf;

  // caps for the error-packet parser
  auto caps = classic_protocol::capabilities::protocol_41;
  {
    auto encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
        classic_protocol::message::client::SetOption>>(
        {0, {255}}, caps, net::dynamic_buffer(buf));
    ASSERT_NO_ERROR(encode_res);

    auto send_res =
        net::impl::socket::send(cli.native_handle(), buf.data(), buf.size(), 0);
    ASSERT_NO_ERROR(send_res);
    EXPECT_EQ(*send_res, buf.size());
  }

  {
    buf.resize(1024);  // should be large enough.

    auto recv_res =
        net::impl::socket::recv(cli.native_handle(), buf.data(), buf.size(), 0);
    ASSERT_NO_ERROR(recv_res);

    buf.resize(*recv_res);

    ASSERT_GT(buf.size(), 5) << mysql_harness::hexify(buf);
    ASSERT_EQ(buf[4], 0xff) << mysql_harness::hexify(buf);

    auto decode_res = classic_protocol::decode<classic_protocol::frame::Frame<
        classic_protocol::message::server::Error>>(net::buffer(buf), caps);
    ASSERT_NO_ERROR(decode_res);

    auto decoded = std::move(*decode_res);
    auto frame = decoded.second;

    auto msg = frame.payload();

    // unknown command
    EXPECT_EQ(msg.error_code(), 1047) << msg.message();
  }
}

TEST_P(ShareConnectionTest,
       classic_protocol_prepare_execute_missing_bind_param) {
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

TEST_P(ShareConnectionTest, classic_protocol_prepare_reset) {
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

TEST_P(ShareConnectionTest, classic_protocol_set_option) {
  RecordProperty("Description",
                 "check if enabling multi-statement at runtime is handled "
                 "and sharing is allowed.");

  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));

  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  {
    auto query_res = cli.query("DO 1; DO 2");
    ASSERT_ERROR(query_res);
  }

  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  ASSERT_NO_ERROR(cli.set_server_option(MYSQL_OPTION_MULTI_STATEMENTS_ON));

  {
    auto query_res = cli.query("DO 1; DO 2");
    if (can_share) {
      ASSERT_ERROR(query_res);

      // multi-statements are forbidden when connection-sharing is enabled.
      EXPECT_EQ(query_res.error().value(), 4501);
    } else {
      ASSERT_NO_ERROR(query_res);

      for (const auto &res [[maybe_unused]] : *query_res) {
      }
    }
  }

  {
    // a single statement is ok though.
    auto query_res = cli.query("DO 1");
    ASSERT_NO_ERROR(query_res);
  }

  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  EXPECT_NO_ERROR(cli.set_server_option(MYSQL_OPTION_MULTI_STATEMENTS_OFF));

  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  {
    auto query_res = cli.query("DO 1; DO 2");
    ASSERT_ERROR(query_res);
  }
}

TEST_P(ShareConnectionTest, classic_protocol_set_option_at_connect) {
  RecordProperty("Description",
                 "check if the multi-statement flag is handled at handshake "
                 "when sharing is allowed.");

  SCOPED_TRACE("// ensure the pool is empty");
  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));

  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.flags(CLIENT_MULTI_STATEMENTS);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  {
    auto query_res = cli.query("DO 1; DO 2");
    if (can_share) {
      ASSERT_ERROR(query_res);

      // multi-statements are forbidden when connection-sharing is enabled.
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

  ASSERT_NO_ERROR(cli.set_server_option(MYSQL_OPTION_MULTI_STATEMENTS_ON));

  {
    auto query_res = cli.query("DO 1; DO 2");
    if (can_share) {
      ASSERT_ERROR(query_res);

      // multi-statements are forbidden when connection-sharing is enabled.
      EXPECT_EQ(query_res.error().value(), 4501);
    } else {
      ASSERT_NO_ERROR(query_res);

      for (const auto &res [[maybe_unused]] : *query_res) {
      }
    }
  }

  {
    // a single statement is ok though.
    auto query_res = cli.query("DO 1");
    ASSERT_NO_ERROR(query_res);
  }

  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  EXPECT_NO_ERROR(cli.set_server_option(MYSQL_OPTION_MULTI_STATEMENTS_OFF));

  if (can_share) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  {
    auto query_res = cli.query("DO 1; DO 2");
    ASSERT_ERROR(query_res);
  }
}

TEST_P(ShareConnectionTest, classic_protocol_set_option_fails) {
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

TEST_P(ShareConnectionTest, classic_protocol_binlog_dump) {
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

TEST_P(ShareConnectionTest, classic_protocol_binlog_dump_fail_no_checksum) {
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
                  AnyOf(StartsWith("Slave can not handle"),
                        StartsWith("Replica can not handle")))
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
TEST_P(ShareConnectionTest, classic_protocol_binlog_dump_gtid) {
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
TEST_P(ShareConnectionTest,
       classic_protocol_binlog_dump_gtid_fail_no_checksum) {
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
                  AnyOf(StartsWith("Slave can not handle"),
                        StartsWith("Replica can not handle")))
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

TEST_P(ShareConnectionTest,
       classic_protocol_binlog_dump_gtid_fail_wrong_position) {
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

//
// caching_sha2_password
//

TEST_P(ShareConnectionTest, classic_protocol_caching_sha2_password_with_pass) {
  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset auth-cache for caching-sha2-password
  }

  auto account = SharedServer::caching_sha2_password_account();

  std::string username(account.username);
  std::string password(account.password);

  {
    SCOPED_TRACE("// user exists, with pass");
    MysqlClient cli;

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

  {
    SCOPED_TRACE("// user exists, with pass, but wrong-pass");
    MysqlClient cli;

    cli.username(username);
    cli.password(wrong_password_);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    ASSERT_ERROR(connect_res);

    if (GetParam().client_ssl_mode == kDisabled) {
      EXPECT_EQ(connect_res.error().value(), 2061) << connect_res.error();
      // Authentication plugin 'caching_sha2_password' reported error:
      // Authentication requires secure connection.
    } else {
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      // "Access denied for user ..."
    }
  }

  {
    SCOPED_TRACE("// user exists, with pass, but wrong-empty-pass");
    MysqlClient cli;

    cli.username(username);
    cli.password(empty_password_);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    ASSERT_ERROR(connect_res);
    EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    // "Access denied for user ..."
  }
}

TEST_P(ShareConnectionTest, classic_protocol_caching_sha2_password_no_pass) {
  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset auth-cache for caching-sha2-password
  }

  auto account = SharedServer::caching_sha2_empty_password_account();

  {
    SCOPED_TRACE("// user exists, with pass");
    MysqlClient cli;

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));
  }

  {
    SCOPED_TRACE("// user exists, with pass, but wrong-pass");
    MysqlClient cli;

    cli.username(account.username);
    cli.password(wrong_password_);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    ASSERT_ERROR(connect_res);
    if (GetParam().client_ssl_mode == kDisabled) {
      EXPECT_EQ(connect_res.error().value(), 2061) << connect_res.error();
      // Authentication plugin 'caching_sha2_password' reported error:
      // Authentication requires secure connection.
    } else {
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      // "Access denied for user ..."
    }
  }

  // should reuse connection.
  {
    SCOPED_TRACE("// user exists, with pass");
    MysqlClient cli;

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));
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
TEST_P(ShareConnectionTest,
       classic_protocol_caching_sha2_over_plaintext_with_pass) {
  if (GetParam().client_ssl_mode == kRequired) {
    GTEST_SKIP() << "test requires plaintext connection.";
  }

  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset auth-cache for caching-sha2-password
  }

  auto account = SharedServer::caching_sha2_single_use_password_account();

  std::string username(account.username);
  std::string password(account.password);

  for (auto *admin_cli : admin_clis()) {
    SharedServer::create_account(*admin_cli, account);
  }

  // remove the account at the end of the test again.
  Scope_guard drop_at_end([account]() {
    for (auto *admin_cli : admin_clis()) {
      SharedServer::drop_account(*admin_cli, account);
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

TEST_P(ShareConnectionTest, classic_protocol_sha256_password_no_pass) {
  auto account = SharedServer::sha256_empty_password_account();

  std::string username(account.username);
  std::string password(account.password);

  {
    SCOPED_TRACE("// user exists, with pass");
    MysqlClient cli;

    cli.username(username);
    cli.password(password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));
  }

  {
    SCOPED_TRACE("// user exists, with pass, but wrong-pass");
    MysqlClient cli;

    cli.username(username);
    cli.password(wrong_password_);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    ASSERT_ERROR(connect_res);
    EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    // "Access denied for user ..."
  }

  // should reuse connection.
  {
    SCOPED_TRACE("// user exists, with pass, reuse");
    MysqlClient cli;

    cli.username(username);
    cli.password(password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));
  }
}

TEST_P(ShareConnectionTest, classic_protocol_sha256_password_with_pass) {
  auto account = SharedServer::sha256_password_account();

  std::string username(account.username);
  std::string password(account.password);

  {
    SCOPED_TRACE("// user exists, with pass");
    MysqlClient cli;

    cli.username(username);
    cli.password(password);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (GetParam().client_ssl_mode == kDisabled &&
        (GetParam().server_ssl_mode == kPreferred ||
         GetParam().server_ssl_mode == kRequired)) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      // Access denied for user '...'@'localhost' (using password: YES)
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  }

  {
    SCOPED_TRACE("// user exists, with pass, but wrong-pass");
    MysqlClient cli;

    cli.username(username);
    cli.password(wrong_password_);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    ASSERT_ERROR(connect_res);

    EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    // "Access denied for user ..."
  }

  {
    SCOPED_TRACE("// user exists, with pass, but wrong-empty-pass");
    MysqlClient cli;

    cli.username(username);
    cli.password(empty_password_);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    ASSERT_ERROR(connect_res);
    EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
    // "Access denied for user ..."
  }

  // should reuse connection.
  {
    SCOPED_TRACE("// user exists, with pass, reuse");
    MysqlClient cli;

    cli.username(username);
    cli.password(password);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (GetParam().client_ssl_mode == kDisabled &&
        (GetParam().server_ssl_mode == kPreferred ||
         GetParam().server_ssl_mode == kRequired)) {
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
      // Access denied for user '...'@'localhost' (using password: YES)
    } else {
      ASSERT_NO_ERROR(connect_res);
    }
  }
}

/**
 * Check, sha256-password over plaintext works with get-server-key.
 */
TEST_P(ShareConnectionTest,
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

  SCOPED_TRACE("// reuse");
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
    ShareConnectionTest,
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

  SCOPED_TRACE("// reuse");
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
    ShareConnectionTest,
    classic_protocol_caching_sha2_password_over_plaintext_with_get_server_key) {
  if (GetParam().client_ssl_mode == kRequired) {
    GTEST_SKIP() << "test requires plaintext connection.";
  }

  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset auth-cache for caching-sha2-password
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

  SCOPED_TRACE("// reuse");
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
    ShareConnectionTest,
    classic_protocol_caching_sha2_password_over_plaintext_with_get_server_key_with_pool) {
  if (GetParam().client_ssl_mode == kRequired) {
    GTEST_SKIP() << "test requires plaintext connection.";
  }

  for (auto &srv : shared_servers()) {
    srv->flush_privileges();  // reset auth-cache for caching-sha2-password
  }

  shared_router()->populate_connection_pool(GetParam());

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
      // - client will ask for server's public-key but the server will treat the
      // request as "password is 0x02" and fail.
      ASSERT_ERROR(connect_res);
    } else {
      ASSERT_NO_ERROR(connect_res);

      ASSERT_NO_ERROR(cli.ping());
    }
  }

  SCOPED_TRACE("// reuse");
  {
    MysqlClient cli;
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));
    cli.set_option(MysqlClient::GetServerPublicKey(true));

    cli.username(username);
    cli.password(password);

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (!expect_success) {
      ASSERT_ERROR(connect_res);
    } else {
      ASSERT_NO_ERROR(connect_res);

      ASSERT_NO_ERROR(cli.ping());
    }
  }
}

/**
 * Check, empty caching-sha2-password over plaintext works with get-server-key.
 */
TEST_P(
    ShareConnectionTest,
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

  SCOPED_TRACE("// reuse");
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
TEST_P(ShareConnectionTest, classic_protocol_unknown_command) {
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

TEST_P(ShareConnectionTest, classic_protocol_charset_after_connect) {
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

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

TEST_P(ShareConnectionTest, php_caching_sha2_password_empty_pass) {
  auto account = SharedServer::caching_sha2_empty_password_account();

  auto php = find_executable_path("php");
  if (php.empty()) GTEST_SKIP() << "php not found in $PATH";

  auto &proc =
      spawner(php)
          .wait_for_sync_point(Spawner::SyncPoint::NONE)
          .spawn({
              "-f",
              get_data_dir().join("routing_sharing_php_query.php").str(),
              shared_router()->host(),                            //
              std::to_string(shared_router()->port(GetParam())),  //
              account.username,
              account.password,
              std::to_string(GetParam().client_ssl_mode == kRequired),
              std::to_string(GetParam().can_share()),
          });

  proc.wait_for_exit();
}

TEST_P(ShareConnectionTest, php_caching_sha2_password_pass) {
  auto account = SharedServer::caching_sha2_password_account();

  if (GetParam().client_ssl_mode == kDisabled &&
      (GetParam().server_ssl_mode == kRequired ||
       GetParam().server_ssl_mode == kPreferred)) {
    // skip it as it is expected to fail to auth.
    return;
  }

  // clean the shared privileges to force a full auth.
  for (const auto &srv : shared_servers()) {
    srv->flush_privileges();
  }

  auto php = find_executable_path("php");
  if (php.empty()) GTEST_SKIP() << "php not found in $PATH";

  auto &proc =
      spawner(php)
          .wait_for_sync_point(Spawner::SyncPoint::NONE)
          .spawn({
              "-f",
              get_data_dir().join("routing_sharing_php_query.php").str(),
              shared_router()->host(),                            //
              std::to_string(shared_router()->port(GetParam())),  //
              account.username,
              account.password,
              std::to_string(GetParam().client_ssl_mode == kRequired),
              std::to_string(GetParam().can_share()),
          });

  proc.wait_for_exit();
}

TEST_P(ShareConnectionTest, php_prepared_statement) {
  auto account = SharedServer::caching_sha2_empty_password_account();

  auto php = find_executable_path("php");
  if (php.empty()) GTEST_SKIP() << "php not found in $PATH";

  auto &proc = spawner(php)
                   .wait_for_sync_point(Spawner::SyncPoint::NONE)
                   .spawn({
                       "-f",
                       get_data_dir()
                           .join("routing_sharing_php_prepared_statement.php")
                           .str(),
                       shared_router()->host(),                            //
                       std::to_string(shared_router()->port(GetParam())),  //
                       account.username,
                       account.password,
                       std::to_string(GetParam().client_ssl_mode != kDisabled),
                       std::to_string(GetParam().can_share()),
                   });

  proc.wait_for_exit();
}

TEST_P(ShareConnectionTest, php_all_commands) {
  auto account = SharedServer::admin_account();

  auto php = find_executable_path("php");
  if (php.empty()) GTEST_SKIP() << "php not found in $PATH";

  auto &proc =
      spawner(php)
          .wait_for_sync_point(Spawner::SyncPoint::NONE)
          .spawn({
              "-f",
              get_data_dir().join("routing_sharing_php_all_commands.php").str(),
              shared_router()->host(),                            //
              std::to_string(shared_router()->port(GetParam())),  //
              account.username,
              account.password,
              std::to_string(GetParam().client_ssl_mode != kDisabled),
              std::to_string(GetParam().can_share()),
          });

  proc.wait_for_exit();
}

TEST_P(ShareConnectionTest, select_overlong) {
  RecordProperty(
      "Description",
      "Check if overlong statements are properly tokenized and forwarded.");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // send a statement that's longer than 16Mbyte which spans multiple protocol
  // frames.
  {
    auto query_res =
        query_one_result(cli, "SET /* " + std::string(16 * 1024 * 1024, 'a') +
                                  " */ GLOBAL wait_timeout = 1");
    ASSERT_ERROR(query_res);
    // should fail with "Access denied; need SUPER|SYSTEM_VARIABLES_ADMIN
    EXPECT_EQ(query_res.error().value(), 1227) << query_res.error();
  }

  // a safe guard that the recv-buffers are proper cleaned
  ASSERT_NO_ERROR(cli.query("DO 1"));
}

TEST_P(ShareConnectionTest, aborted_lexing) {
  RecordProperty("Description",
                 "Check that lexing a statement with a non-closed comment "
                 "fails properly.");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto query_res = cli.query("DO 1 /*");
  ASSERT_ERROR(query_res);

  // parse-error at /*
  EXPECT_EQ(query_res.error().value(), 1064) << query_res.error();
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

class ShareConnectionReconnectTest
    : public ShareConnectionTestBase,
      public ::testing::WithParamInterface<ShareConnectionParam> {
 public:
  void SetUp() override {
    SharedServer::Account account{
        "onetime",
        "",  // no password.
        "caching_sha2_password",
    };

    for (auto *srv : shared_servers()) {
      auto cli_res = srv->admin_cli();
      ASSERT_NO_ERROR(cli_res);

      auto cli = std::move(*cli_res);

      ASSERT_NO_ERROR(cli.query("DROP USER IF EXISTS " + account.username));

      SharedServer::create_account(cli, account);
      SharedServer::grant_access(cli, account, "SELECT", "testing");
    }

    const bool can_share = GetParam().can_share();

    for (auto [ndx, cli] : stdx::views::enumerate(clis_)) {
      SCOPED_TRACE("// connection [" + std::to_string(ndx) + "]");

      cli.username(account.username);
      cli.password(account.password);
      cli.set_option(MysqlClient::GetServerPublicKey(true));

      ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                  shared_router()->port(GetParam())));

      // wait until the connection is in the pool.
      if (can_share) {
        size_t expected_pooled_connections = ndx < 3 ? ndx + 1 : 3;

        ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(
            expected_pooled_connections, 10s));
      }
    }

    SCOPED_TRACE(
        "// change the password of the 'onetime' user to force a reauth fail.");
    for (auto *srv : shared_servers()) {
      auto cli_res = srv->admin_cli();
      ASSERT_NO_ERROR(cli_res);

      auto cli = std::move(*cli_res);

      ASSERT_NO_ERROR(cli.query("ALTER USER " + account.username +
                                " IDENTIFIED BY 'someotherpass'"));
    }
  }

  void TearDown() override {
    for (auto &cli : clis_) cli.close();
  }

 protected:
  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 4> clis_;
};

TEST_P(ShareConnectionReconnectTest, ping) {
  const bool can_share = GetParam().can_share();

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
  const bool can_share = GetParam().can_share();

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
  const bool can_share = GetParam().can_share();

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
  const bool can_share = GetParam().can_share();

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
  const bool can_share = GetParam().can_share();

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
  const bool can_share = GetParam().can_share();

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
  const bool can_share = GetParam().can_share();

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
  SCOPED_TRACE("// check if a changed password has handled properly.");

  auto &cli = clis_[0];
  const auto cmd_res = cli.change_user("onetime", "someotherpass", "");
  if (GetParam().client_ssl_mode == kDisabled &&
      (GetParam().server_ssl_mode == kRequired ||
       GetParam().server_ssl_mode == kPreferred)) {
    // caching-sha2-password needs a secure-channel on the client side too if
    // the server side is secure (Required/Preferred)
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1045) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                testing::HasSubstr("while reauthenticating"));
  } else {
    ASSERT_NO_ERROR(cmd_res);
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionReconnectTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
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
          bool, ShareConnectionParam, ChangeUserParam, std::string>> {
 public:
  void SetUp() override {
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

 protected:
  static std::unique_ptr<MysqlClient> cli_;
  static bool last_with_ssl_;
  static ShareConnectionParam last_connect_param_;
  static int expected_change_user_;
  static int expected_reset_connection_;
  static int expected_select_;
  static int expected_set_option_;
};

std::unique_ptr<MysqlClient> ChangeUserTest::cli_{};
bool ChangeUserTest::last_with_ssl_{};
ShareConnectionParam ChangeUserTest::last_connect_param_{};
int ChangeUserTest::expected_change_user_{0};
int ChangeUserTest::expected_reset_connection_{0};
int ChangeUserTest::expected_select_{0};
int ChangeUserTest::expected_set_option_{0};

TEST_P(ChangeUserTest, classic_protocol) {
  auto [with_ssl, connect_param, test_param, schema] = GetParam();

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
       last_connect_param_.server_ssl_mode != connect_param.server_ssl_mode)) {
    cli_.reset();
  }

  if (!cli_) {
    // flush the pool to ensure the test can for "wait_for_pooled_connection(1)"
    for (auto &srv : shared_servers()) {
      srv->close_all_connections();  // reset the router's connection-pool
    }

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

    ASSERT_NO_ERROR(cli_->connect(shared_router()->host(),
                                  shared_router()->port(connect_param)));

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
    for (auto &srv : shared_servers()) {
      srv->flush_privileges();
    }
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

    if (!account.password.empty() &&
        (account.auth_method == "caching_sha2_password" ||
         account.auth_method == "sha256_password") &&
        connect_param.client_ssl_mode == kDisabled &&
        (connect_param.server_ssl_mode == kPreferred ||
         connect_param.server_ssl_mode == kRequired)) {
      // client will ask for the public-key, but router doesn't have a
      // public key (as client_ssl_mode is DISABLED and server is SSL and
      // therefore doesn't have public-key either).
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
    ::testing::Combine(::testing::Bool(),
                       ::testing::ValuesIn(share_connection_params),
                       ::testing::ValuesIn(change_user_params),
                       ::testing::Values("", "testing")),
    [](auto &info) {
      auto schema = std::get<3>(info.param);
      return "with" + std::string(std::get<0>(info.param) ? "" : "out") +
             "_ssl__via_" + std::get<1>(info.param).testname + "_" +
             std::get<2>(info.param).scenario +
             (schema.empty() ? "_without_schema"s : ("_with_schema_" + schema));
    });

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
          std::tuple<StatementSharableParam, ShareConnectionParam>> {
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
  auto [test_param, connect_param] = GetParam();

  auto account = SharedServer::caching_sha2_empty_password_account();

  MysqlClient cli;

  cli.set_option(MysqlClient::GetServerPublicKey(true));
  cli.username(account.username);
  cli.password(account.password);

  auto connect_res = cli.connect(shared_router()->host(),
                                 shared_router()->port(connect_param));
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

         expected_stmts.emplace_back(
             Event::sql_prepare_sql("PREPARE `stmt` FROM ?"));
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
                       ::testing::ValuesIn(share_connection_params)),
    [](auto &info) {
      return std::get<0>(info.param).test_name + "_via_" +
             std::get<1>(info.param).testname;
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
