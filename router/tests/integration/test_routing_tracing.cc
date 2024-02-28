/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#define RAPIDJSON_HAS_STDSTRING 1

#include "my_rapidjson_size_t.h"

#include <rapidjson/error/en.h>
#include <rapidjson/pointer.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "hexify.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/stdx/filesystem.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysql/harness/tls_context.h"
#include "mysql/harness/utility/string.h"  // join
#include "mysqld_error.h"
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

using ::testing::ElementsAre;
using ::testing::SizeIs;

static constexpr const auto kIdleServerConnectionsSleepTime{10ms};

static constexpr const std::string_view kDisabled{"DISABLED"};
static constexpr const std::string_view kRequired{"REQUIRED"};
static constexpr const std::string_view kPreferred{"PREFERRED"};
static constexpr const std::string_view kPassthrough{"PASSTHROUGH"};
static constexpr const std::string_view kAsClient{"AS_CLIENT"};

static constexpr const std::string_view kErRouterTrace("4600");

static const auto show_warnings_status_mask = SERVER_STATUS_IN_TRANS |
                                              SERVER_STATUS_IN_TRANS_READONLY |
                                              SERVER_STATUS_AUTOCOMMIT;

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

  auto results = *cmd_res;

  auto res_it = results.begin();
  if (res_it == results.end()) {
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

struct ConnectionParam {
  std::string testname;

  std::string_view client_ssl_mode;
  std::string_view server_ssl_mode;

  [[nodiscard]] bool can_trace() const {
    return !(client_ssl_mode == kPassthrough ||
             (client_ssl_mode == kPreferred && server_ssl_mode == kAsClient));
  }
};

const ConnectionParam connection_params[] = {
    // DISABLED
    {
        "DISABLED__DISABLED",
        kDisabled,  // client_ssl_mode
        kDisabled,  // server_ssl_mode
    },
#ifdef WITH_REDUNDANT_COMBINATIONS
    {
        "DISABLED__AS_CLIENT",
        kDisabled,
        kAsClient,
    },
#endif
    {
        "DISABLED__REQUIRED",
        kDisabled,
        kRequired,
    },
#ifdef WITH_REDUNDANT_COMBINATIONS
    {
        "DISABLED__PREFERRED",
        kDisabled,
        kPreferred,
    },
#endif

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
#ifdef WITH_REDUNDANT_COMBINATIONS
    {
        "PREFERRED__REQUIRED",
        kPreferred,
        kRequired,
    },
#endif

    // REQUIRED ...
    {
        "REQUIRED__DISABLED",
        kRequired,
        kDisabled,
    },
#ifdef WITH_REDUNDANT_COMBINATIONS
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
#endif
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
                                 {"port", std::to_string(rest_port_)}})
        .section("connection_pool", {
                                        {"max_idle_server_connections", "1"},
                                    });

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
            {"protocol", "classic"},  //
            {"routing_strategy", "round-robin"},

            {"client_ssl_mode", std::string(param.client_ssl_mode)},
            {"server_ssl_mode", std::string(param.server_ssl_mode)},

            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
            {"connection_sharing", "1"},        //
            {"connection_sharing_delay", "0"},  //
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
        return stdx::unexpected(make_error_code(std::errc::invalid_argument));
      }
      return v->GetInt();
    } else {
      std::cerr << json_doc << "\n";

      return stdx::unexpected(
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
      if (!int_res) return stdx::unexpected(int_res.error());

      if (*int_res == expected_value) return {};

      if (clock_type::now() > end_time) {
        return stdx::unexpected(make_error_code(std::errc::timed_out));
      }

      std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
    } while (true);
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

class TracingTestBase : public RouterComponentTest {
 public:
  static constexpr const size_t kNumServers = 1;

  static void SetUpTestSuite() {
    for (const auto &s : shared_servers()) {
      if (s->mysqld_failed_to_start()) GTEST_SKIP();
    }

    TestWithSharedRouter::SetUpTestSuite(test_env->port_pool(),
                                         shared_servers());

    schema_doc_ = std::make_unique<rapidjson::Document>();
    schema_doc_->Parse(schema_json.data(), schema_json.size());
    ASSERT_FALSE(schema_doc_->HasParseError())
        << rapidjson::GetParseError_En(schema_doc_->GetParseError()) << " at "
        << schema_doc_->GetErrorOffset() << " near\n"
        << schema_json.substr(schema_doc_->GetErrorOffset());
  }

  static void TearDownTestSuite() {
    schema_doc_.reset();
    TestWithSharedRouter::TearDownTestSuite();
  }

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
      } else {
        // s->flush_privileges();  // reset the auth-cache
      }
    }
  }

  ~TracingTestBase() override {
    if (::testing::Test::HasFailure()) {
      shared_router()->process_manager().dump_logs();
    }
  }

  MYSQL_BIND zero_getter(enum_field_types type) {
    MYSQL_BIND bnd{};
    bnd.buffer_type = type;

    switch (type) {
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
      case MYSQL_TYPE_JSON:
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_GEOMETRY:
        bnd.buffer = str_zero_.data();
        bnd.buffer_length = static_cast<unsigned long>(str_zero_.size());
        return bnd;
      case MYSQL_TYPE_TINY:
        bnd.buffer = &tiny_zero_;
        return bnd;
      case MYSQL_TYPE_SHORT:
        bnd.buffer = &short_zero_;
        return bnd;
      case MYSQL_TYPE_LONG:
        bnd.buffer = &long_zero_;
        return bnd;
      case MYSQL_TYPE_LONGLONG:
        bnd.buffer = &longlong_zero_;
        return bnd;
      case MYSQL_TYPE_FLOAT:
        bnd.buffer = &float_zero_;
        return bnd;
      case MYSQL_TYPE_DOUBLE:
        bnd.buffer = &double_zero_;
        return bnd;
      case MYSQL_TYPE_NULL:
        bnd.buffer = nullptr;
        return bnd;
      case MYSQL_TYPE_TIMESTAMP:
        bnd.buffer = &time_zero_;
        return bnd;
      default:
        abort();
    }
  }

  MYSQL_BIND one_getter(enum_field_types type) {
    MYSQL_BIND bnd{};
    bnd.buffer_type = type;

    switch (type) {
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
      case MYSQL_TYPE_JSON:
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_GEOMETRY:
        bnd.buffer = str_one_.data();
        bnd.buffer_length = static_cast<unsigned long>(str_one_.size());
        return bnd;
      case MYSQL_TYPE_TINY:
        bnd.buffer = &tiny_one_;
        return bnd;
      case MYSQL_TYPE_SHORT:
        bnd.buffer = &short_one_;
        return bnd;
      case MYSQL_TYPE_LONG:
        bnd.buffer = &long_one_;
        return bnd;
      case MYSQL_TYPE_LONGLONG:
        bnd.buffer = &longlong_one_;
        return bnd;
      case MYSQL_TYPE_FLOAT:
        bnd.buffer = &float_one_;
        return bnd;
      case MYSQL_TYPE_DOUBLE:
        bnd.buffer = &double_one_;
        return bnd;
      case MYSQL_TYPE_NULL:
        bnd.buffer = nullptr;
        return bnd;
      case MYSQL_TYPE_TIMESTAMP:
        bnd.buffer = &time_one_;
        return bnd;
      default:
        abort();
    }
  }

  MYSQL_BIND two_getter(enum_field_types type) {
    MYSQL_BIND bnd{};
    bnd.buffer_type = type;

    switch (type) {
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
      case MYSQL_TYPE_JSON:
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_GEOMETRY:
        bnd.buffer = str_two_.data();
        bnd.buffer_length = static_cast<unsigned long>(str_two_.size());
        return bnd;
      case MYSQL_TYPE_TINY:
        bnd.buffer = &tiny_two_;
        return bnd;
      case MYSQL_TYPE_SHORT:
        bnd.buffer = &short_two_;
        return bnd;
      case MYSQL_TYPE_LONG:
        bnd.buffer = &long_two_;
        return bnd;
      case MYSQL_TYPE_LONGLONG:
        bnd.buffer = &longlong_two_;
        return bnd;
      case MYSQL_TYPE_FLOAT:
        bnd.buffer = &float_two_;
        return bnd;
      case MYSQL_TYPE_DOUBLE:
        bnd.buffer = &double_two_;
        return bnd;
      case MYSQL_TYPE_NULL:
        bnd.buffer = nullptr;
        return bnd;
      case MYSQL_TYPE_TIMESTAMP:
        bnd.buffer = &time_two_;
        return bnd;
      default:
        abort();
    }
  }

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

  static void assert_warnings_with_trace(
      MysqlClient &cli, bool expected_sharing_is_blocked = false) {
    auto trace_res = get_trace(cli);
    ASSERT_TRUE(trace_res);

    assert_sharing_blocked(*trace_res, expected_sharing_is_blocked);
  }

  static void assert_warnings_no_trace(MysqlClient &cli) {
    auto warnings_res = query_one_result(cli, "SHOW warnings");
    ASSERT_NO_ERROR(warnings_res);

    EXPECT_THAT(*warnings_res, ::testing::Not(::testing::Contains(ElementsAre(
                                   "Note", kErRouterTrace, ::testing::_))));
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

  static testing::AssertionResult sharing_blocked_eq(
      rapidjson::Document &doc, bool expected_sharing_blocked) {
    return json_pointer_eq(
        doc, rapidjson::Pointer("/attributes/mysql.sharing_blocked"),
        rapidjson::Value(expected_sharing_blocked));
  }

  static void assert_sharing_blocked(std::string_view json_trace,
                                     bool expected_sharing_blocked) {
    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());
    ASSERT_TRUE(trace_is_valid(doc)) << json_trace;

    ASSERT_TRUE(sharing_blocked_eq(doc, expected_sharing_blocked))
        << json_trace;
  }

 private:
  std::array<char, 2> str_zero_{'0'};
  int8_t tiny_zero_{};
  int16_t short_zero_{};
  int32_t long_zero_{};
  int64_t longlong_zero_{};
  float float_zero_{};
  double double_zero_{};
  MYSQL_TIME time_zero_{};

  std::array<char, 2> str_one_{'1'};
  int8_t tiny_one_{1};
  int16_t short_one_{1};
  int32_t long_one_{1};
  int64_t longlong_one_{1};
  float float_one_{1};
  double double_one_{1};
  MYSQL_TIME time_one_{
      /* .year = */ 2022,
      /* .month = */ 12,
      /* .day = */ 1,
      /* .hour = */ 1,
      /* .minute = */ 2,
      /* .second = */ 3,
      /* .second_part = */ 4,
      /* .neg = */ false,
      /* .time_type = */ MYSQL_TIMESTAMP_TIME,
      /* .time_zone_displacement = */ 1,
  };

  std::array<char, 2> str_two_{'2'};
  int8_t tiny_two_{2};
  int16_t short_two_{2};
  int32_t long_two_{2};
  int64_t longlong_two_{2};
  float float_two_{2};
  double double_two_{2};
  MYSQL_TIME time_two_{
      /* .year = */ 2022,
      /* .month = */ 12,
      /* .day = */ 2,
      /* .hour = */ 1,
      /* .minute = */ 2,
      /* .second = */ 3,
      /* .second_part = */ 4,
      /* .neg = */ false,
      /* .time_type = */ MYSQL_TIMESTAMP_TIME,
      /* .time_zone_displacement = */ 1,
  };

 protected:
  static constexpr const std::string_view valid_ssl_key_{
      SSL_TEST_DATA_DIR "/server-key-sha512.pem"};
  static constexpr const std::string_view valid_ssl_cert_{
      SSL_TEST_DATA_DIR "/server-cert-sha512.pem"};

  static constexpr const std::string_view wrong_password_{"wrong_password"};
  static constexpr const std::string_view empty_password_{};

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

  static std::unique_ptr<rapidjson::Document> schema_doc_;
};

