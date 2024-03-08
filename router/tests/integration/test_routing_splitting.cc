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

#include <rapidjson/pointer.h>

#include "hexify.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/stdx/filesystem.h"
#include "mysql/harness/stdx/ranges.h"   // enumerate
#include "mysql/harness/string_utils.h"  // split_string
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

using testing::AnyOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Not;
using testing::Pair;
using testing::UnorderedElementsAre;

static constexpr const auto kIdleServerConnectionsSleepTime{10ms};

static constexpr const std::string_view kDisabled{"DISABLED"};
static constexpr const std::string_view kRequired{"REQUIRED"};
static constexpr const std::string_view kPreferred{"PREFERRED"};
static constexpr const std::string_view kPassthrough{"PASSTHROUGH"};
static constexpr const std::string_view kAsClient{"AS_CLIENT"};

std::ostream &operator<<(std::ostream &os, const MysqlError &err) {
  os << err.sql_state() << " (" << err.value() << ") " << err.message();
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
        auto *fld = row[ndx];

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

  for (const auto *row : query_it->rows()) {
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

static stdx::expected<std::vector<std::pair<std::string, uint32_t>>, MysqlError>
changed_event_counters(MysqlClient &cli) {
  return changed_event_counters_impl(cli, R"(SELECT EVENT_NAME, COUNT_STAR
 FROM performance_schema.events_statements_summary_by_thread_by_event_name AS e
 JOIN performance_schema.threads AS t ON (e.THREAD_ID = t.THREAD_ID)
WHERE t.PROCESSLIST_ID = CONNECTION_ID()
  AND COUNT_STAR > 0
ORDER BY EVENT_NAME)");
}

static stdx::expected<std::vector<std::tuple<std::string, std::string>>,
                      MysqlError>
statement_history(MysqlClient &cli, bool to_read_write) {
  {
    auto set_res =
        cli.query(to_read_write ? "ROUTER SET access_mode='read_write'"
                                : "ROUTER SET access_mode='read_only'");
    if (!set_res) return stdx::unexpected(set_res.error());
  }

  auto hist_res = query_one_result(
      cli,
      "SELECT event_name, digest_text "
      "  FROM performance_schema.events_statements_history AS h"
      "  JOIN performance_schema.threads AS t "
      "    ON (h.thread_id = t.thread_id)"
      " WHERE t.processlist_id = CONNECTION_ID()"
      " ORDER BY event_id");

  {
    auto set_res = cli.query("ROUTER SET access_mode='auto'");
    if (!set_res) return stdx::unexpected(set_res.error());
  }

  std::vector<std::tuple<std::string, std::string>> res;

  for (auto row : *hist_res) {
    res.emplace_back(row[0], row[1]);
  }

  return res;
}

static stdx::expected<std::vector<std::tuple<std::string, std::string>>,
                      MysqlError>
statement_history_from_read_write(MysqlClient &cli) {
  return statement_history(cli, true);
}

static stdx::expected<std::vector<std::tuple<std::string, std::string>>,
                      MysqlError>
statement_history_from_read_only(MysqlClient &cli) {
  return statement_history(cli, false);
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

    return testing::AssertionFailure() << "Value of: " << pointer_sb.GetString()
                                       << ", Actual: " << lhs_sb.GetString()
                                       << " Expected: " << rhs_sb.GetString();
  }

  return testing::AssertionSuccess();
}

struct SplittingConnectionParam {
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

const SplittingConnectionParam share_connection_params[] = {
    // DISABLED
    {
        "DISABLED__DISABLED",
        kDisabled,  // client_ssl_mode
        kDisabled,  // server_ssl_mode
    },
    {
        "DISABLED__REQUIRED",
        kDisabled,
        kRequired,
    },

    // PREFERRED
    {
        "PREFERRED__DISABLED",
        kPreferred,
        kDisabled,
    },
    {
        "PREFERRED__PREFERRED",
        kPreferred,
        kPreferred,
    },

    // all other combinates are somewhat redundant.
};

#ifdef _WIN32
#define SO_EXTENSION ".dll"
#else
#define SO_EXTENSION ".so"
#endif

/* test environment.
 *
 * spawns servers for the tests.
 */
class TestEnv : public ::testing::Environment {
 public:
  constexpr const static std::string_view cluster_id =
      "4abd4148-eb35-11ed-9423-1cfd0870a5a9";

  static std::vector<std::string> gr_node_init_stmts(
      const std::vector<std::string> &seeds) {
    std::vector<std::string> stmts{{
        "INSTALL PLUGIN `group_replication`"
        "  SONAME 'group_replication" SO_EXTENSION "'",

        "SET SESSION sql_log_bin = 0",

        "CREATE USER IF NOT EXISTS 'gr_user' "
        "  IDENTIFIED BY 'gr_pass' "
        "  REQUIRE NONE PASSWORD EXPIRE NEVER",

        "GRANT REPLICATION SLAVE, BACKUP_ADMIN, "
        "  GROUP_REPLICATION_STREAM, CONNECTION_ADMIN "
        "  ON *.* TO 'gr_user'@'%'",

        "SET SESSION sql_log_bin = 1",
    }};
    std::vector<std::pair<std::string, std::string>> vars{
        {"super_read_only", "'ON'"},
        // {"group_replication_paxos_single_leader", "DEFAULT"},
        {"group_replication_group_name", "'" + std::string(cluster_id) + "'"},
        // {"group_replication_enforce_update_everywhere_checks", "'OFF'"},
        // {"group_replication_single_primary_mode", "'ON'"},
        // {"group_replication_recovery_use_ssl", "'ON'"},
        // {"group_replication_recovery_ssl_verify_server_cert", "'OFF'"},
        // {"group_replication_ssl_mode", "'REQUIRED'"},
        {"group_replication_local_address", "CONCAT('127.0.0.1:', @@port)"},
        {"group_replication_start_on_boot", "'ON'"},
        {"group_replication_communication_stack", "'MYSQL'"},
        // {"auto_increment_increment", "1"},
        // {"auto_increment_offset", "3"},
    };

    if (!seeds.empty()) {
      vars.emplace_back("group_replication_group_seeds",
                        "'" + mysql_harness::join(seeds, ",") + "'");
    }

    std::string set_persist;
    for (auto [key, val] : vars) {
      if (set_persist.empty()) {
        set_persist = "SET PERSIST ";
      } else {
        set_persist += ", ";
      }
      set_persist += key;
      set_persist += " = ";
      set_persist += val;
    }

    stmts.emplace_back(set_persist);

    if (seeds.empty()) {
      stmts.emplace_back(
          "SET GLOBAL `group_replication_bootstrap_group` = 'ON'");
    }

    stmts.emplace_back(
        "START GROUP_REPLICATION USER='gr_user', PASSWORD='gr_pass'");

    if (seeds.empty()) {
      stmts.emplace_back(
          "SET GLOBAL `group_replication_bootstrap_group` = 'OFF'");
    }

    return stmts;
  }

  void SetUp() override {
    std::vector<std::string> seeds;

    for (auto [ndx, srv] : stdx::views::enumerate(shared_servers_)) {
      if (srv != nullptr) continue;

      srv = new SharedServer(port_pool_);
      srv->prepare_datadir();
      srv->spawn_server({"--server_id", std::to_string(ndx + 1),
                         "--report_host", "127.0.0.1"});

      if (srv->mysqld_failed_to_start()) {
        GTEST_SKIP() << "mysql-server failed to start.";
      }

      seeds.emplace_back(srv->server_host() + ":"s +
                         std::to_string(srv->server_port()));
    }

    for (auto [ndx, srv] : stdx::views::enumerate(shared_servers_)) {
      SCOPED_TRACE("// " + std::to_string(ndx));
      auto cli_res = srv->admin_cli();
      ASSERT_NO_ERROR(cli_res);

      auto cli = std::move(*cli_res);

      for (const auto &stmt :
           gr_node_init_stmts(ndx == 0 ? std::vector<std::string>{} : seeds)) {
        SCOPED_TRACE("// " + std::string(stmt));
        ASSERT_NO_ERROR(cli.query(stmt)) << stmt;
      }
    }

    std::this_thread::sleep_for(1s);

    create_cluster_metadata();
    bootstrap_router();

    run_slow_tests_ = std::getenv("RUN_SLOW_TESTS") != nullptr;
  }

  void create_cluster_metadata() {
    auto *srv = shared_servers_[0];
    // import the dump.
    //
    auto primary_cli_res = srv->admin_cli();
    ASSERT_NO_ERROR(primary_cli_res);
    auto primary_cli = std::move(*primary_cli_res);

    std::stringstream ss;
    {
      std::ifstream ifs(ProcessManager::get_data_dir()
                            .join("metadata-model-2.1.0.sql")
                            .str());

      ASSERT_TRUE(ifs.good());
      ss << ifs.rdbuf();
    }

    auto &proc_mgr = srv->process_manager();
    {
      auto &mysql_proc =
          proc_mgr.spawner(proc_mgr.get_origin().join("mysql").str())
              .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
              .spawn({"--host", "127.0.0.1", "--port",
                      std::to_string(srv->server_port()), "--user", "root",
                      "--password=", "-e",
                      "source " + ProcessManager::get_data_dir()
                                      .join("metadata-model-2.1.0.sql")
                                      .str()});
      ASSERT_NO_THROW(mysql_proc.wait_for_exit(20s))
          << mysql_proc.get_current_output();
      ASSERT_EQ(mysql_proc.exit_code(), 0) << mysql_proc.get_full_output();
    }

    // create a cluster
    ASSERT_NO_ERROR(primary_cli.query(
        "INSERT INTO mysql_innodb_cluster_metadata.clusters ("
        "    cluster_id, cluster_name, description, cluster_type, "
        "    primary_mode, attributes)"
        "  VALUES ("
        "    '" +
        std::string(cluster_id) +
        "', 'main_cluster',"
        "    'Default Cluster', 'gr', 'pm',"
        "    JSON_OBJECT("
        "      'adopted', 0,"
        "      'group_replication_group_name', "
        "        '" +
        std::string(cluster_id) + "'))"));

    for (auto [ndx, srv] : stdx::views::enumerate(shared_servers_)) {
      auto cli_res = srv->admin_cli();
      ASSERT_NO_ERROR(cli_res);

      auto cli = std::move(*cli_res);

      auto query_res =
          query_one_result(cli, "SELECT @@server_uuid, @@server_id");
      ASSERT_NO_ERROR(query_res);
      auto row = (*query_res)[0];
      auto [server_uuid, server_id] = std::make_pair(row[0], row[1]);

      auto server_classic_address =
          srv->server_host() + ":"s + std::to_string(srv->server_port());
      auto server_x_address =
          srv->server_host() + ":"s + std::to_string(srv->server_mysqlx_port());

      // add this instance to the cluster.
      ASSERT_NO_ERROR(primary_cli.query(
          "INSERT INTO mysql_innodb_cluster_metadata.instances ("
          "    cluster_id, address, mysql_server_uuid, instance_name,"
          "    addresses, attributes)"
          "  VALUES ('" +
          std::string(cluster_id) + "', '" + server_classic_address + "', '" +
          server_uuid + "', '" + server_classic_address +
          "', json_object("
          "	   'mysqlClassic', '" +
          server_classic_address +
          "',"
          "      'mysqlX',       '" +
          server_x_address +
          "',"
          "      'grLocal',      '" +
          server_classic_address +
          "'),"
          "    json_object('server_id', " +
          server_id + "))"));
    }
  }

  void bootstrap_router() {
    auto *srv = shared_servers_[0];

    auto &proc_mgr = srv->process_manager();
    auto &bootstrap_proc =
        proc_mgr.spawner(proc_mgr.get_origin().join("mysqlrouter").str())
            .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
            .output_responder([](auto in) {
              if (in.find("Please enter MySQL password for router:") !=
                  std::string::npos) {
                return "foobar\n";
              }
              if (in.find("Please enter MySQL password for root:") !=
                  std::string::npos) {
                return "\n";
              }

              return "";
            })
            .spawn({"--bootstrap",
                    "root@127.0.0.1:" + std::to_string(srv->server_port()),
                    "--account", "router",         //
                    "--report-host", "127.0.0.1",  //
                    "-d", router_dir_.name(),      //
                    "--conf-set-option",
                    "DEFAULT.plugin_folder=" +
                        ProcessManager::get_plugin_dir().str()});
    ASSERT_NO_THROW(bootstrap_proc.wait_for_exit(10s))
        << bootstrap_proc.get_current_output();
    ASSERT_EQ(bootstrap_proc.exit_code(), 0)
        << bootstrap_proc.get_full_output();

    srv->setup_mysqld_accounts();

    // create a table used for insert/update/select.
    auto primary_cli_res = srv->admin_cli();
    ASSERT_NO_ERROR(primary_cli_res);
    auto primary_cli = std::move(*primary_cli_res);

    ASSERT_NO_ERROR(primary_cli.query("CREATE TABLE testing.t1 (id SERIAL)"));
  }

  std::array<SharedServer *, 3> servers() { return shared_servers_; }

  TcpPortPool &port_pool() { return port_pool_; }

  [[nodiscard]] bool run_slow_tests() const { return run_slow_tests_; }

  void TearDown() override {
    if (testing::Test::HasFatalFailure()) {
      for (auto &srv : shared_servers_) {
        srv->process_manager().dump_logs();
      }
    }

    for (auto &srv : shared_servers_) {
      if (srv == nullptr || srv->mysqld_failed_to_start()) continue;

      EXPECT_NO_ERROR(srv->shutdown());
    }

    for (auto &srv : shared_servers_) {
      if (srv != nullptr) delete srv;

      srv = nullptr;
    }

    SharedServer::destroy_statics();
  }

  std::string router_dir() const { return router_dir_.name(); }

 protected:
  TcpPortPool port_pool_;

  std::array<SharedServer *, 3> shared_servers_{};

  bool run_slow_tests_{false};

  TempDirectory router_dir_;
};

TestEnv *test_env{};

class SharedRouter {
 public:
  SharedRouter(TcpPortPool &port_pool, uint64_t pool_size)
      : port_pool_(port_pool),
        pool_size_{pool_size},
        rest_port_{port_pool_.get_next_available()} {}

  integration_tests::Procs &process_manager() { return procs_; }

  void spawn_router() {
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
                                 {"port", std::to_string(rest_port_)}})
        .section("metadata_cache:bootstrap",
                 {
                     {"cluster_type", "gr"},
                     {"router_id", "1"},
                     {"user", "router"},
                     {"metadata_cluster", "main_cluster"},
                 });

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
              {"destinations",
               "metadata-cache://main_cluster/?role=PRIMARY_AND_SECONDARY"},
              {"protocol", "classic"},
              {"routing_strategy", "round-robin"},

              {"client_ssl_mode", std::string(param.client_ssl_mode)},
              {"server_ssl_mode", std::string(param.server_ssl_mode)},

              {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
              {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
              {"connection_sharing", "1"},
              {"access_mode", "auto"},
              {"connection_sharing_delay", "0"},
              {"connect_retry_timeout", "0"},
          });
    }

    auto bootstrap_dir = mysql_harness::Path(test_env->router_dir());

    auto &default_section = writer.sections()["DEFAULT"];
    default_section["keyring_path"] =
        bootstrap_dir.join("data").join("keyring").str();
    default_section["master_key_path"] =
        bootstrap_dir.join("mysqlrouter.key").str();
    default_section["dynamic_state"] =
        bootstrap_dir.join("data").join("state.json").str();
    default_section["unknown_config_option"] = "error";

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
    }
  }

  [[nodiscard]] auto host() const { return router_host_; }

  [[nodiscard]] uint16_t port(const SplittingConnectionParam &param) const {
    return ports_.at(
        std::make_tuple(param.client_ssl_mode, param.server_ssl_mode));
  }

  [[nodiscard]] auto rest_port() const { return rest_port_; }
  [[nodiscard]] auto rest_user() const { return rest_user_; }
  [[nodiscard]] auto rest_pass() const { return rest_pass_; }

  void populate_connection_pool(const SplittingConnectionParam &param) {
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
          std::min(num_destinations, pool_size_), 1s));
    }
  }

  stdx::expected<int, std::error_code> rest_get_int(
      const std::string &uri, const std::string &pointer) {
    JsonDocument json_doc;

    fetch_json(rest_client_, uri, json_doc);

    if (auto *val = JsonPointer(pointer).Get(json_doc)) {
      if (!val->IsInt()) {
        return stdx::unexpected(make_error_code(std::errc::invalid_argument));
      }
      return val->GetInt();
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
      shared_router_->spawn_router();
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

class SplittingConnectionTestBase : public RouterComponentTest {
 public:
  static constexpr const size_t kNumServers = 3;
  static constexpr const size_t kMaxPoolSize = 128;

  static void SetUpTestSuite() {
    for (const auto &srv : shared_servers()) {
      if (srv->mysqld_failed_to_start()) GTEST_SKIP();
    }

    TestWithSharedRouter::SetUpTestSuite(test_env->port_pool(),
                                         shared_servers(), kMaxPoolSize);
  }

  static void TearDownTestSuite() { TestWithSharedRouter::TearDownTestSuite(); }

  static std::array<SharedServer *, kNumServers> shared_servers() {
    std::array<SharedServer *, kNumServers> srvs;

    // get a subset of the started servers
    for (auto [ndx, srv] : stdx::views::enumerate(test_env->servers())) {
      if (ndx >= kNumServers) break;

      srvs[ndx] = srv;
    }

    return srvs;
  }

  static SharedRouter *shared_router() {
    return TestWithSharedRouter::router();
  }
};

class SplittingConnectionTest
    : public SplittingConnectionTestBase,
      public ::testing::WithParamInterface<SplittingConnectionParam> {
 public:
  void SetUp() override {
    for (auto &srv : shared_servers()) {
      // shared_server_ may be null if TestWithSharedServer::SetUpTestSuite
      // threw?
      if (srv == nullptr || srv->mysqld_failed_to_start()) {
        GTEST_SKIP() << "failed to start mysqld";
      } else {
        srv->close_all_connections();  // reset the router's connection-pool
      }
    }
  }

  void TearDown() override {
    if (HasFatalFailure()) {
      shared_router()->process_manager().dump_logs();
    }
  }

  const std::string stmt_type_sql_select{"statement/sql/select"};
  const std::string stmt_type_sql_set_option{"statement/sql/set_option"};
  const std::string stmt_type_sql_insert{"statement/sql/insert"};
  const std::string stmt_type_sql_truncate{"statement/sql/truncate"};
  const std::string stmt_type_com_reset_connection{
      "statement/com/Reset Connection"};
  const std::string stmt_type_com_set_option{"statement/com/Set option"};

  const std::string stmt_select_session_vars{
      "SELECT ? , @@SESSION . `collation_connection` UNION "
      "SELECT ? , @@SESSION . `character_set_client` UNION "
      "SELECT ? , @@SESSION . `sql_mode`"};
  const std::string stmt_set_session_tracker{
      "SET "
      "@@SESSION . `session_track_system_variables` = ? , "
      "@@SESSION . `session_track_gtids` = ? , "
      "@@SESSION . `session_track_schema` = ? , "
      "@@SESSION . `session_track_state_change` = ? , "
      "@@SESSION . `session_track_transaction_info` = ?"};

  const std::string stmt_restore_session_vars{
      "SET "
      "@@SESSION . `session_track_system_variables` = ? , "
      "@@SESSION . `character_set_client` = ? , "
      "@@SESSION . `collation_connection` = ? , "
      "@@SESSION . `session_track_gtids` = ? , "
      "@@SESSION . `session_track_schema` = ? , "
      "@@SESSION . `session_track_state_change` = ? , "
      "@@SESSION . `session_track_transaction_info` = ? , "
      "@@SESSION . `sql_mode` = ?"};

  const std::string stmt_select_history{
      "SELECT `event_name` , `digest_text` "
      "FROM `performance_schema` . `events_statements_history` AS `h` "
      "JOIN `performance_schema` . `threads` AS `t` "
      "ON ( `h` . `thread_id` = `t` . `thread_id` ) "
      "WHERE `t` . `processlist_id` = `CONNECTION_ID` ( ) "
      "ORDER BY `event_id`"};

  const std::string stmt_select_wait_gtid{
      "SELECT NOT `WAIT_FOR_EXECUTED_GTID_SET` (...)"};
};

/**
 * check connections can be shared after the connection is established.
 *
 * - connect
 * - wait for connection be pooled
 * - connect a 2nd connection to same backend
 * - check they share the same connection
 */
TEST_P(SplittingConnectionTest, select_and_insert) {
  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // connection goes out of the pool and back to the pool again.
  ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(1, 1s));

  std::string primary_port;

  {
    auto query_res = query_one_result(
        cli, "SELECT * FROM performance_schema.replication_group_members");
    ASSERT_NO_ERROR(query_res);

    // 3 nodes
    // - a PRIMARY and 2 SECONDARY
    // - all ONLINE
    EXPECT_THAT(
        *query_res,
        UnorderedElementsAre(
            ElementsAre("group_replication_applier", testing::_, "127.0.0.1",
                        testing::_, "ONLINE", "PRIMARY", testing::_, "MySQL"),
            ElementsAre("group_replication_applier", testing::_, "127.0.0.1",
                        testing::_, "ONLINE", "SECONDARY", testing::_, "MySQL"),
            ElementsAre("group_replication_applier", testing::_, "127.0.0.1",
                        testing::_, "ONLINE", "SECONDARY", testing::_,
                        "MySQL")));

    // find the port of the current PRIMARY.
    for (auto const &row : *query_res) {
      if (row[5] == "PRIMARY") primary_port = row[3];
    }
  }
  ASSERT_THAT(primary_port, ::testing::Not(::testing::IsEmpty()));

  // enable tracing to detect if the query went to the primary or secondary.
  ASSERT_NO_ERROR(cli.query("ROUTER SET trace = 1"));

  SCOPED_TRACE("// clean up from earlier runs");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  {
    auto query_res = query_one_result(cli, "SHOW WARNINGS");
    ASSERT_NO_ERROR(query_res);
    ASSERT_THAT(*query_res, ElementsAre(::testing::SizeIs(3)));

    auto json_trace = query_res->operator[](0)[2];

    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());

    for (const auto &[pntr, val] : {
             std::pair{"/name", rapidjson::Value("mysql/query")},
             std::pair{"/attributes/mysql.sharing_blocked",
                       rapidjson::Value(false)},
             std::pair{"/events/0/name",
                       rapidjson::Value("mysql/query_classify")},
             std::pair{"/events/0/attributes/mysql.query.classification",
                       rapidjson::Value("accept_session_state_from_"
                                        "session_tracker")},
             std::pair{"/events/1/name",
                       rapidjson::Value("mysql/connect_and_forward")},
             std::pair{"/events/1/attributes/mysql.remote.is_connected",
                       rapidjson::Value(false)},
         }) {
      ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
          << json_trace;
    }
  }

  SCOPED_TRACE("// INSERT on PRIMARY");
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));
  {
    auto query_res = query_one_result(cli, "SHOW WARNINGS");
    ASSERT_NO_ERROR(query_res);
    ASSERT_THAT(*query_res, ElementsAre(::testing::SizeIs(3)));

    auto json_trace = query_res->operator[](0)[2];

    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());

    for (const auto &[pntr, val] : {
             std::pair{"/name", rapidjson::Value("mysql/query")},
             std::pair{"/attributes/mysql.sharing_blocked",
                       rapidjson::Value(false)},
             std::pair{"/events/0/name",
                       rapidjson::Value("mysql/query_classify")},
             std::pair{"/events/0/attributes/mysql.query.classification",
                       rapidjson::Value("accept_session_state_from_"
                                        "session_tracker")},
             std::pair{"/events/1/name",
                       rapidjson::Value("mysql/connect_and_forward")},
             std::pair{"/events/1/attributes/mysql.remote.is_connected",
                       rapidjson::Value(false)},
             std::pair{"/events/1/events/0/name",
                       rapidjson::Value("mysql/prepare_server_connection")},
             std::pair{"/events/1/events/0/events/0/name",
                       rapidjson::Value("mysql/from_stash")},
             std::pair{"/events/1/events/0/events/0/attributes/"
                       "mysql.remote.is_connected",
                       rapidjson::Value(true)},
             std::pair{"/events/1/events/0/events/0/attributes/"
                       "mysql.remote.endpoint",
                       rapidjson::Value("127.0.0.1:" + primary_port,
                                        doc.GetAllocator())},
             std::pair{"/events/1/events/0/events/0/attributes/"
                       "db.name",
                       rapidjson::Value("")},
         }) {
      ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
          << json_trace;
    }
  }

  SCOPED_TRACE("// switch schema");
  ASSERT_NO_ERROR(cli.query("USE testing"));

  SCOPED_TRACE(
      "// SELECT COUNT(): check schema-change is propagated, check the INSERT "
      "was replicated.");
  {
    auto query_res = query_one_result(cli, "SELECT COUNT(*) FROM t1");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, ElementsAre(ElementsAre("1")));
  }

  SCOPED_TRACE("// get trace for SELECT COUNT");
  {
    auto query_res = query_one_result(cli, "SHOW WARNINGS");
    ASSERT_NO_ERROR(query_res);
    ASSERT_THAT(*query_res, ElementsAre(::testing::SizeIs(3)));

    auto json_trace = query_res->operator[](0)[2];

    rapidjson::Document doc;
    doc.Parse(json_trace.data(), json_trace.size());

    for (const auto &[pntr, val] : {
             std::pair{"/name", rapidjson::Value("mysql/query")},
             std::pair{"/attributes/mysql.sharing_blocked",
                       rapidjson::Value(false)},
             std::pair{"/events/0/name",
                       rapidjson::Value("mysql/query_classify")},
             std::pair{"/events/0/attributes/mysql.query.classification",
                       rapidjson::Value("accept_session_state_from_"
                                        "session_tracker,read-only")},
             std::pair{"/events/1/name",
                       rapidjson::Value("mysql/connect_and_forward")},
             std::pair{"/events/1/attributes/mysql.remote.is_connected",
                       rapidjson::Value(false)},
             std::pair{"/events/1/events/0/name",
                       rapidjson::Value("mysql/prepare_server_connection")},
             std::pair{"/events/1/events/0/events/0/name",
                       rapidjson::Value("mysql/from_stash")},
             std::pair{"/events/1/events/0/events/0/attributes/"
                       "mysql.remote.is_connected",
                       rapidjson::Value(true)},
             // std::pair{"/events/1/events/0/events/0/events/0/attributes/"
             //           "mysql.remote.endpoint",
             //           rapidjson::Value("")},
             std::pair{"/events/1/events/0/events/0/attributes/"
                       "db.name",
                       rapidjson::Value("testing")},
         }) {
      // 11010 [the SECONDARY]
      ASSERT_TRUE(json_pointer_eq(doc, rapidjson::Pointer(pntr), val))
          << json_trace;
    }
  }
}