std::unique_ptr<rapidjson::Document> TracingTestBase::schema_doc_;

struct TracingCommandParam {
  struct Env {
    bool expected_is_connected;
    bool expected_sharing_is_blocked;
    bool trace_enabled;
  };

  std::string_view test_name;

  bool sharing_blocked_after_test;
  bool needs_super_privs;

  std::function<void(const ConnectionParam &connect_param, MysqlClient &, Env)>
      test_func;
};

const TracingCommandParam tracing_command_params[] = {
    {"query_ok", false, false,
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       auto cmd_res = cli.query("DO 1");
       ASSERT_NO_ERROR(cmd_res);

       auto warning_count_res = cli.warning_count();
       ASSERT_NO_ERROR(warning_count_res);

       auto num_warnings_res = query_one_result(cli, "SHOW COUNT(*) WARNINGS");
       ASSERT_NO_ERROR(num_warnings_res);

       auto num_warnings = *num_warnings_res;

       if (can_trace && env.trace_enabled) {
         EXPECT_EQ(*warning_count_res, 1);  // the trace
         EXPECT_THAT(num_warnings, ElementsAre(ElementsAre("1")));

         auto trace_res = TracingTestBase::get_trace(cli);
         ASSERT_TRUE(trace_res);

         auto json_trace = *trace_res;

         rapidjson::Document doc;
         doc.Parse(json_trace.data(), json_trace.size());
         ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

         for (const auto &[pntr, val] : {
                  std::pair{"/name", rapidjson::Value("mysql/query")},
                  std::pair{"/attributes/mysql.sharing_blocked",
                            rapidjson::Value(env.expected_sharing_is_blocked)},
                  std::pair{"/events/0/name",
                            rapidjson::Value("mysql/query_classify")},
                  std::pair{"/events/0/attributes/mysql.query.classification",
                            rapidjson::Value("accept_session_state_from_"
                                             "session_tracker,read-only")},
                  std::pair{"/events/1/name",
                            rapidjson::Value("mysql/connect_and_forward")},
                  std::pair{"/events/1/attributes/mysql.remote.is_connected",
                            rapidjson::Value(env.expected_is_connected)},
              }) {
           ASSERT_TRUE(TracingTestBase::json_pointer_eq(
               doc, rapidjson::Pointer(pntr), val))
               << json_trace;
         }
       } else {
         EXPECT_EQ(*warning_count_res, 0);
         EXPECT_THAT(num_warnings, ElementsAre(ElementsAre("0")));

         TracingTestBase::assert_warnings_no_trace(cli);
       }
     }},
    {"query_error", false, false,
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       auto cmd_res = cli.query("ERROR 1");
       ASSERT_ERROR(cmd_res);
       EXPECT_EQ(cmd_res.error().value(), 1064);

       auto warning_count_res = cli.warning_count();
       ASSERT_NO_ERROR(warning_count_res);

       if (can_trace && env.trace_enabled) {
         auto trace_res = TracingTestBase::get_trace(cli);
         ASSERT_TRUE(trace_res);

         auto json_trace = *trace_res;

         rapidjson::Document doc;
         doc.Parse(json_trace.data(), json_trace.size());
         ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

         for (const auto &[pntr, val] : {
                  std::pair{"/name", rapidjson::Value("mysql/query")},
                  std::pair{"/status_code", rapidjson::Value("ERROR")},
                  std::pair{"/attributes/mysql.sharing_blocked",
                            rapidjson::Value(env.expected_sharing_is_blocked)},
                  std::pair{"/events/0/name",
                            rapidjson::Value("mysql/query_classify")},
                  std::pair{"/events/0/attributes/mysql.query.classification",
                            rapidjson::Value(
                                "accept_session_state_from_session_tracker")},
                  std::pair{"/events/1/name",
                            rapidjson::Value("mysql/connect_and_forward")},
                  std::pair{"/events/1/attributes/mysql.remote.is_connected",
                            rapidjson::Value(env.expected_is_connected)},
              }) {
           ASSERT_TRUE(TracingTestBase::json_pointer_eq(
               doc, rapidjson::Pointer(pntr), val))
               << json_trace;
         }
       } else {
         EXPECT_EQ(*warning_count_res, 0);

         TracingTestBase::assert_warnings_no_trace(cli);
       }
     }},
    {"ping_ok", false, false,
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       auto cmd_res = cli.ping();
       ASSERT_NO_ERROR(cmd_res);

       auto warning_count_res = cli.warning_count();
       ASSERT_NO_ERROR(warning_count_res);

       if (can_trace && env.trace_enabled) {
         EXPECT_EQ(*warning_count_res, 1);  // the trace

         auto trace_res = TracingTestBase::get_trace(cli);
         ASSERT_TRUE(trace_res);

         auto json_trace = *trace_res;

         rapidjson::Document doc;
         doc.Parse(json_trace.data(), json_trace.size());
         ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

         for (const auto &[pntr, val] : {
                  std::pair{"/name", rapidjson::Value("mysql/ping")},
                  std::pair{"/attributes/mysql.sharing_blocked",
                            rapidjson::Value(env.expected_sharing_is_blocked)},
                  std::pair{"/events/0/name",
                            rapidjson::Value("mysql/connect_and_forward")},
                  std::pair{"/events/0/attributes/mysql.remote.is_connected",
                            rapidjson::Value(env.expected_is_connected)},
              }) {
           ASSERT_TRUE(TracingTestBase::json_pointer_eq(
               doc, rapidjson::Pointer(pntr), val))
               << json_trace;
         }
       } else {
         EXPECT_EQ(*warning_count_res, 0);

         TracingTestBase::assert_warnings_no_trace(cli);
       }
     }},
    {"stmt_prepare_fail", false, false,
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       SCOPED_TRACE("// - prepare");
       auto cmd_res = cli.prepare("ERROR 1");
       ASSERT_ERROR(cmd_res);
       EXPECT_EQ(cmd_res.error().value(), 1064);

       auto warning_count_res = cli.warning_count();
       ASSERT_NO_ERROR(warning_count_res);

       if (can_trace && env.trace_enabled) {
         EXPECT_EQ(*warning_count_res, 0);

         auto trace_res = TracingTestBase::get_trace(cli);
         ASSERT_TRUE(trace_res);

         auto json_trace = *trace_res;

         rapidjson::Document doc;
         doc.Parse(json_trace.data(), json_trace.size());
         ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

         for (const auto &[pntr, val] : {
                  std::pair{"/name", rapidjson::Value("mysql/stmt_prepare")},
                  std::pair{"/status_code", rapidjson::Value("ERROR")},
                  std::pair{"/attributes/mysql.sharing_blocked",
                            rapidjson::Value(env.expected_sharing_is_blocked)},
                  std::pair{"/events/0/name",
                            rapidjson::Value("mysql/connect_and_forward")},
                  std::pair{"/events/0/attributes/mysql.remote.is_connected",
                            rapidjson::Value(env.expected_is_connected)},
              }) {
           ASSERT_TRUE(TracingTestBase::json_pointer_eq(
               doc, rapidjson::Pointer(pntr), val))
               << json_trace;
         }
       } else {
         EXPECT_EQ(*warning_count_res, 0);

         TracingTestBase::assert_warnings_no_trace(cli);
       }
     }},
    {"stmt_prepare_ok",  //
     true,               // blocks sharing after test
     false,              //
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       SCOPED_TRACE("// - prepare");
       auto cmd_res = cli.prepare("DO 1");
       ASSERT_NO_ERROR(cmd_res);
       {
         auto warning_count_res = cli.warning_count();
         ASSERT_NO_ERROR(warning_count_res);

         auto num_warnings_res =
             query_one_result(cli, "SHOW COUNT(*) WARNINGS");
         ASSERT_NO_ERROR(num_warnings_res);

         auto num_warnings = *num_warnings_res;

         if (can_trace && env.trace_enabled) {
           EXPECT_EQ(*warning_count_res, 1);
           EXPECT_THAT(num_warnings, ElementsAre(ElementsAre("1")));

           auto trace_res = TracingTestBase::get_trace(cli);
           ASSERT_TRUE(trace_res);

           auto json_trace = *trace_res;

           rapidjson::Document doc;
           doc.Parse(json_trace.data(), json_trace.size());
           ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

           for (const auto &[pntr, val] : {
                    std::pair{"/name", rapidjson::Value("mysql/stmt_prepare")},
                    std::pair{
                        "/attributes/mysql.sharing_blocked",
                        rapidjson::Value(env.expected_sharing_is_blocked)},
                    std::pair{"/events/0/name",
                              rapidjson::Value("mysql/connect_and_forward")},
                    std::pair{"/events/0/attributes/mysql.remote.is_connected",
                              rapidjson::Value(env.expected_is_connected)},
                }) {
             ASSERT_TRUE(TracingTestBase::json_pointer_eq(
                 doc, rapidjson::Pointer(pntr), val))
                 << json_trace;
           }
         } else {
           EXPECT_EQ(*warning_count_res, 0);
           EXPECT_THAT(num_warnings, ElementsAre(ElementsAre("0")));

           TracingTestBase::assert_warnings_no_trace(cli);
         }
       }

       auto stmt = std::move(*cmd_res);

       SCOPED_TRACE("// - execute");
       auto exec_res = stmt.execute();
       ASSERT_NO_ERROR(exec_res);

       {
         auto warning_count_res = cli.warning_count();
         ASSERT_NO_ERROR(warning_count_res);

         if (can_trace && env.trace_enabled) {
           EXPECT_EQ(*warning_count_res, 1);

           auto trace_res = TracingTestBase::get_trace(cli);
           ASSERT_TRUE(trace_res);

           auto json_trace = *trace_res;

           rapidjson::Document doc;
           doc.Parse(json_trace.data(), json_trace.size());
           ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

           for (const auto &[pntr, val] : {
                    std::pair{"/name", rapidjson::Value("mysql/stmt_execute")},
                    std::pair{
                        "/attributes/mysql.sharing_blocked",
                        rapidjson::Value(env.expected_sharing_is_blocked)},
                    std::pair{"/events/0/name",
                              rapidjson::Value("mysql/connect_and_forward")},
                    std::pair{"/events/0/attributes/mysql.remote.is_connected",
                              rapidjson::Value(env.expected_is_connected)},
                }) {
             ASSERT_TRUE(TracingTestBase::json_pointer_eq(
                 doc, rapidjson::Pointer(pntr), val))
                 << json_trace;
           }
         } else {
           EXPECT_EQ(*warning_count_res, 0);

           TracingTestBase::assert_warnings_no_trace(cli);
         }
       }
     }},
    {"stmt_prepare_ok_one_wildcard",  //
     true,                            // blocks sharing after test
     false,                           //
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       SCOPED_TRACE("// - prepare");
       auto cmd_res = cli.prepare("DO ?");
       ASSERT_NO_ERROR(cmd_res);
       {
         auto warning_count_res = cli.warning_count();
         ASSERT_NO_ERROR(warning_count_res);

         if (can_trace && env.trace_enabled) {
           EXPECT_EQ(*warning_count_res, 1);  // the trace

           auto trace_res = TracingTestBase::get_trace(cli);
           ASSERT_TRUE(trace_res);

           auto json_trace = *trace_res;

           rapidjson::Document doc;
           doc.Parse(json_trace.data(), json_trace.size());
           ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

           for (const auto &[pntr, val] : {
                    std::pair{"/name", rapidjson::Value("mysql/stmt_prepare")},
                    std::pair{
                        "/attributes/mysql.sharing_blocked",
                        rapidjson::Value(env.expected_sharing_is_blocked)},
                    std::pair{"/events/0/name",
                              rapidjson::Value("mysql/connect_and_forward")},
                    std::pair{"/events/0/attributes/mysql.remote.is_connected",
                              rapidjson::Value(env.expected_is_connected)},
                }) {
             ASSERT_TRUE(TracingTestBase::json_pointer_eq(
                 doc, rapidjson::Pointer(pntr), val))
                 << json_trace;
           }
         } else {
           EXPECT_EQ(*warning_count_res, 0);

           TracingTestBase::assert_warnings_no_trace(cli);
         }
       }

       std::array<MYSQL_BIND, 1> params{{
           {},
       }};
       params[0].buffer_type = MYSQL_TYPE_NULL;

       auto stmt = std::move(*cmd_res);
       stmt.bind_params(params);

       SCOPED_TRACE("// - execute");
       auto exec_res = stmt.execute();
       ASSERT_NO_ERROR(exec_res);

       {
         auto warning_count_res = cli.warning_count();
         ASSERT_NO_ERROR(warning_count_res);

         if (can_trace && env.trace_enabled) {
           EXPECT_EQ(*warning_count_res, 1);  // the trace

           auto trace_res = TracingTestBase::get_trace(cli);
           ASSERT_TRUE(trace_res);

           auto json_trace = *trace_res;

           rapidjson::Document doc;
           doc.Parse(json_trace.data(), json_trace.size());
           ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

           for (const auto &[pntr, val] : {
                    std::pair{"/name", rapidjson::Value("mysql/stmt_execute")},
                    std::pair{
                        "/attributes/mysql.sharing_blocked",
                        rapidjson::Value(env.expected_sharing_is_blocked)},
                    std::pair{"/events/0/name",
                              rapidjson::Value("mysql/connect_and_forward")},
                    std::pair{"/events/0/attributes/mysql.remote.is_connected",
                              rapidjson::Value(env.expected_is_connected)},
                }) {
             ASSERT_TRUE(TracingTestBase::json_pointer_eq(
                 doc, rapidjson::Pointer(pntr), val))
                 << json_trace;
           }
         } else {
           EXPECT_EQ(*warning_count_res, 0);

           TracingTestBase::assert_warnings_no_trace(cli);
         }
       }
     }},
    {"set_option_ok", false, false,
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       ASSERT_NO_ERROR(
           cli.set_server_option(MYSQL_OPTION_MULTI_STATEMENTS_OFF));

       auto warning_count_res = cli.warning_count();
       ASSERT_NO_ERROR(warning_count_res);

       if (can_trace && env.trace_enabled) {
         EXPECT_EQ(*warning_count_res, 1);  // the trace

         auto trace_res = TracingTestBase::get_trace(cli);
         ASSERT_TRUE(trace_res);

         auto json_trace = *trace_res;

         rapidjson::Document doc;
         doc.Parse(json_trace.data(), json_trace.size());
         ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

         for (const auto &[pntr, val] : {
                  std::pair{"/name", rapidjson::Value("mysql/set_option")},
                  std::pair{"/attributes/mysql.sharing_blocked",
                            rapidjson::Value(env.expected_sharing_is_blocked)},
                  std::pair{"/events/0/name",
                            rapidjson::Value("mysql/connect_and_forward")},
                  std::pair{"/events/0/attributes/mysql.remote.is_connected",
                            rapidjson::Value(env.expected_is_connected)},
              }) {
           ASSERT_TRUE(TracingTestBase::json_pointer_eq(
               doc, rapidjson::Pointer(pntr), val))
               << json_trace;
         }
       } else {
         EXPECT_EQ(*warning_count_res, 0);

         TracingTestBase::assert_warnings_no_trace(cli);
       }
     }},
    {"set_option_fail", false, false,
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       auto cmd_res =
           cli.set_server_option(static_cast<enum_mysql_set_option>(0xff));
       ASSERT_ERROR(cmd_res);
       EXPECT_EQ(cmd_res.error().value(), 1047);

       auto warning_count_res = cli.warning_count();
       ASSERT_NO_ERROR(warning_count_res);

       if (can_trace && env.trace_enabled) {
         EXPECT_EQ(*warning_count_res, 0);

         auto trace_res = TracingTestBase::get_trace(cli);
         ASSERT_TRUE(trace_res);

         auto json_trace = *trace_res;

         rapidjson::Document doc;
         doc.Parse(json_trace.data(), json_trace.size());
         ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

         for (const auto &[pntr, val] : {
                  std::pair{"/name", rapidjson::Value("mysql/set_option")},
                  std::pair{"/status_code", rapidjson::Value("ERROR")},
                  std::pair{"/attributes/mysql.sharing_blocked",
                            rapidjson::Value(env.expected_sharing_is_blocked)},
                  std::pair{"/events/0/name",
                            rapidjson::Value("mysql/connect_and_forward")},
                  std::pair{"/events/0/attributes/mysql.remote.is_connected",
                            rapidjson::Value(env.expected_is_connected)},
              }) {
           ASSERT_TRUE(TracingTestBase::json_pointer_eq(
               doc, rapidjson::Pointer(pntr), val))
               << json_trace;
         }
       } else {
         EXPECT_EQ(*warning_count_res, 0);

         TracingTestBase::assert_warnings_no_trace(cli);
       }
     }},
    {"init_schema_ok", false, false,
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       auto cmd_res = cli.use_schema("performance_schema");
       ASSERT_NO_ERROR(cmd_res);

       auto warning_count_res = cli.warning_count();
       ASSERT_NO_ERROR(warning_count_res);

       if (can_trace && env.trace_enabled) {
         EXPECT_EQ(*warning_count_res, 1);  // the trace

         auto trace_res = TracingTestBase::get_trace(cli);
         ASSERT_TRUE(trace_res);

         auto json_trace = *trace_res;

         rapidjson::Document doc;
         doc.Parse(json_trace.data(), json_trace.size());
         ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

         for (const auto &[pntr, val] : {
                  std::pair{"/name", rapidjson::Value("mysql/init_schema")},
                  std::pair{"/attributes/mysql.sharing_blocked",
                            rapidjson::Value(env.expected_sharing_is_blocked)},
                  std::pair{"/events/0/name",
                            rapidjson::Value("mysql/connect_and_forward")},
                  std::pair{"/events/0/attributes/mysql.remote.is_connected",
                            rapidjson::Value(env.expected_is_connected)},
              }) {
           ASSERT_TRUE(TracingTestBase::json_pointer_eq(
               doc, rapidjson::Pointer(pntr), val))
               << json_trace;
         }
       } else {
         EXPECT_EQ(*warning_count_res, 0);

         TracingTestBase::assert_warnings_no_trace(cli);
       }
     }},
    {"init_schema_fail", false, false,
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       auto cmd_res = cli.use_schema("does-not-exit");
       ASSERT_ERROR(cmd_res);
       EXPECT_EQ(cmd_res.error().value(), 1044) << cmd_res.error();

       auto warning_count_res = cli.warning_count();
       ASSERT_NO_ERROR(warning_count_res);

       if (can_trace && env.trace_enabled) {
         EXPECT_EQ(*warning_count_res, 0);

         auto trace_res = TracingTestBase::get_trace(cli);
         ASSERT_TRUE(trace_res);

         auto json_trace = *trace_res;

         rapidjson::Document doc;
         doc.Parse(json_trace.data(), json_trace.size());
         ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

         for (const auto &[pntr, val] : {
                  std::pair{"/name", rapidjson::Value("mysql/init_schema")},
                  std::pair{"/status_code", rapidjson::Value("ERROR")},
                  std::pair{"/attributes/mysql.sharing_blocked",
                            rapidjson::Value(env.expected_sharing_is_blocked)},
                  std::pair{"/events/0/name",
                            rapidjson::Value("mysql/connect_and_forward")},
                  std::pair{"/events/0/attributes/mysql.remote.is_connected",
                            rapidjson::Value(env.expected_is_connected)},
              }) {
           ASSERT_TRUE(TracingTestBase::json_pointer_eq(
               doc, rapidjson::Pointer(pntr), val))
               << json_trace;
         }
       } else {
         EXPECT_EQ(*warning_count_res, 0);

         TracingTestBase::assert_warnings_no_trace(cli);
       }
     }},
    {"statistics_ok", false, false,
     [](const ConnectionParam &connect_param, MysqlClient &cli,
        TracingCommandParam::Env env) {
       const bool can_trace = connect_param.can_trace();

       /*
        * check COM_STATISTICS generates a trace even though it doesn't
        * have a warning-count.
        */
       auto cmd_res = cli.stat();
       ASSERT_NO_ERROR(cmd_res);

       auto warning_count_res = cli.warning_count();
       ASSERT_NO_ERROR(warning_count_res);

       if (can_trace && env.trace_enabled) {
         // statistics has no warning count. But there should be trace.
         EXPECT_EQ(*warning_count_res, 0);

         auto trace_res = TracingTestBase::get_trace(cli);
         ASSERT_TRUE(trace_res);

         auto json_trace = *trace_res;

         rapidjson::Document doc;
         doc.Parse(json_trace.data(), json_trace.size());
         ASSERT_TRUE(TracingTestBase::trace_is_valid(doc));

         for (const auto &[pntr, val] : {
                  std::pair{"/name", rapidjson::Value("mysql/statistics")},
                  std::pair{"/attributes/mysql.sharing_blocked",
                            rapidjson::Value(env.expected_sharing_is_blocked)},
                  std::pair{"/events/0/name",
                            rapidjson::Value("mysql/connect_and_forward")},
                  std::pair{"/events/0/attributes/mysql.remote.is_connected",
                            rapidjson::Value(env.expected_is_connected)},
              }) {
           ASSERT_TRUE(TracingTestBase::json_pointer_eq(
               doc, rapidjson::Pointer(pntr), val))
               << json_trace;
         }
       } else {
         TracingTestBase::assert_warnings_no_trace(cli);
       }
     }},
};