TEST_P(SplittingConnectionTest, prepare_fails_if_locked_on_read_only) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR6.1");
  RecordProperty(
      "Requirement",
      "If the session's access_mode is 'auto' and a transaction is "
      "active on a read only server, prepared statements MUST fail.");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// clean up from earlier runs");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  // announce that the following statements are for the secondary.
  ASSERT_NO_ERROR(cli.query("START TRANSACTION READ ONLY"));

  // select something to make the transaction actually "open".
  {
    auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(query_res);
  }

  // prepare should fail as sharing isn't allowed.
  {
    auto prep_res = cli.prepare("SELECT 1");
    ASSERT_ERROR(prep_res);

    EXPECT_EQ(prep_res.error().value(), 1064) << prep_res.error();
  }
}

TEST_P(SplittingConnectionTest, prepare_succeeds_if_locked_on_read_write) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR6.2");
  RecordProperty("Requirement",
                 "If the session's access_mode is 'auto', prepared statements "
                 "MUST be targeted at a read-write server.");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // primary
  SCOPED_TRACE("// clean up from earlier runs");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  // switch to secondary
  {
    auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(query_res);
  }

  // primary again
  ASSERT_NO_ERROR(cli.query("START TRANSACTION"));

  // SELECT something to make the transaction actually "open".
  // (START TRANSACTION doesn't open a transaction, but SELECT-after-START
  // does.)
  {
    auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(query_res);
  }

  // prepare should succeed as this is on the PRIMARY.
  {
    auto prep_res = cli.prepare("INSERT INTO testing.t1 VALUES ()");
    ASSERT_NO_ERROR(prep_res);

    ASSERT_NO_ERROR(prep_res->execute());
  }
}

TEST_P(SplittingConnectionTest,
       prepare_to_read_only_if_access_mode_is_read_only) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR6.3");
  RecordProperty("Requirement",
                 "If the session's access_mode is 'read_only', the prepared "
                 "statement MUST be targeted at a read-only server.");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // primary
  SCOPED_TRACE("// clean up from earlier runs");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  // switch to secondary
  {
    auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(query_res);
  }

  // force secondary
  ASSERT_NO_ERROR(cli.query("ROUTER SET access_mode='read_only'"));

  // prepare should succeed as it is forced on the read-only server.
  //
  // execute should fail as the INSERT fails with --super-read-only
  {
    auto prep_res = cli.prepare("INSERT INTO testing.t1 VALUES ()");
    ASSERT_NO_ERROR(prep_res);

    auto exec_res = prep_res->execute();
    ASSERT_ERROR(exec_res);

    // The MySQL server is running with the --read-only option so it cannot
    // execute this statement
    EXPECT_EQ(exec_res.error().value(), 1290) << exec_res.error();
  }
}

TEST_P(SplittingConnectionTest,
       prepare_to_read_write_if_access_mode_is_read_write) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR6.4");
  RecordProperty("Requirement",
                 "If the session's access_mode is 'read_write', the prepared "
                 "statement MUST be targeted at a read-write server.");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // primary
  SCOPED_TRACE("// clean up from earlier runs");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  // switch to secondary
  {
    auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(query_res);
  }

  // force primary
  ASSERT_NO_ERROR(cli.query("ROUTER SET access_mode='read_write'"));

  // prepare and executed should succeed as it is forced on a read-write server.
  //
  {
    auto prep_res = cli.prepare("INSERT INTO testing.t1 VALUES ()");
    ASSERT_NO_ERROR(prep_res);

    ASSERT_NO_ERROR(prep_res->execute());
  }
}