class TracingCommandTest
    : public TracingTestBase,
      public ::testing::WithParamInterface<
          std::tuple<ConnectionParam, TracingCommandParam>> {};

TEST_P(TracingCommandTest,
       classic_protocol_router_set_trace_enable_sharing_blocked) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RS2");
  RecordProperty("Requirement", "'ROUTER SET trace = 1' MUST enable the trace");
  RecordProperty("Description",  //
                 R"(- connect()
- block connection sharing by sending a BEGIN
- send command, check no trace
- enable ROUTER SET trace = 1
- send command again, check there is a trace
)");

  auto [connect_param, test_param] = GetParam();

  auto can_trace = connect_param.can_trace();

  bool expected_is_connected = true;
  bool expected_sharing_is_blocked = true;

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = test_param.needs_super_privs
                     ? SharedServer::admin_account()
                     : SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                              shared_router()->port(connect_param)));

  SCOPED_TRACE("// block connection sharing");
  {
    ASSERT_NO_ERROR(cli.query("SET @block_me = 1"));

    auto warning_count_res = cli.warning_count();
    ASSERT_NO_ERROR(warning_count_res);

    EXPECT_EQ(*warning_count_res, 0);
  }

  SCOPED_TRACE("// check cmd without trace");
  ASSERT_NO_FATAL_FAILURE(test_param.test_func(
      connect_param, cli,
      {expected_is_connected, expected_sharing_is_blocked, false}));

  SCOPED_TRACE("// ROUTER SET trace = 1");
  {
    auto query_res = cli.query("ROUTER SET trace = 1");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
      {
        auto warning_count_res = cli.warning_count();
        ASSERT_NO_ERROR(warning_count_res);

        EXPECT_EQ(*warning_count_res, 0);
      }
      {
        auto server_status_res = cli.server_status();
        ASSERT_NO_ERROR(server_status_res);

        // ignore the session-state-changed flag which announces a
        // protocol-field, and doesn't trace session-state.
        auto server_status = *server_status_res & ~SERVER_SESSION_STATE_CHANGED;

        // no flags outside the expected set.
        EXPECT_EQ(server_status & ~show_warnings_status_mask, 0)
            << std::bitset<32>(server_status);
        // ensure the connection's flags are remembered
        EXPECT_EQ(server_status & show_warnings_status_mask,
                  SERVER_STATUS_AUTOCOMMIT)
            << std::bitset<32>(server_status);
      }

      assert_warnings_no_trace(cli);
      {
        auto warning_count_res = cli.warning_count();
        ASSERT_NO_ERROR(warning_count_res);

        EXPECT_EQ(*warning_count_res, 0);
      }
    } else {
      ASSERT_ERROR(query_res);
      assert_warnings_no_trace(cli);
    }
  }

  SCOPED_TRACE("// check cmd with trace");
  ASSERT_NO_FATAL_FAILURE(test_param.test_func(
      connect_param, cli,
      {expected_is_connected, expected_sharing_is_blocked, true}));
}