TEST_P(SplittingConnectionTest, prepare_instance_local_statements_fails) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR6.5");
  RecordProperty(
      "Requirement",
      "If access_mode is `auto` and a instance local statement is prepared, "
      "the prepare MUST fail.");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // primary
  SCOPED_TRACE("// clean up from earlier runs");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  for (const auto &stmt : {
           "ALTER SERVER",
           "CREATE SERVER",
           "DROP SERVER",
           "LOCK TABLES testing.t1 READ",
           "SHUTDOWN",
           "START GROUP_REPLICATION",
           "START REPLICA",
           "STOP GROUP_REPLICATION",
           "STOP REPLICA",
           "UNLOCK TABLES",
       }) {
    SCOPED_TRACE(stmt);

    auto stmt_res = cli.prepare(stmt);
    ASSERT_ERROR(stmt_res);
    // Statement not allowed if access_mode is 'auto'
    EXPECT_EQ(stmt_res.error().value(), 4501) << stmt_res.error();
  }
}

TEST_P(SplittingConnectionTest,
       explicitly_commit_statements_that_commit_implicitly_read_only) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR2.11");
  RecordProperty(
      "Requirement",
      "If connection-sharing is possible and the session's "
      "`access_mode` is `auto` and a statement is received which "
      "would implicitly commit a transaction, Router MUST explicitly commit "
      "the transaction before forwarding the received statement.");
  RecordProperty("Description", "COMMIT read-only trx before TRUNCATE");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // primary
  SCOPED_TRACE("// clean up from earlier runs");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  ASSERT_NO_ERROR(cli.query("START TRANSACTION READ ONLY"));

  // switch to the secondary.
  {
    auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(query_res);
  }

  // should inject a commit and switch to the primary.
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));
}