TEST_P(TracingCommandTest,
       classic_protocol_router_set_trace_enable_after_from_pool) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RS2");
  RecordProperty("Requirement", "'ROUTER SET trace = 1' MUST enable the trace");
  RecordProperty("Description", R"(- close all connections of the pool
- connect to the router
- execute command and expect no trace
- enable trace
- wait until connection is pooled
- execute command and expect a trace
)");

  auto [connect_param, test_param] = GetParam();

  auto can_trace = connect_param.can_trace();
  bool expected_is_connected = !can_trace;
  bool expected_sharing_is_blocked = test_param.sharing_blocked_after_test;

  SCOPED_TRACE("// ensure that the pool is empty.");
  for (auto *srv : shared_servers()) {
    srv->close_all_connections();
  }

  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = test_param.needs_super_privs
                     ? SharedServer::admin_account()
                     : SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                              shared_router()->port(connect_param)));

  SCOPED_TRACE("// cmds without tracing");
  ASSERT_NO_FATAL_FAILURE(test_param.test_func(
      connect_param, cli,
      {expected_is_connected, expected_sharing_is_blocked, false}));

  // is the sharing is blocked, the server-connection should stay attached.
  if (expected_sharing_is_blocked) {
    expected_is_connected = true;
  }

  SCOPED_TRACE("// ROUTER SET trace = 1");
  {
    auto query_res = cli.query("ROUTER SET trace = 1");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
      assert_warnings_no_trace(cli);

      {
        auto server_status_res = cli.server_status();
        ASSERT_NO_ERROR(server_status_res);

        // ignore the session-state-changed flag which announces a
        // protocol-field, and doesn't trace session-state.
        auto server_status = *server_status_res & ~SERVER_SESSION_STATE_CHANGED;

        // no flags outside the expected set.
        EXPECT_EQ(server_status & ~show_warnings_status_mask, 0)
            << std::bitset<32>(server_status);
        // ensure the connection's flags are remembered
        EXPECT_EQ(server_status & show_warnings_status_mask,
                  SERVER_STATUS_AUTOCOMMIT)
            << std::bitset<32>(server_status);
      }
    } else {
      ASSERT_ERROR(query_res);
      assert_warnings_no_trace(cli);
    }
  }

  if (can_trace && !expected_sharing_is_blocked) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));
  }

  SCOPED_TRACE("// cmds with tracing");
  ASSERT_NO_FATAL_FAILURE(test_param.test_func(
      connect_param, cli,
      {expected_is_connected, expected_sharing_is_blocked, true}));
}

TEST_P(TracingCommandTest,
       classic_protocol_router_set_trace_enable_after_reconnect) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RS2");
  RecordProperty("Requirement", "'ROUTER SET trace = 1' MUST enable the trace");

  auto [connect_param, test_param] = GetParam();

  auto can_trace = connect_param.can_trace();
  bool expected_is_connected = !can_trace;
  bool expected_sharing_is_blocked = test_param.sharing_blocked_after_test;

  SCOPED_TRACE("// ensure that the pool is empty.");
  for (auto *srv : shared_servers()) {
    srv->close_all_connections();
  }

  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = test_param.needs_super_privs
                     ? SharedServer::admin_account()
                     : SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                              shared_router()->port(connect_param)));

  SCOPED_TRACE("// cmds without tracing");
  ASSERT_NO_FATAL_FAILURE(test_param.test_func(
      connect_param, cli,
      {expected_is_connected, expected_sharing_is_blocked, false}));

  // is the sharing is blocked, the server-connection should stay attached.
  if (expected_sharing_is_blocked) {
    expected_is_connected = true;
  }

  SCOPED_TRACE("// ROUTER SET trace = 1");
  {
    auto query_res = cli.query("ROUTER SET trace = 1");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
      assert_warnings_no_trace(cli);

      {
        auto server_status_res = cli.server_status();
        ASSERT_NO_ERROR(server_status_res);

        // ignore the session-state-changed flag which announces a
        // protocol-field, and doesn't trace session-state.
        auto server_status = *server_status_res & ~SERVER_SESSION_STATE_CHANGED;

        // no flags outside the expected set.
        EXPECT_EQ(server_status & ~show_warnings_status_mask, 0)
            << std::bitset<32>(server_status);
        // ensure the connection's flags are remembered
        EXPECT_EQ(server_status & show_warnings_status_mask,
                  SERVER_STATUS_AUTOCOMMIT)
            << std::bitset<32>(server_status);
      }
    } else {
      ASSERT_ERROR(query_res);
      assert_warnings_no_trace(cli);
    }
  }

  SCOPED_TRACE("// force a reconnect");
  if (can_trace && !expected_sharing_is_blocked) {
    ASSERT_NO_ERROR(
        shared_router()->wait_for_stashed_server_connections(1, 10s));

    for (auto *srv : shared_servers()) {
      srv->close_all_connections();
    }

    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 10s));
  }

  SCOPED_TRACE("// cmds with tracing");
  ASSERT_NO_FATAL_FAILURE(test_param.test_func(
      connect_param, cli,
      {expected_is_connected, expected_sharing_is_blocked, true}));
}

INSTANTIATE_TEST_SUITE_P(
    Spec, TracingCommandTest,
    ::testing::Combine(::testing::ValuesIn(connection_params),
                       ::testing::ValuesIn(tracing_command_params)),
    [](auto &info) {
      return "via_" + std::get<0>(info.param).testname + "__" +
             std::string(std::get<1>(info.param).test_name);
    });

class TracingTest : public TracingTestBase,
                    public ::testing::WithParamInterface<ConnectionParam> {};