stdx::expected<std::string, MysqlError> executed_gtid(MysqlClient &cli) {
  auto query_res = query_one_result(cli, "SELECT @@gtid_executed");
  if (!query_res) return stdx::unexpected(query_res.error());

  if ((*query_res).size() != 1) {
    return stdx::unexpected(MysqlError{2013, "expected a row", "HY000"});
  }
  if ((*query_res)[0].size() != 1) {
    return stdx::unexpected(MysqlError{2013, "expected one column", "HY000"});
  }

  return ((*query_res)[0][0]);
}

TEST_P(SplittingConnectionTest,
       explicitly_commit_statements_that_commit_implicitly_read_write) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR2.11");
  RecordProperty(
      "Requirement",
      "If connection-sharing is possible and the session's "
      "`access_mode` is `auto` and a statement is received which "
      "would implicitly commit a transaction, Router MUST explicitly commit "
      "the transaction before forwarding the received statement.");
  RecordProperty("Description", "COMMIT read-write trx before DROP");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto last_gtid = [](MysqlClient &cli) -> std::string {
    for (auto [key, val] : cli.session_trackers()) {
      if (key == SESSION_TRACK_GTIDS) {
        return std::string(val);
      }
    }

    return {};
  };

  // primary
  SCOPED_TRACE("// clean up from earlier runs");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));
  auto last_known_gtid = last_gtid(cli);
  EXPECT_THAT(last_known_gtid, Not(testing::IsEmpty()));

  // stay on the primary.
  ASSERT_NO_ERROR(cli.query("START TRANSACTION READ WRITE"));
  EXPECT_THAT(last_gtid(cli), IsEmpty());  // no new gtid.

  {
    auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, IsEmpty());
  }
  EXPECT_THAT(last_gtid(cli), IsEmpty());  // no new gtid.

  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));
  EXPECT_THAT(last_gtid(cli), IsEmpty());  // no new gtid.

  // should inject a commit and stay on the primary.
  //
  // ... and fail as the table does not exist.
  ASSERT_ERROR(cli.query("DROP TABLE testing.does_not_exist"));

  // stay on the primary.
  ASSERT_NO_ERROR(cli.query("START TRANSACTION READ WRITE"));
  {
    auto last_executed_gtid_res = executed_gtid(cli);
    ASSERT_NO_ERROR(last_executed_gtid_res);
    EXPECT_NE(last_known_gtid, *last_executed_gtid_res);
    last_known_gtid = *last_executed_gtid_res;
  }
  ASSERT_NO_ERROR(cli.query("ROLLBACK"));  // no need to commit.

  {
    auto last_executed_gtid_res = executed_gtid(cli);
    ASSERT_NO_ERROR(last_executed_gtid_res);
    EXPECT_EQ(last_known_gtid, *last_executed_gtid_res);
  }

  // switches the read-only server and waits for the implicitly committed trx.
  {
    auto query_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(query_res);
    EXPECT_THAT(*query_res, testing::SizeIs(1));
  }
}

TEST_P(SplittingConnectionTest, reset_connection_resets_last_executed_gtid) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR7.1");
  RecordProperty("Requirement",
                 "If access_mode is 'auto' and the client sends a "
                 "reset-connection, Router MUST reset the last executed");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  SCOPED_TRACE("// connect");

  // primary or secondary
  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  std::vector<std::tuple<std::string, std::string>> initial_expected_stmts{
      {stmt_type_sql_set_option, stmt_set_session_tracker},
      {stmt_type_sql_select, stmt_select_session_vars},
  };

  std::vector<std::tuple<std::string, std::string>> switched_expected_stmts{
      {stmt_type_sql_set_option, stmt_restore_session_vars},
  };

  std::vector<std::tuple<std::string, std::string>> rw_expected_stmts;
  std::vector<std::tuple<std::string, std::string>> ro_expected_stmts;
  bool started_on_rw{false};

  {
    auto stmt_hist_res = statement_history_from_read_write(cli);
    ASSERT_NO_ERROR(stmt_hist_res);

    // detect if router started on a RW or RO node.
    started_on_rw = stmt_hist_res->size() == 2;

    if (started_on_rw) {
      rw_expected_stmts = initial_expected_stmts;
      ro_expected_stmts = switched_expected_stmts;
    } else {
      ro_expected_stmts = initial_expected_stmts;
      rw_expected_stmts = switched_expected_stmts;
    }

    EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(rw_expected_stmts));
    rw_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_history);
  }

  {
    auto stmt_hist_res = statement_history_from_read_only(cli);
    ASSERT_NO_ERROR(stmt_hist_res);

    EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(ro_expected_stmts));

    ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_history);
  }

  ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(2, 10s));

  // primary
  SCOPED_TRACE("// cleanup");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  rw_expected_stmts.emplace_back(stmt_type_sql_truncate,
                                 "TRUNCATE TABLE `testing` . `t1`");

  // primary
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  rw_expected_stmts.emplace_back(stmt_type_sql_insert,
                                 "INSERT INTO `testing` . `t1` VALUES ( )");

  {
    auto stmt_hist_res = statement_history_from_read_write(cli);
    ASSERT_NO_ERROR(stmt_hist_res);

    EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(rw_expected_stmts));

    rw_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_history);
  }

  // secondary
  //
  // Router should wait for GTID_EXECUTED.
  ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_wait_gtid);

  {
    auto stmt_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(stmt_res);

    ro_expected_stmts.emplace_back(stmt_type_sql_select,
                                   "SELECT * FROM `testing` . `t1`");
  }

  ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_wait_gtid);

  {
    auto stmt_hist_res = statement_history_from_read_only(cli);
    ASSERT_NO_ERROR(stmt_hist_res);

    EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(ro_expected_stmts));

    ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_history);
  }

  // the RO and RW connections should be stashed now.
  ASSERT_NO_ERROR(shared_router()->wait_for_stashed_server_connections(2, 10s));

  ASSERT_NO_ERROR(cli.reset_connection());

  // reset-connection should also reset the last-executed GTID of the current
  // client-side session. -> no select_wait_gtid query.
  ro_expected_stmts.emplace_back(stmt_type_com_reset_connection, "<NULL>");
  ro_expected_stmts.emplace_back(stmt_type_sql_set_option,
                                 stmt_set_session_tracker);
  ro_expected_stmts.emplace_back(stmt_type_sql_select,
                                 stmt_select_session_vars);

  {
    auto stmt_hist_res = statement_history_from_read_only(cli);
    ASSERT_NO_ERROR(stmt_hist_res);

    EXPECT_THAT(*stmt_hist_res,
                ::testing::ElementsAreArray(
                    std::span(ro_expected_stmts)
                        .last(std::min(ro_expected_stmts.size(), size_t{10}))));

    ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_history);
  }

  rw_expected_stmts.emplace_back(stmt_type_com_reset_connection, "<NULL>");
  rw_expected_stmts.emplace_back(stmt_type_sql_set_option,
                                 stmt_restore_session_vars);

  // primary
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  rw_expected_stmts.emplace_back(stmt_type_sql_insert,
                                 "INSERT INTO `testing` . `t1` VALUES ( )");
  {
    auto stmt_hist_res = statement_history_from_read_write(cli);
    ASSERT_NO_ERROR(stmt_hist_res);

    EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(rw_expected_stmts));

    rw_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_history);
  }
}

TEST_P(SplittingConnectionTest, reset_connection_resets_session_access_mode) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR7.2");
  RecordProperty("Requirement",
                 "If `access_mode` is 'auto' and the client sends a "
                 "reset-connection, Router MUST reset the session's "
                 "`access_mode` to 'auto'");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // primary

  SCOPED_TRACE("// cleanup");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  // force INSERT to go to the secondary.
  ASSERT_NO_ERROR(cli.query("ROUTER SET access_mode='read_only'"));

  // secondary.
  {
    auto stmt_res = cli.query("INSERT INTO testing.t1 VALUES ()");
    ASSERT_ERROR(stmt_res);
    // The MySQL server is running with the --read-only option so it cannot
    // execute this statement
    EXPECT_EQ(stmt_res.error().value(), 1290) << stmt_res.error();
  }

  // set the access_mode to 'auto'
  ASSERT_NO_ERROR(cli.reset_connection());

  // primary
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));
}

TEST_P(SplittingConnectionTest,
       reset_connection_resets_session_wait_for_my_writes) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR7.3");
  RecordProperty("Requirement",
                 "If `access_mode` is 'auto' and the client sends a "
                 "reset-connection, Router MUST reset the session's "
                 "`wait_for_my_writes`");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // force INSERT to go to the secondary.
  ASSERT_NO_ERROR(cli.query("ROUTER SET wait_for_my_writes=0"));

  // primary

  SCOPED_TRACE("// cleanup");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  // primary
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  // secondary, does not wait for executed gtid.
  //
  // executed on the secondary:
  //
  // - reset-connection
  // - SET trackers
  // - SELECT @@super_read_only
  // - [no SELECT NOT WAIT_FOR_GTID...]
  // - SELECT * FROM testing...
  {
    auto stmt_res = query_one_result(cli, "SELECT * FROM testing.t1");

    // either succeeds or fails as the the table doesn't exist yet.
    if (!stmt_res) {
      // Table testing.t1 does not exist.
      EXPECT_EQ(stmt_res.error().value(), 1146) << stmt_res.error();
    } else {
      ASSERT_NO_ERROR(stmt_res);
      // row may exist or not.
      EXPECT_THAT(*stmt_res, testing::SizeIs(testing::Le(1)));
    }
  }

  // executed on the secondary:
  //
  // - reset-connection (from pool)
  // - SET trackers
  // - SELECT @@super_read_only
  // - [no SELECT NOT WAIT_FOR_GTID...]
  // - SELECT * FROM performance_schema... [not seen by this query]
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                AnyOf(
                    // started on read-write
                    ElementsAre(Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)),
                    // started on read-write and table didn't exist yet.
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/select", 3),
                                Pair("statement/sql/set_option", 2),
                                Pair("statement/sql/show_warnings", 1)),
                    // started on read-only
                    ElementsAre(Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 1)),
                    // start on read-only and table didn't exist yet.
                    ElementsAre(Pair("statement/com/Reset Connection", 2),
                                Pair("statement/sql/select", 5),
                                Pair("statement/sql/set_option", 3),
                                Pair("statement/sql/show_warnings", 1))));
  }

  // reset sets the wait_for_my_writes to '1'
  ASSERT_NO_ERROR(cli.reset_connection());

  // primary
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  // secondary, waits for executed gtid.
  //
  // executed on the secondary:
  //
  // - reset-connection
  // - SET trackers
  // - SELECT GTID...
  // - SELECT * FROM testing...
  {
    auto stmt_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(stmt_res);
    EXPECT_THAT(*stmt_res, testing::SizeIs(2));
  }

  // executed on the secondary:
  //
  // - reset-connection (from pool)
  // - SET trackers
  // - SELECT GTID...
  // - SELECT * FROM performance_schema... [not seen by this query]
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                AnyOf(
                    // started on read-write
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/select", 6),
                                Pair("statement/sql/set_option", 2)),
                    // started on read-write and table didn't exist yet.
                    ElementsAre(Pair("statement/com/Reset Connection", 5),
                                Pair("statement/sql/select", 10),
                                Pair("statement/sql/set_option", 6),
                                Pair("statement/sql/show_warnings", 1)),
                    // start on read-only
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/select", 7),
                                Pair("statement/sql/set_option", 2)),
                    // start on read-only and table didn't exist yet
                    ElementsAre(Pair("statement/com/Reset Connection", 6),
                                Pair("statement/sql/select", 12),
                                Pair("statement/sql/set_option", 7),
                                Pair("statement/sql/show_warnings", 1))));
  }
}