TEST_P(TracingTest, classic_protocol_router_set_trace_disable) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RS3");
  RecordProperty("Requirement",
                 "'ROUTER SET trace = 0' MUST disable the trace");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  SCOPED_TRACE("// check that tracing is disabled at start");
  ASSERT_NO_ERROR(cli.query("DO 1"));
  assert_warnings_no_trace(cli);

  SCOPED_TRACE("// enable trace");
  {
    auto cmd_res = cli.query("ROUTER SET trace = 1");
    if (can_trace) {
      ASSERT_NO_ERROR(cmd_res);
    } else {
      ASSERT_ERROR(cmd_res);
    }
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// check that a trace is generated");
  ASSERT_NO_ERROR(cli.query("DO 1"));
  if (can_trace) {
    assert_warnings_with_trace(cli);
  } else {
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// disable trace");
  {
    auto cmd_res = cli.query("ROUTER SET trace = 0");
    if (can_trace) {
      ASSERT_NO_ERROR(cmd_res);
    } else {
      ASSERT_ERROR(cmd_res);
    }
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// check that no trace is generated");
  ASSERT_NO_ERROR(cli.query("DO 1"));
  assert_warnings_no_trace(cli);

  SCOPED_TRACE("// check that still no trace is generated");
  ASSERT_NO_ERROR(cli.query("DO 1"));
  assert_warnings_no_trace(cli);
}

TEST_P(TracingTest, classic_protocol_query_query_attribute_enable) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RQ1");
  RecordProperty(
      "Requirement",
      "The Query attribute `router.trace` with the value `1` enables "
      "the trace for the current statement.");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  SCOPED_TRACE("// disable trace");
  {
    auto query_res = cli.query("ROUTER SET trace = 0");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_ERROR(query_res);
    }
  }

  for (const auto &param : {
           // "unsupported buffer type"
           // std::pair(MYSQL_TYPE_INT24, 0),
           // std::pair(MYSQL_TYPE_ENUM, 0),
           // std::pair(MYSQL_TYPE_SET, 0),
           // std::pair(MYSQL_TYPE_GEOMETRY, 0),
           std::pair(MYSQL_TYPE_DECIMAL, 1064),
           std::pair(MYSQL_TYPE_NEWDECIMAL, 1064),
           std::pair(MYSQL_TYPE_TINY, 0),
           std::pair(MYSQL_TYPE_SHORT, 0),
           std::pair(MYSQL_TYPE_LONG, 0),
           std::pair(MYSQL_TYPE_FLOAT, 1064),
           std::pair(MYSQL_TYPE_DOUBLE, 1064),
           std::pair(MYSQL_TYPE_NULL, 1064),
           std::pair(MYSQL_TYPE_TIMESTAMP, 1064),
           std::pair(MYSQL_TYPE_LONGLONG, 0),
           std::pair(MYSQL_TYPE_JSON, 1064),  // not ok
           std::pair(MYSQL_TYPE_VARCHAR, 0),
           std::pair(MYSQL_TYPE_TINY_BLOB, 0),
           std::pair(MYSQL_TYPE_MEDIUM_BLOB, 0),
           std::pair(MYSQL_TYPE_LONG_BLOB, 0),
           std::pair(MYSQL_TYPE_BLOB, 0),
           std::pair(MYSQL_TYPE_VAR_STRING, 0),
           std::pair(MYSQL_TYPE_STRING, 0),
       }) {
    SCOPED_TRACE("// cmd with query-attr: router.trace = 1 {type: " +
                 std::to_string(param.first) + "}");

    std::array<MYSQL_BIND, 1> params{one_getter(param.first)};
    std::array<const char *, 1> param_names{{"router.trace"}};

    auto query_res = cli.query("DO 'router.trace = 1'", params, param_names);
    if (!can_trace || param.second == 0) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(), param.second);
    }

    if (can_trace && param.second == 0) {
      assert_warnings_with_trace(cli);
    } else {
      assert_warnings_no_trace(cli);
    }
  }
}

TEST_P(TracingTest, classic_protocol_query_query_attribute_disable) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RQ2");
  RecordProperty(
      "Requirement",
      "The Query attribute `router.trace` with the value `0` disables "
      "the trace for the current statement.");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  SCOPED_TRACE("// enable trace");
  {
    auto query_res = cli.query("ROUTER SET trace = 1");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_ERROR(query_res);
    }
  }

  for (const auto &param : {
           // "unsupported buffer type"
           // std::pair(MYSQL_TYPE_INT24, 0),
           // std::pair(MYSQL_TYPE_ENUM, 0),
           // std::pair(MYSQL_TYPE_SET, 0),
           // std::pair(MYSQL_TYPE_GEOMETRY, 0),
           std::pair(MYSQL_TYPE_DECIMAL, 1064),
           std::pair(MYSQL_TYPE_NEWDECIMAL, 1064),
           std::pair(MYSQL_TYPE_TINY, 0),
           std::pair(MYSQL_TYPE_SHORT, 0),
           std::pair(MYSQL_TYPE_LONG, 0),
           std::pair(MYSQL_TYPE_FLOAT, 1064),
           std::pair(MYSQL_TYPE_DOUBLE, 1064),
           std::pair(MYSQL_TYPE_NULL, 1064),
           std::pair(MYSQL_TYPE_TIMESTAMP, 1064),
           std::pair(MYSQL_TYPE_LONGLONG, 0),
           std::pair(MYSQL_TYPE_JSON, 1064),
           std::pair(MYSQL_TYPE_VARCHAR, 0),
           std::pair(MYSQL_TYPE_TINY_BLOB, 0),
           std::pair(MYSQL_TYPE_MEDIUM_BLOB, 0),
           std::pair(MYSQL_TYPE_LONG_BLOB, 0),
           std::pair(MYSQL_TYPE_BLOB, 0),
           std::pair(MYSQL_TYPE_VAR_STRING, 0),
           std::pair(MYSQL_TYPE_STRING, 0),
       }) {
    SCOPED_TRACE("// cmd with query-attr: router.trace = 0 {type: " +
                 std::to_string(param.first) + "}");

    std::array<MYSQL_BIND, 1> params{zero_getter(param.first)};
    std::array<const char *, 1> param_names{{"router.trace"}};
    auto query_res = cli.query("DO 'router.trace = 1'", params, param_names);
    if (!can_trace || param.second == 0) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(), param.second);
    }

    assert_warnings_no_trace(cli);
  }
}

TEST_P(TracingTest, classic_protocol_query_query_attribute_invalid_value) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RQ3");
  RecordProperty("Requirement",
                 "If the Query attribute `router.trace` has a value different "
                 "from `0` or `1`, the query MUST fail.");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  for (const auto &param : {
           // "unsupported buffer type"
           // std::pair(MYSQL_TYPE_INT24, 0),
           // std::pair(MYSQL_TYPE_ENUM, 0),
           // std::pair(MYSQL_TYPE_SET, 0),
           // std::pair(MYSQL_TYPE_GEOMETRY, 0),
           std::pair(MYSQL_TYPE_DECIMAL, 1064),
           std::pair(MYSQL_TYPE_NEWDECIMAL, 1064),
           std::pair(MYSQL_TYPE_TINY, 1064),
           std::pair(MYSQL_TYPE_SHORT, 1064),
           std::pair(MYSQL_TYPE_LONG, 1064),
           std::pair(MYSQL_TYPE_FLOAT, 1064),
           std::pair(MYSQL_TYPE_DOUBLE, 1064),
           std::pair(MYSQL_TYPE_NULL, 1064),
           std::pair(MYSQL_TYPE_TIMESTAMP, 1064),
           std::pair(MYSQL_TYPE_LONGLONG, 1064),
           std::pair(MYSQL_TYPE_JSON, 1064),  // not ok
           std::pair(MYSQL_TYPE_VARCHAR, 1064),
           std::pair(MYSQL_TYPE_TINY_BLOB, 1064),
           std::pair(MYSQL_TYPE_MEDIUM_BLOB, 1064),
           std::pair(MYSQL_TYPE_LONG_BLOB, 1064),
           std::pair(MYSQL_TYPE_BLOB, 1064),
           std::pair(MYSQL_TYPE_VAR_STRING, 1064),
           std::pair(MYSQL_TYPE_STRING, 1064),
       }) {
    SCOPED_TRACE("// cmd with query-attr: router.trace = 2 {type: " +
                 std::to_string(param.first) + "}");

    std::array<MYSQL_BIND, 1> params{two_getter(param.first)};
    std::array<const char *, 1> param_names{{"router.trace"}};
    auto query_res = cli.query("DO 'router.trace = 2'", params, param_names);
    if (can_trace && param.second != 0) {
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(), param.second);
    } else {
      ASSERT_NO_ERROR(query_res);
    }

    assert_warnings_no_trace(cli);
  }
}

TEST_P(TracingTest, classic_protocol_query_query_attribute_precedence) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RQ4");
  RecordProperty("Requirement",
                 "The Query attribute `router.trace` MUST take precedence over "
                 "the Router session variable.");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  {
    auto query_res = cli.query("ROUTER SET trace = 0");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_ERROR(query_res);
    }
  }

  {
    std::array<MYSQL_BIND, 1> params{one_getter(MYSQL_TYPE_TINY)};
    std::array<const char *, 1> param_names{{"router.trace"}};
    auto query_res = cli.query("DO 'router.trace = 0'", params, param_names);
    ASSERT_NO_ERROR(query_res);

    if (can_trace) {
      assert_warnings_with_trace(cli);
    } else {
      assert_warnings_no_trace(cli);
    }
  }

  {
    auto query_res = cli.query("ROUTER SET trace = 1");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_ERROR(query_res);
    }
  }

  {
    std::array<MYSQL_BIND, 1> params{zero_getter(MYSQL_TYPE_TINY)};
    std::array<const char *, 1> param_names{{"router.trace"}};
    auto query_res = cli.query("DO 'router.trace = 0'", params, param_names);
    ASSERT_NO_ERROR(query_res);

    assert_warnings_no_trace(cli);
  }

  assert_warnings_no_trace(cli);
}