TEST_P(SplittingConnectionTest,
       reset_connection_targets_the_current_destination) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR7.5");
  RecordProperty(
      "Requirement",
      "If `access_mode` is 'auto' and the client sends a "
      "reset-connection, Router MUST target the current destination");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  // may start on the primary or secondary.
  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // a noop statement which switches to the primary.
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  SCOPED_TRACE("// reset to primary");
  ASSERT_NO_ERROR(cli.reset_connection());

  // executed on the secondary:
  //
  // - SET trackers
  // - SELECT * FROM performance_schema... [not seen by this query]
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                AnyOf(
                    // started on read-write
                    ElementsAre(Pair("statement/sql/set_option", 1)),
                    // started on read-only
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 2))));
  }

  SCOPED_TRACE("// reset to secondary");
  ASSERT_NO_ERROR(cli.reset_connection());

  // executed on the secondary:
  //
  // - SET trackers
  // - SELECT * FROM performance_schema... [not seen by this query]
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                AnyOf(
                    // started on read-write
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 2)),
                    // started on read-only
                    ElementsAre(Pair("statement/com/Reset Connection", 2),
                                Pair("statement/sql/select", 3),
                                Pair("statement/sql/set_option", 3))));
  }
}

TEST_P(SplittingConnectionTest, change_user_resets_session_wait_for_my_writes) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR8.1");
  RecordProperty("Requirement",
                 "If `access_mode` is 'auto' and the client sends a "
                 "change-user, Router MUST reset the session's "
                 "`wait_for_my_writes`");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // force INSERT to go to the secondary.
  ASSERT_NO_ERROR(cli.query("ROUTER SET wait_for_my_writes=0"));

  // primary

  SCOPED_TRACE("// cleanup");
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  // primary
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  // secondary, does not wait for executed gtid.
  //
  // executed on the secondary:
  //
  // - reset-connection
  // - SET trackers
  // - SELECT @@super_read_only
  // - [no SELECT NOT WAIT_FOR_GTID...]
  // - SELECT * FROM testing...
  {
    auto stmt_res = query_one_result(cli, "SELECT * FROM testing.t1");
    // either succeeds or fails as the the table doesn't exist yet.
    if (!stmt_res) {
      // Table testing.t1 does not exist.
      EXPECT_EQ(stmt_res.error().value(), 1146) << stmt_res.error();
    } else {
      ASSERT_NO_ERROR(stmt_res);
      // row may exist or not.
      EXPECT_THAT(*stmt_res, testing::SizeIs(testing::Le(1)));
    }
  }

  // executed on the secondary:
  //
  // - reset-connection (from pool)
  // - SET trackers
  // - SELECT @@super_read_only
  // - [no SELECT NOT WAIT_FOR_GTID...]
  // - SELECT * FROM performance_schema... [not seen by this query]
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                AnyOf(
                    // started on read-write
                    ElementsAre(Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)),
                    // started on read-only
                    ElementsAre(Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 1))));
  }

  auto change_user_account =
      SharedServer::caching_sha2_empty_password_account();

  // change-user sets the wait_for_my_writes to '1'
  ASSERT_NO_ERROR(cli.change_user(change_user_account.username,
                                  change_user_account.password, ""));

  // primary
  ASSERT_NO_ERROR(cli.query("INSERT INTO testing.t1 VALUES ()"));

  // secondary, waits for executed gtid.
  //
  // executed on the secondary:
  //
  // - reset-connection
  // - SET trackers
  // - SELECT GTID...
  // - SELECT * FROM testing...
  {
    auto stmt_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(stmt_res);
  }

  // executed on the secondary:
  //
  // - reset-connection (from pool)
  // - SET trackers
  // - SELECT GTID...
  // - SELECT * FROM performance_schema... [not seen by this query]
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                AnyOf(
                    // started on read-write
                    ElementsAre(Pair("statement/com/Change user", 1),
                                Pair("statement/sql/select", 5),
                                Pair("statement/sql/set_option", 2)),
                    // started on read-only
                    ElementsAre(Pair("statement/com/Change user", 1),
                                Pair("statement/sql/select", 6),
                                Pair("statement/sql/set_option", 2))));
  }
}

TEST_P(SplittingConnectionTest, change_user_targets_the_current_destination) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR8.2");
  RecordProperty("Requirement",
                 "If `access_mode` is 'auto' and the client sends a "
                 "change-user, Router MUST target the current destination");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();
  auto change_user_account = SharedServer::native_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  // may start on the primary or secondary.
  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // a noop statement which switches to the primary.
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  SCOPED_TRACE("// change-user to primary");

  ASSERT_NO_ERROR(cli.change_user(change_user_account.username,
                                  change_user_account.password, ""));

  // executed on the secondary:
  //
  // - SET trackers
  // - SELECT * FROM performance_schema... [not seen by this query]
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                AnyOf(
                    // started on read-write
                    ElementsAre(Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)),
                    // started on read-only
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 2))));
  }

  SCOPED_TRACE("// change-user to secondary");
  ASSERT_NO_ERROR(cli.change_user(change_user_account.username,
                                  change_user_account.password, ""));

  // executed on the secondary:
  //
  // - SET trackers
  // - SELECT * FROM performance_schema... [not seen by this query]
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                AnyOf(
                    // started on read-write
                    ElementsAre(Pair("statement/com/Change user", 1),
                                Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 2)),
                    // started on read-only
                    ElementsAre(Pair("statement/com/Change user", 1),
                                Pair("statement/com/Reset Connection", 2),
                                Pair("statement/sql/select", 4),
                                Pair("statement/sql/set_option", 4))));
  }

  {
    ASSERT_NO_ERROR(cli.query("ROUTER SET access_mode='read_write'"));

    {
      auto user_res = query_one_result(cli, "SELECT CURRENT_USER()");
      ASSERT_NO_ERROR(user_res);
      EXPECT_THAT(
          *user_res,
          ElementsAre(ElementsAre(change_user_account.username + "@%")));
    }

    ASSERT_NO_ERROR(cli.query("ROUTER SET access_mode='read_only'"));

    {
      auto user_res = query_one_result(cli, "SELECT CURRENT_USER()");
      ASSERT_NO_ERROR(user_res);
      EXPECT_THAT(
          *user_res,
          ElementsAre(ElementsAre(change_user_account.username + "@%")));
    }

    ASSERT_NO_ERROR(cli.query("ROUTER SET access_mode='auto'"));
  }
}

TEST_P(SplittingConnectionTest, ping_succeeds) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR9.1");
  RecordProperty("Requirement",
                 "If `access_mode` is 'auto' and the client sends a "
                 "a ping command, Router MUST target the current host");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  // may start on the primary or secondary.
  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // a noop statement which switches to the primary.
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  SCOPED_TRACE("// ping primary");
  ASSERT_NO_ERROR(cli.ping());

  // executed on the secondary:
  //
  // - SET trackers
  // - SELECT * FROM performance_schema... [not seen by this query]
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                AnyOf(
                    // started on read-write
                    ElementsAre(Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 1)),
                    // started on read-only
                    ElementsAre(Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 1))));
  }

  SCOPED_TRACE("// ping secondary");
  ASSERT_NO_ERROR(cli.ping());

  // executed on the secondary:
  //
  // - SET trackers
  // - SELECT * FROM performance_schema... [not seen by this query]
  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    EXPECT_THAT(*events_res,
                AnyOf(
                    // started on read-write
                    ElementsAre(Pair("statement/com/Ping", 1),
                                Pair("statement/sql/select", 4),
                                Pair("statement/sql/set_option", 1)),
                    // started on read-only
                    ElementsAre(Pair("statement/com/Ping", 1),
                                Pair("statement/sql/select", 5),
                                Pair("statement/sql/set_option", 1))));
  }
}

TEST_P(SplittingConnectionTest, set_option_succeeds) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR9.3");
  RecordProperty("Requirement",
                 "If `access_mode` is 'auto' and the client sends a "
                 "a set-option command, Router MUST target the current host");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  // may start on the primary or secondary depending on router's round-robin
  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  std::vector<std::tuple<std::string, std::string>> initial_expected_stmts{
      {stmt_type_sql_set_option, stmt_set_session_tracker},
      {stmt_type_sql_select, stmt_select_session_vars},
  };

  std::vector<std::tuple<std::string, std::string>> switched_expected_stmts{
      {stmt_type_sql_set_option, stmt_restore_session_vars},
  };

  std::vector<std::tuple<std::string, std::string>> rw_expected_stmts;
  std::vector<std::tuple<std::string, std::string>> ro_expected_stmts;
  bool started_on_rw{false};

  {
    auto stmt_hist_res = statement_history_from_read_write(cli);
    ASSERT_NO_ERROR(stmt_hist_res);

    // detect if router started on a RW or RO node.
    started_on_rw = stmt_hist_res->size() == 2;

    if (started_on_rw) {
      rw_expected_stmts = initial_expected_stmts;
      ro_expected_stmts = switched_expected_stmts;
    } else {
      ro_expected_stmts = initial_expected_stmts;
      rw_expected_stmts = switched_expected_stmts;
    }

    EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(rw_expected_stmts));
    rw_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_history);
  }

  // a noop statement which switches to the primary.
  ASSERT_NO_ERROR(cli.query("TRUNCATE TABLE testing.t1"));

  rw_expected_stmts.emplace_back(stmt_type_sql_truncate,
                                 "TRUNCATE TABLE `testing` . `t1`");

  SCOPED_TRACE("// set-option from primary");
  ASSERT_NO_ERROR(cli.set_server_option(MYSQL_OPTION_MULTI_STATEMENTS_ON));

  rw_expected_stmts.emplace_back(stmt_type_com_set_option, "<NULL>");

  {
    auto stmt_hist_res = statement_history_from_read_write(cli);
    ASSERT_NO_ERROR(stmt_hist_res);

    EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(rw_expected_stmts));
    rw_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_history);
  }

  // secondary
  //
  // Router should:
  // - wait for GTID_EXECUTED.
  // - set multi-statement option.
  ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_wait_gtid);

  {
    auto stmt_res = query_one_result(cli, "SELECT * FROM testing.t1");
    ASSERT_NO_ERROR(stmt_res);

    ro_expected_stmts.emplace_back(stmt_type_sql_select,
                                   "SELECT * FROM `testing` . `t1`");
  }

  // needed?
  ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_wait_gtid);

  {
    auto stmt_hist_res = statement_history_from_read_only(cli);
    ASSERT_NO_ERROR(stmt_hist_res);

    EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(ro_expected_stmts));
    ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_history);
  }

  SCOPED_TRACE("// set-option from secondary");
  ASSERT_NO_ERROR(cli.set_server_option(MYSQL_OPTION_MULTI_STATEMENTS_ON));

  // needed?
  ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_wait_gtid);

  ro_expected_stmts.emplace_back(stmt_type_com_set_option, "<NULL>");

  // needed?
  ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_wait_gtid);

  {
    auto stmt_hist_res = statement_history_from_read_only(cli);
    ASSERT_NO_ERROR(stmt_hist_res);

    EXPECT_THAT(*stmt_hist_res, ::testing::ElementsAreArray(ro_expected_stmts));
    ro_expected_stmts.emplace_back(stmt_type_sql_select, stmt_select_history);
  }
}

TEST_P(SplittingConnectionTest, clone_fails) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR9.4");
  RecordProperty("Requirement",
                 "If `access_mode` is 'auto' and the client sends a "
                 "a 'CLONE INSTANCE', Router MUST fail the statement");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // primary
  SCOPED_TRACE("// CLONE");

  {
    auto stmt_res = cli.query(
        "CLONE INSTANCE FROM clone_user@somehost IDENTIFIED BY 'clone_pass'");
    ASSERT_ERROR(stmt_res);

    // Statement not allowed if access_mode is 'auto'
    EXPECT_EQ(stmt_res.error().value(), 4501) << stmt_res.error();
  }
}

TEST_P(SplittingConnectionTest, binlog_fails) {
  RecordProperty("Worklog", "12794");
  RecordProperty("RequirementId", "FR9.5");
  RecordProperty("Requirement",
                 "If `access_mode` is 'auto' and the client sends a "
                 "a binlog command, Router MUST fail the command");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  ASSERT_NO_ERROR(
      cli.query("SET @source_binlog_checksum=@@global.binlog_checksum"));

  SCOPED_TRACE("// binlog_dump");
  {
    MYSQL_RPL rpl;
    rpl.file_name = nullptr;
    rpl.start_position = 4;
    rpl.server_id = 0;
    rpl.flags = 1 << 0 /* NON_BLOCK */;

    // dump doesn't check the error, fetch does.
    ASSERT_NO_ERROR(cli.binlog_dump(rpl));

    auto fetch_res = cli.binlog_fetch(rpl);
    ASSERT_ERROR(fetch_res);

    // Statement not allowed if access_mode is 'auto'
    EXPECT_EQ(fetch_res.error().value(), 4501) << fetch_res.error();
  }
}

TEST_P(SplittingConnectionTest, select_overlong) {
  RecordProperty("Worklog", "12794");
  RecordProperty(
      "Description",
      "Check if overlong statements are properly tokenized and forwarded.");

  MysqlClient cli;

  auto account = SharedServer::caching_sha2_empty_password_account();

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  // select something to make the transaction actually "open".
  {
    auto query_res =
        query_one_result(cli, "SET /* " + std::string(16 * 1024 * 1024, 'a') +
                                  " */ GLOBAL abc = 1");
    ASSERT_ERROR(query_res);
    // should fail with "Statement not allowed if access_mode is 'auto'"
    EXPECT_EQ(query_res.error().value(), 4501) << query_res.error();
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, SplittingConnectionTest,
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