TEST_P(TracingTest, classic_protocol_query_query_attribute_overwrite) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RQ5");
  RecordProperty("Requirement",
                 "If the query-attribute `router.trace` specified multiple "
                 "times, the last value MUST be used.");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  for (const auto &param : {
           // "unsupported buffer type"
           // std::pair(MYSQL_TYPE_INT24, 0),
           // std::pair(MYSQL_TYPE_ENUM, 0),
           // std::pair(MYSQL_TYPE_SET, 0),
           // std::pair(MYSQL_TYPE_GEOMETRY, 0),
           std::pair(MYSQL_TYPE_DECIMAL, 1064),
           std::pair(MYSQL_TYPE_NEWDECIMAL, 1064),
           std::pair(MYSQL_TYPE_TINY, 0),
           std::pair(MYSQL_TYPE_SHORT, 0),
           std::pair(MYSQL_TYPE_LONG, 0),
           std::pair(MYSQL_TYPE_FLOAT, 1064),
           std::pair(MYSQL_TYPE_DOUBLE, 1064),
           std::pair(MYSQL_TYPE_NULL, 1064),
           std::pair(MYSQL_TYPE_TIMESTAMP, 1064),
           std::pair(MYSQL_TYPE_LONGLONG, 0),
           std::pair(MYSQL_TYPE_JSON, 1064),  // not ok
           std::pair(MYSQL_TYPE_VARCHAR, 0),
           std::pair(MYSQL_TYPE_TINY_BLOB, 0),
           std::pair(MYSQL_TYPE_MEDIUM_BLOB, 0),
           std::pair(MYSQL_TYPE_LONG_BLOB, 0),
           std::pair(MYSQL_TYPE_BLOB, 0),
           std::pair(MYSQL_TYPE_VAR_STRING, 0),
           std::pair(MYSQL_TYPE_STRING, 0),
       }) {
    SCOPED_TRACE("// cmd with query-attr: router.trace = 1 -> 0 {type: " +
                 std::to_string(param.first) + "}");
    {
      std::array<MYSQL_BIND, 2> params{one_getter(param.first),
                                       zero_getter(param.first)};
      std::array<const char *, 2> param_names{{"router.trace", "router.traCE"}};

      auto query_res = cli.query("DO 'router.trace = 0'", params, param_names);
      if (!can_trace || param.second == 0) {
        ASSERT_NO_ERROR(query_res);
      } else {
        ASSERT_ERROR(query_res);
        EXPECT_EQ(query_res.error().value(), param.second);
      }

      assert_warnings_no_trace(cli);
    }

    SCOPED_TRACE("// cmd with query-attr: router.trace = 0 -> 1 {type: " +
                 std::to_string(param.first) + "}");

    {
      std::array<MYSQL_BIND, 2> params{zero_getter(param.first),
                                       one_getter(param.first)};
      std::array<const char *, 2> param_names{{"router.trace", "router.trace"}};

      auto query_res = cli.query("DO 'router.trace = 0, router.trace = 1'",
                                 params, param_names);
      if (!can_trace || param.second == 0) {
        ASSERT_NO_ERROR(query_res);
      } else {
        ASSERT_ERROR(query_res);
        EXPECT_EQ(query_res.error().value(), param.second);
      }

      if (can_trace && param.second == 0) {
        assert_warnings_with_trace(cli);
      } else {
        assert_warnings_no_trace(cli);
      }
    }
  }
}

TEST_P(TracingTest, classic_protocol_query_query_attribute_unknown) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "FR1.2");
  RecordProperty(
      "Requirement",
      "If a Query attribute starts with `router.` and is not known by "
      "Router, the command MUST fail.");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  for (const auto &param : {
           // "unsupported buffer type"
           // std::pair(MYSQL_TYPE_INT24, 0),
           // std::pair(MYSQL_TYPE_ENUM, 0),
           // std::pair(MYSQL_TYPE_SET, 0),
           // std::pair(MYSQL_TYPE_GEOMETRY, 0),
           std::pair(MYSQL_TYPE_DECIMAL, 1064),
           std::pair(MYSQL_TYPE_NEWDECIMAL, 1064),
           std::pair(MYSQL_TYPE_TINY, 1064),
           std::pair(MYSQL_TYPE_SHORT, 1064),
           std::pair(MYSQL_TYPE_LONG, 1064),
           std::pair(MYSQL_TYPE_FLOAT, 1064),
           std::pair(MYSQL_TYPE_DOUBLE, 1064),
           std::pair(MYSQL_TYPE_NULL, 1064),
           std::pair(MYSQL_TYPE_TIMESTAMP, 1064),
           std::pair(MYSQL_TYPE_LONGLONG, 1064),
           std::pair(MYSQL_TYPE_JSON, 1064),  // not ok
           std::pair(MYSQL_TYPE_VARCHAR, 1064),
           std::pair(MYSQL_TYPE_TINY_BLOB, 1064),
           std::pair(MYSQL_TYPE_MEDIUM_BLOB, 1064),
           std::pair(MYSQL_TYPE_LONG_BLOB, 1064),
           std::pair(MYSQL_TYPE_BLOB, 1064),
           std::pair(MYSQL_TYPE_VAR_STRING, 1064),
           std::pair(MYSQL_TYPE_STRING, 1064),
       }) {
    SCOPED_TRACE("// cmd with query-attr: router.unknown = 1 {type: " +
                 std::to_string(param.first) + "}");

    std::array<MYSQL_BIND, 1> params{one_getter(param.first)};
    std::array<const char *, 1> param_names{{"rouTER.unknown"}};
    auto query_res =
        cli.query("DO 'query-attr: rouTER.unknown = 2'", params, param_names);
    if (can_trace && param.second != 0) {
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(), param.second);
    } else {
      ASSERT_NO_ERROR(query_res);
    }

    assert_warnings_no_trace(cli);
  }
}

TEST_P(TracingTest, classic_protocol_query_query_attribute_character_set) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RQ6");
  RecordProperty("Requirement",
                 "The query attributes MUST be matched against the "
                 "`character_set_client`");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  SCOPED_TRACE(
      "// query-attr: rouTER.trace = 1 (default, case-insensitive charset)");
  {
    std::array<MYSQL_BIND, 1> params{one_getter(MYSQL_TYPE_STRING)};
    std::array<const char *, 1> param_names{{"rouTER.trace"}};
    auto query_res =
        cli.query("DO 'query-attr: rouTER.trace = 1'", params, param_names);
    ASSERT_NO_ERROR(query_res);

    if (can_trace) {
      assert_warnings_with_trace(cli);
    } else {
      assert_warnings_no_trace(cli);
    }
  }

  ASSERT_NO_ERROR(cli.query("set names 'utf8mb4'"));
  SCOPED_TRACE(
      "// query-attr: rouTER.trace = 1 (utf8, default case-insensitive "
      "collation)");
  {
    std::array<MYSQL_BIND, 1> params{one_getter(MYSQL_TYPE_STRING)};
    std::array<const char *, 1> param_names{{"rouTER.trace"}};
    auto query_res =
        cli.query("DO 'query-attr: rouTER.trace = 1'", params, param_names);
    ASSERT_NO_ERROR(query_res);

    if (can_trace) {
      assert_warnings_with_trace(cli);
    } else {
      assert_warnings_no_trace(cli);
    }
  }

  SCOPED_TRACE(
      "// query-attr: rouTER.trace = 1 (utf8, case-sensitive collation)");
  ASSERT_NO_ERROR(cli.query("set names 'utf8mb4' collate 'utf8mb4_bin'"));
  {
    std::array<MYSQL_BIND, 1> params{one_getter(MYSQL_TYPE_STRING)};
    std::array<const char *, 1> param_names{{"rouTER.trace"}};
    auto query_res =
        cli.query("DO 'query-attr: rouTER.trace = 1'", params, param_names);
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_NO_ERROR(query_res);
    }

    assert_warnings_no_trace(cli);
  }
}

TEST_P(TracingTest, classic_protocol_reset_connection_ok) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RS1");
  RecordProperty("Requirement",
                 "The trace MUST be disabled at start or when the client sends "
                 "reset-connection or change-user.");
  RecordProperty("Description",
                 "verify the 'reset-connection' part of the requirement");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// check tracing is disabled at start");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// reset the connection");
  {
    ASSERT_NO_ERROR(cli.reset_connection());
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// check tracing is disabled after reset");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));
    assert_warnings_no_trace(cli);
  }

  bool can_trace = GetParam().can_trace();
  {
    auto query_res = cli.query("ROUTER SET trace = 1");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(), 1064);  // parse error
    }
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// verify tracing is enabled.");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));

    if (can_trace) {
      assert_warnings_with_trace(cli);
    } else {
      assert_warnings_no_trace(cli);
    }
  }

  SCOPED_TRACE("// reset connection");
  ASSERT_NO_ERROR(cli.reset_connection());
  assert_warnings_no_trace(cli);

  SCOPED_TRACE("// check tracing is disabled after reset");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));
    assert_warnings_no_trace(cli);
  }

  // back to the initial state.
  {
    auto query_res = cli.query("ROUTER SET trace = 0");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_ERROR(query_res);

      auto warnings_res = query_one_result(cli, "SHOW warnings");
      ASSERT_NO_ERROR(warnings_res);
      EXPECT_EQ(query_res.error().value(), 1064);  // parse error
    }
  }

  SCOPED_TRACE("// check tracing is disabled");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// reset the connection");
  ASSERT_NO_ERROR(cli.reset_connection());
  assert_warnings_no_trace(cli);

  SCOPED_TRACE("// check tracing is disabled after reset");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));
    assert_warnings_no_trace(cli);
  }
}

TEST_P(TracingTest, classic_protocol_change_user_ok) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RS1");
  RecordProperty("Requirement",
                 "The trace MUST be disabled at start or when the client sends "
                 "reset-connection or change-user.");
  RecordProperty("Description",
                 "verify the 'change-user' part of the requirement");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// check tracing is disabled at start");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// reset the connection");
  {
    ASSERT_NO_ERROR(cli.change_user(account.username, account.password, ""));
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// check tracing is disabled after reset");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));
    assert_warnings_no_trace(cli);
  }

  bool can_trace = GetParam().can_trace();
  {
    auto query_res = cli.query("ROUTER SET trace = 1");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_ERROR(query_res);
      EXPECT_EQ(query_res.error().value(), 1064);  // parse error
    }
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// verify tracing is enabled.");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));

    if (can_trace) {
      assert_warnings_with_trace(cli);
    } else {
      assert_warnings_no_trace(cli);
    }
  }

  SCOPED_TRACE("// change user");
  ASSERT_NO_ERROR(cli.change_user(account.username, account.password, ""));
  assert_warnings_no_trace(cli);

  SCOPED_TRACE("// check tracing is disabled after reset");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));
    assert_warnings_no_trace(cli);
  }

  // back to the initial state.
  {
    auto query_res = cli.query("ROUTER SET trace = 0");
    if (can_trace) {
      ASSERT_NO_ERROR(query_res);
    } else {
      ASSERT_ERROR(query_res);

      auto warnings_res = query_one_result(cli, "SHOW warnings");
      ASSERT_NO_ERROR(warnings_res);
      EXPECT_EQ(query_res.error().value(), 1064);  // parse error
    }
  }

  SCOPED_TRACE("// check tracing is disabled");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));
    assert_warnings_no_trace(cli);
  }

  SCOPED_TRACE("// reset the connection");
  ASSERT_NO_ERROR(cli.change_user(account.username, account.password, ""));
  assert_warnings_no_trace(cli);

  SCOPED_TRACE("// check tracing is disabled after reset");
  {
    ASSERT_NO_ERROR(cli.query("DO 1"));
    assert_warnings_no_trace(cli);
  }
}

TEST_P(TracingTest, classic_protocol_router_multi_statements) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "FR1.3");
  RecordProperty("Requirement",
                 "If the client sends a multi-statement while connection "
                 "sharing is active, the statement MUST fail");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);
  cli.flags(CLIENT_MULTI_STATEMENTS);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  SCOPED_TRACE("send a multi-statement");
  {
    auto stmt_res = cli.query("DO 1; DO 2");

    if (can_trace) {
      ASSERT_ERROR(stmt_res);
      EXPECT_EQ(stmt_res.error().value(), 4501);  // Not allowed
    } else {
      ASSERT_NO_ERROR(stmt_res);
    }
  }
}

TEST_P(TracingTest, classic_protocol_router_set_trace_ok) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RS5");
  RecordProperty("Requirement",
                 "`ROUTER SET <name>` MUST be matched case-insensitive.");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  for (const auto &p : {
           "ROUTER SET trace = 0",
           "ROUTER SET traCe = 0",
           "ROUTER Set traCe = 0",
           "Router Set traCe = 0",
           "Router Set `traCe` = 0",
           "ROUTER SET trace = 1",
           "ROUTER SET traCe = 1",
           "ROUTER Set traCe = 1",
           "Router Set traCe = 1",
           "Router Set `traCe` = 1",
           "/*! Router Set `traCe` = 1 */",
           "/*!80000 Router Set `traCe` = 1 */",
           "/*! Router */ /*! Set */ `traCe` = 1",
       }) {
    SCOPED_TRACE(p);
    {
      auto stmt_res = cli.query(p);

      if (can_trace) {
        ASSERT_NO_ERROR(stmt_res);
      } else {
        ASSERT_ERROR(stmt_res);
        EXPECT_EQ(stmt_res.error().value(), 1064);  // Parse error
      }
    }
  }
}

TEST_P(TracingTest, classic_protocol_router_set_trace_failed) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "RS4");
  RecordProperty("Requirement",
                 "`ROUTER SET trace = <val>` with a `<val>` different from `0` "
                 "or `1` MUST fail.");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  for (const auto &stmt_and_err : {
           // extra data after valid query.
           std::pair("ROUTER Set traCe = 1 extra", ER_PARSE_ERROR),
           // multiple vars aren't supported yet.
           std::pair("ROUTER Set traCe = 1, trace = 0", ER_PARSE_ERROR),
           // value too small
           std::pair("ROUTER SET trace = -1", ER_WRONG_VALUE_FOR_VAR),
           // value too large
           std::pair("ROUTER SET trace = 2", ER_WRONG_VALUE_FOR_VAR),
           std::pair("ROUTER SET traCe = '0'",
                     ER_WRONG_VALUE_FOR_VAR),                    // 1231
           std::pair("ROUTER Set traCe = 1.0", ER_PARSE_ERROR),  // 1064
           std::pair("ROUTER Set traCe := 1", ER_PARSE_ERROR),
           std::pair("ROUTER Set traCe", ER_PARSE_ERROR),
       }) {
    SCOPED_TRACE(stmt_and_err.first);
    {
      auto stmt_res = cli.query(stmt_and_err.first);
      ASSERT_ERROR(stmt_res);

      if (can_trace) {
        EXPECT_EQ(stmt_res.error().value(), stmt_and_err.second)
            << stmt_res.error();
      } else {
        EXPECT_EQ(stmt_res.error().value(), ER_PARSE_ERROR);  // 1064
      }
    }
  }
}

TEST_P(TracingTest, classic_protocol_router_set_trace_via_prepare) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "FR2");
  RecordProperty("Requirement",
                 "If connection pooling is not active, or the query is sent "
                 "via other commands (e.g. `COM_STMT_PREPARE`) the behaviour "
                 "MUST not change.");
  RecordProperty("Description",
                 "prepare `ROUTER SET trace = 1` and expect it to fail if "
                 "pooling is enabled.");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// cmd");
  {
    auto stmt_res = cli.prepare("ROUTER SET trace = 1");
    ASSERT_ERROR(stmt_res);

    EXPECT_EQ(stmt_res.error().value(), ER_PARSE_ERROR)
        << stmt_res.error();  // 1064
  }
}

TEST_P(TracingTest, classic_protocol_router_set_failed) {
  RecordProperty("Worklog", "15582");
  RecordProperty("RequirementId", "FR1.1");
  RecordProperty(
      "Requirement",
      "If the statement starts with the keyword `ROUTER` and is not known by "
      "Router, it MUST fail.");

  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  bool can_trace = GetParam().can_trace();

  for (const auto &stmt_and_err : {
           std::pair("ROUTER SET unknown_opt = -1", ER_UNKNOWN_SYSTEM_VARIABLE),
           std::pair("ROUTER no_such_token = 1", ER_PARSE_ERROR),
       }) {
    SCOPED_TRACE(stmt_and_err.first);
    {
      auto stmt_res = cli.query(stmt_and_err.first);
      ASSERT_ERROR(stmt_res);

      if (can_trace) {
        EXPECT_EQ(stmt_res.error().value(), stmt_and_err.second)
            << stmt_res.error();
      } else {
        EXPECT_EQ(stmt_res.error().value(), ER_PARSE_ERROR);  // 1064
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, TracingTest,
                         ::testing::ValuesIn(connection_params),
                         [](auto &info) {
                           return "via_" + info.param.testname;
                         });

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
