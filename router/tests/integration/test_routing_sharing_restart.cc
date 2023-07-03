/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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
#include "process_manager.h"
#include "procs.h"
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "scope_guard.h"
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

std::ostream &operator<<(std::ostream &os, MysqlError e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
}

/**
 * convert a multi-resultset into a simple container which can be EXPECTed
 * against.
 */

// query a single row and return an array of N std::strings.
template <size_t N>
stdx::expected<std::array<std::string, N>, MysqlError> query_one(
    MysqlClient &cli, std::string_view stmt) {
  auto cmd_res = cli.query(stmt);
  if (!cmd_res) return stdx::make_unexpected(cmd_res.error());

  auto results = std::move(*cmd_res);

  auto res_it = results.begin();
  if (!(res_it != results.end())) {
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

static void copy_tree(const mysql_harness::Directory &from_dir,
                      const mysql_harness::Directory &to_dir) {
  for (const auto &path : from_dir) {
    auto from = path;
    auto to = to_dir.join(path.basename());

    if (path.is_directory()) {
      mysql_harness::mkdir(to.str(), mysql_harness::kStrictDirectoryPerm);
      copy_tree(from, to);
    } else {
      mysqlrouter::copy_file(from.str(), to.str());
    }
  }
}

/**
 * A manager of a mysql-server.
 *
 * allows:
 *
 * - initializing a server
 * - copying data directories.
 * - stopping servers
 * - setting up accounts for testing
 * - closing all connections
 */
class SharedServer {
 public:
  SharedServer(TcpPortPool &port_pool) : port_pool_(port_pool) {}

  ~SharedServer() {
    // shutdown via API to get a clean exit-code on windows.
    shutdown();
    process_manager().wait_for_exit();
  }

  stdx::expected<void, MysqlError> shutdown() {
    auto cli_res = admin_cli();
    if (!cli_res) return stdx::make_unexpected(cli_res.error());

    auto shutdown_res = cli_res->shutdown();
    if (!shutdown_res) return stdx::make_unexpected(shutdown_res.error());

    return {};
  }

  std::string mysqld_init_once_dir_name() const {
    return mysqld_init_once_dir_->name();
  }

  [[nodiscard]] std::string mysqld_dir_name() const {
    return mysqld_dir_.name();
  }

  integration_tests::Procs &process_manager() { return procs_; }
#ifdef _WIN32
#define EXE_EXTENSION ".exe"
#define SO_EXTENSION ".dll"
#else
#define EXE_EXTENSION ""
#define SO_EXTENSION ".so"
#endif

  // initialize the server
  //
  // initializes the server once into mysqld_init_once_dir_ and creates copies
  // from that into mysqld_dir_
  void initialize_server(const std::string &datadir) {
    auto bindir = process_manager().get_origin();
    auto mysqld = bindir.join("mysqld" EXE_EXTENSION);

    if (!mysqld.exists()) {
      mysqld_failed_to_start_ = true;
      return;
    }

    auto &proc =
        process_manager()
            .spawner(mysqld.str())
            .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
            .spawn({
                "--no-defaults",
                "--initialize-insecure",
                "--loose-skip-ndbcluster",
                "--innodb_redo_log_capacity=8M",
                "--innodb_autoextend_increment=1",
                "--innodb_use_native_aio=0",
                "--datadir=" + datadir,
                "--log-error=" + datadir +
                    mysql_harness::Path::directory_separator +
                    "mysqld-init.err",
            });
    proc.set_logging_path(datadir, "mysqld-init.err");
    ASSERT_NO_THROW(proc.wait_for_exit(60s));
    if (proc.exit_code() != 0) mysqld_failed_to_start_ = true;
  }

  void prepare_datadir() {
    if (mysqld_init_once_dir_ == nullptr) {
      mysqld_init_once_dir_ = new TempDirectory("mysqld-init-once");

      initialize_server(mysqld_init_once_dir_name());

      if (!mysqld_failed_to_start()) {
        spawn_server_with_datadir(mysqld_init_once_dir_name());
        setup_mysqld_accounts();

        shutdown();
        process_manager().wait_for_exit();
        process_manager().clear();
      }
    }

    // copy the init-once dir to the datadir.
    copy_tree(mysqld_init_once_dir_name(), mysqld_dir_name());

    // remove the auto.cnf to get a unique server-uuid
    unlink(mysqld_dir_.file("auto.cnf").c_str());
  }

  void spawn_server_with_datadir(
      const std::string &datadir,
      const std::vector<std::string> &extra_args = {}) {
    SCOPED_TRACE("// start server");

    // parent is either:
    //
    // - runtime_output_directory/ or
    // - runtime_output_directory/Debug/
    auto bindir = process_manager().get_origin().real_path();

    // if this is a multi-config-build, remember the build-type.
    auto build_type = bindir.basename().str();
    if (build_type == "runtime_output_directory") {
      // no multi-config build.
      build_type = {};
    }

    auto builddir = bindir.dirname();
    if (!build_type.empty()) {
      builddir = builddir.dirname();
    }
    auto sharedir = builddir.join("share");
    auto plugindir = builddir.join("plugin_output_directory");
    if (!build_type.empty()) {
      plugindir = plugindir.join(build_type);
    }
    auto lc_messages_dir = sharedir;

    auto lc_messages80_dir = sharedir.join("mysql-8.0");

    if (lc_messages80_dir.join("english").join("errmsg.sys").exists()) {
      lc_messages_dir = lc_messages80_dir;
    }

    std::string log_file_name = "mysqld-" + std::to_string(starts_) + ".err";

    std::vector<std::string> args{
        "--no-defaults-file",  //
        "--lc-messages-dir=" + lc_messages_dir.str(),
        "--datadir=" + datadir,             //
        "--plugin_dir=" + plugindir.str(),  //
        "--log-error=" + datadir + mysql_harness::Path::directory_separator +
            log_file_name,
        "--port=" + std::to_string(server_port_),
        // defaults to {datadir}/mysql.socket
        "--socket=" + Path(datadir).join("mysql.sock").str(),
        "--mysqlx-port=" + std::to_string(server_mysqlx_port_),
        // defaults to {datadir}/mysqlx.socket
        "--mysqlx-socket=" + Path(datadir).join("mysqlx.sock").str(),
        // disable LOAD DATA/SELECT INTO on the server
        "--secure-file-priv=NULL",          //
        "--innodb_redo_log_capacity=8M",    // fast startups
        "--innodb_autoextend_increment=1",  //
        "--innodb_buffer_pool_size=5M",     //
        "--innodb_use_native_aio=0",        // avoid 'Cannot initialize AIO
                                            // subsystem'
        "--gtid_mode=ON",                   // group-replication
        "--enforce_gtid_consistency=ON",    //
        "--relay-log=relay-log",
    };

    for (const auto &arg : extra_args) {
      args.push_back(arg);
    }

    // remember the extra args for "restart_server()"
    started_args_ = extra_args;

    auto &proc =
        process_manager()
            .spawner(bindir.join("mysqld").str())
#ifdef _WIN32
            // on windows, mysqld has no notify-socket
            .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
#endif
            .spawn(args);
    proc.set_logging_path(datadir, log_file_name);
    if (!proc.wait_for_sync_point_result()) mysqld_failed_to_start_ = true;

#ifdef _WIN32
    // on windows, wait until port is ready as there is no notify-socket.
    if (!(wait_for_port_ready(server_port_, 10s) &&
          wait_for_port_ready(server_mysqlx_port_, 10s))) {
      mysqld_failed_to_start_ = true;
    }
#endif

    ++starts_;
  }

  void spawn_server(const std::vector<std::string> &extra_args = {}) {
    spawn_server_with_datadir(mysqld_dir_name(), extra_args);
  }

  struct Account {
    std::string username;
    std::string password;
    std::string auth_method;
  };

  stdx::expected<MysqlClient, MysqlError> admin_cli() {
    MysqlClient cli;

    auto account = admin_account();

    cli.username(account.username);
    cli.password(account.password);

    auto connect_res = cli.connect(server_host(), server_port());
    if (!connect_res) return connect_res.get_unexpected();

    return cli;
  }

  void create_schema(MysqlClient &cli, const std::string &schema) {
    std::ostringstream oss;
    oss << "CREATE SCHEMA " << std::quoted(schema, '`');

    auto q = oss.str();

    SCOPED_TRACE("// " + q);
    ASSERT_NO_ERROR(cli.query(q)) << q;
  }

  void grant_access(MysqlClient &cli, const Account &account,
                    const std::string &rights) {
    std::ostringstream oss;
    oss << "GRANT " << rights << " ON *.* TO "
        << std::quoted(account.username, '`');

    auto q = oss.str();

    SCOPED_TRACE("// " + q);
    ASSERT_NO_ERROR(cli.query(q)) << q;
  }

  void grant_access(MysqlClient &cli, const Account &account,
                    const std::string &rights, const std::string &schema) {
    std::ostringstream oss;
    oss << "GRANT " << rights << "  ON " << std::quoted(schema, '`') << ".* TO "
        << std::quoted(account.username, '`');

    auto q = oss.str();

    SCOPED_TRACE("// " + q);
    ASSERT_NO_ERROR(cli.query(q)) << q;
  }

  void create_account(MysqlClient &cli, Account account) {
    const std::string q = "CREATE USER " + account.username + " " +         //
                          "IDENTIFIED WITH " + account.auth_method + " " +  //
                          "BY '" + account.password + "'";

    SCOPED_TRACE("// " + q);
    ASSERT_NO_ERROR(cli.query(q)) << q;
  }

  void drop_account(MysqlClient &cli, Account account) {
    const std::string q = "DROP USER " + account.username;

    SCOPED_TRACE("// " + q);
    ASSERT_NO_ERROR(cli.query(q)) << q;
  }

  void setup_mysqld_accounts() {
    auto cli_res = admin_cli();
    ASSERT_NO_ERROR(cli_res);

    auto cli = std::move(cli_res.value());

    create_schema(cli, "testing");

    ASSERT_NO_ERROR(cli.query(R"(CREATE PROCEDURE testing.multiple_results()
BEGIN
  SELECT 1;
  SELECT 2;
END)"));

    for (auto account : {
             native_password_account(),
             native_empty_password_account(),
             caching_sha2_password_account(),
             caching_sha2_empty_password_account(),
             sha256_password_account(),
             sha256_empty_password_account(),
         }) {
      create_account(cli, account);
      grant_access(cli, account, "FLUSH_TABLES, BACKUP_ADMIN");
      grant_access(cli, account, "ALL", "testing");
      grant_access(cli, account, "SELECT", "performance_schema");
    }

    // locking_service
    //
    ASSERT_NO_ERROR(
        cli.query("CREATE FUNCTION service_get_read_locks"
                  "        RETURNS INT"
                  "         SONAME 'locking_service" SO_EXTENSION "'"));

    ASSERT_NO_ERROR(
        cli.query("CREATE FUNCTION service_get_write_locks"
                  "        RETURNS INT"
                  "         SONAME 'locking_service" SO_EXTENSION "'"));
    ASSERT_NO_ERROR(
        cli.query("CREATE FUNCTION service_release_locks"
                  "        RETURNS INT"
                  "         SONAME 'locking_service" SO_EXTENSION "'"));

    // version_token

    ASSERT_NO_ERROR(
        cli.query("CREATE FUNCTION version_tokens_lock_shared"
                  "        RETURNS INT"
                  "         SONAME 'version_token" SO_EXTENSION "'"));
    ASSERT_NO_ERROR(
        cli.query("CREATE FUNCTION version_tokens_lock_exclusive"
                  "        RETURNS INT"
                  "         SONAME 'version_token" SO_EXTENSION "'"));

    // clone

    ASSERT_NO_ERROR(
        cli.query("INSTALL PLUGIN clone"
                  "        SONAME 'mysql_clone" SO_EXTENSION "'"));
  }

  void flush_privileges() {
    SCOPED_TRACE("// flushing privileges");
    auto cli_res = admin_cli();
    ASSERT_NO_ERROR(cli_res);

    flush_privileges(*cli_res);
  }

  void flush_privileges(MysqlClient &cli) {
    ASSERT_NO_ERROR(cli.query("FLUSH PRIVILEGES"));
  }

  // get all connections, but ignore internal connections and this
  // connection.
  static stdx::expected<std::vector<uint64_t>, MysqlError> user_connection_ids(
      MysqlClient &cli) {
    auto ids_res = cli.query(R"(SELECT id
 FROM performance_schema.processlist
WHERE id != CONNECTION_ID() AND
      Command != "Daemon")");
    if (!ids_res) return stdx::make_unexpected(ids_res.error());

    std::vector<uint64_t> ids;
    for (const auto &res : *ids_res) {
      for (auto row : res.rows()) {
        ids.push_back(strtol(row[0], nullptr, 10));
      }
    }

    return ids;
  }

  // close all connections.
  void close_all_connections() {
    SCOPED_TRACE("// closing all connections at the server.");

    auto cli_res = admin_cli();
    ASSERT_NO_ERROR(cli_res);

    close_all_connections(*cli_res);
  }

  void close_all_connections(MysqlClient &cli) {
    {
      auto ids_res = user_connection_ids(cli);
      ASSERT_NO_ERROR(ids_res);

      for (auto id : *ids_res) {
        auto kill_res = cli.kill(id);

        // either it succeeds or "Unknown thread id" because it closed itself
        // between the SELECT and this kill
        EXPECT_TRUE(kill_res || kill_res.error().value() == 1094)
            << kill_res.error();
      }
    }

    SCOPED_TRACE("// checking all connections are closed now.");
    {
      // wait a bit until all connections are really closed.
      using clock_type = std::chrono::steady_clock;
      auto end = clock_type::now() + 1000ms;
      do {
        auto ids_res = user_connection_ids(cli);
        ASSERT_NO_ERROR(ids_res);

        if ((*ids_res).empty()) break;

        ASSERT_LT(clock_type::now(), end) << ": timeout";

        std::this_thread::sleep_for(10ms);
      } while (true);
    }
  }

  // set global settings to default values.
  void reset_to_defaults() {
    auto cli_res = admin_cli();
    ASSERT_NO_ERROR(cli_res);

    reset_to_defaults(*cli_res);
  }

  // set global settings to default values.
  void reset_to_defaults(MysqlClient &cli) {
    ASSERT_NO_ERROR(cli.query("SET GLOBAL max_connections = DEFAULT"));
  }

  [[nodiscard]] bool mysqld_failed_to_start() const {
    return mysqld_failed_to_start_;
  }

  [[nodiscard]] uint16_t server_port() const { return server_port_; }
  [[nodiscard]] uint16_t server_mysqlx_port() const {
    return server_mysqlx_port_;
  }
  [[nodiscard]] std::string server_host() const { return server_host_; }

  static Account caching_sha2_password_account() {
    return {"caching_sha2", "somepass", "caching_sha2_password"};
  }

  static Account caching_sha2_empty_password_account() {
    return {"caching_sha2_empty", "", "caching_sha2_password"};
  }

  static Account caching_sha2_single_use_password_account() {
    return {"caching_sha2_single_use", "notusedyet", "caching_sha2_password"};
  }

  static Account native_password_account() {
    return {"native", "somepass", "mysql_native_password"};
  }

  static Account native_empty_password_account() {
    return {"native_empty", "", "mysql_native_password"};
  }

  static Account sha256_password_account() {
    return {"sha256_pass", "sha256pass", "sha256_password"};
  }

  static Account sha256_empty_password_account() {
    return {"sha256_empty", "", "sha256_password"};
  }

  static Account admin_account() {
    return {"root", "", "caching_sha2_password"};
  }

  static void destroy_statics() {
    if (mysqld_init_once_dir_) {
      delete mysqld_init_once_dir_;
      mysqld_init_once_dir_ = nullptr;
    }
  }

 private:
  static TempDirectory *mysqld_init_once_dir_;
  TempDirectory mysqld_dir_{"mysqld"};

  integration_tests::Procs procs_;
  TcpPortPool &port_pool_;

  static const constexpr char server_host_[] = "127.0.0.1";
  uint16_t server_port_{port_pool_.get_next_available()};
  uint16_t server_mysqlx_port_{port_pool_.get_next_available()};

  bool mysqld_failed_to_start_{false};

  std::vector<std::string> started_args_;

  uint32_t starts_{};
};

TempDirectory *SharedServer::mysqld_init_once_dir_ = nullptr;

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
        .section("http_server", {{"port", std::to_string(rest_port_)}});

    for (const auto &param : share_connection_params) {
      auto port_key =
          std::make_tuple(param.client_ssl_mode, param.server_ssl_mode, 0);
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
          std::min(num_destinations, pool_size_), 1s));
    }
  }

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
      return stdx::make_unexpected(
          make_error_code(std::errc::no_such_file_or_directory));
    }
  }

  stdx::expected<int, std::error_code> idle_server_connections() {
    return rest_get_int(rest_api_basepath + "/connection_pool/main/status",
                        "/idleServerConnections");
  }

  stdx::expected<void, std::error_code> wait_for_idle_server_connections(
      int expected_value, std::chrono::seconds timeout) {
    using clock_type = std::chrono::steady_clock;

    const auto end_time = clock_type::now() + timeout;
    do {
      auto int_res = idle_server_connections();
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
    for (auto &s : shared_servers_) {
      if (s == nullptr) {
        s = new SharedServer(port_pool_);
        s->prepare_datadir();
        s->spawn_server();

        if (s->mysqld_failed_to_start()) {
          GTEST_SKIP() << "mysql-server failed to start.";
        }
      }
    }

    run_slow_tests_ = std::getenv("RUN_SLOW_TESTS") != nullptr;
  }

  std::array<SharedServer *, 4> servers() { return shared_servers_; }

  TcpPortPool &port_pool() { return port_pool_; }

  [[nodiscard]] bool run_slow_tests() const { return run_slow_tests_; }

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

  std::array<SharedServer *, 4> shared_servers_{};

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

    shared_router_ = std::make_unique<SharedRouter>(test_env->port_pool(), 128);
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

    if (::testing::Test::HasFatalFailure()) {
      shared_router_->process_manager().dump_logs();
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

    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 1s));
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
  static constexpr const size_t kNumServers = S;
  static constexpr const size_t kMaxPoolSize = P;

  static void SetUpTestSuite() {
    for (const auto &s : shared_servers()) {
      if (s->mysqld_failed_to_start()) GTEST_SKIP();
    }

    TestWithSharedRouter::SetUpTestSuite(
        test_env->port_pool(), shared_servers(), kMaxPoolSize, split_routes);
  }

  static void TearDownTestSuite() {
    TestWithSharedRouter::TearDownTestSuite();

    if (::testing::Test::HasFatalFailure()) {
      for (const auto &s : shared_servers()) {
        s->process_manager().dump_logs();
      }
    }
  }

  static std::array<SharedServer *, kNumServers> shared_servers() {
    std::array<SharedServer *, kNumServers> o;

    // get a subset of the started servers
    for (auto [ndx, s] : stdx::views::enumerate(test_env->servers())) {
      if (ndx >= kNumServers) break;

      o[ndx] = s;
    }

    return o;
  }

  SharedRouter *shared_router() { return TestWithSharedRouter::router(); }

  void SetUp() override {
    for (auto &s : shared_servers()) {
      // shared_server_ may be null if TestWithSharedServer::SetUpTestSuite
      // threw?
      if (s == nullptr || s->mysqld_failed_to_start()) {
        GTEST_SKIP() << "failed to start mysqld";
      } else {
        s->flush_privileges();       // reset the auth-cache
        s->close_all_connections();  // reset the router's connection-pool
        s->reset_to_defaults();
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

  const std::string wrong_password_{"wrong_password"};
  const std::string empty_password_{""};
};

template <class T>
static constexpr uint8_t cmd_byte() {
  return classic_protocol::Codec<T>::cmd_byte();
}

/**
 * test if a ping to dead server after on-demand connect is handled correctly.
 *
 * 1. connect
 * 2. pool connection
 * 3. kill server
 * 4. send command to establish a new connection to server
 * 5. expect an error
 */
TEST_P(ShareConnectionTestWithRestartedServer,
       classic_protocol_kill_backend_reconnect_all_commands) {
  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connecting to server");
  std::array<MysqlClient, 40> clis;

  // open one connection per command
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    SCOPED_TRACE("// connecting for cmd " + std::to_string(ndx));
    cli.username("root");
    cli.password("");
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (GetParam().client_ssl_mode == kRequired) {
      ASSERT_ERROR(connect_res);
      GTEST_SKIP() << connect_res.error();
    }
    ASSERT_NO_ERROR(connect_res);

    // wait until connection is in the pool.
    if (can_share) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(
          std::min(ndx + 1, kNumServers), 1s));
    }
  }

  // shut down the intermediate routers while the connection is pooled.
  for (auto &inter : intermediate_routers()) {
    ASSERT_NO_FATAL_FAILURE(
        stop_intermediate_router(inter, false /* wait_for_stopped */));
  }

  // wait for the intermediate router to shutdown
  for (auto &inter : intermediate_routers()) {
    ASSERT_NO_FATAL_FAILURE(wait_stopped_intermediate_router(inter));
  }

  // caps for the error-packet parser
  auto caps = classic_protocol::capabilities::protocol_41;

  // send one command per connection.
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    SCOPED_TRACE("// testing command " + std::to_string(ndx));
    std::vector<uint8_t> buf;

    {
      auto encode_res = classic_protocol::encode<
          classic_protocol::frame::Frame<classic_protocol::wire::FixedInt<1>>>(
          {0, {static_cast<uint8_t>(ndx)}}, caps, net::dynamic_buffer(buf));
      ASSERT_NO_ERROR(encode_res);

      auto send_res = net::impl::socket::send(cli.native_handle(), buf.data(),
                                              buf.size(), 0);
      ASSERT_NO_ERROR(send_res);
      EXPECT_EQ(*send_res, buf.size());
    }

    enum class ExpectedResponse {
      None,
      Error,
    } expected_response{ExpectedResponse::Error};

    switch (ndx) {
      case cmd_byte<classic_protocol::message::client::StmtParamAppendData>():
      case cmd_byte<classic_protocol::message::client::StmtClose>():
      case cmd_byte<classic_protocol::message::client::Quit>():
        expected_response = ExpectedResponse::None;
        break;
    }

    // recv the error-msg
    if (expected_response == ExpectedResponse::Error) {
      buf.resize(1024);  // should be large enough.

      auto recv_res = net::impl::socket::recv(cli.native_handle(), buf.data(),
                                              buf.size(), 0);
      if (!recv_res) {
        // on windows the connection may be closed before the error-msg is sent.
        ASSERT_THAT(recv_res.error(),
                    AnyOf(make_error_condition(std::errc::connection_aborted),
                          make_error_condition(std::errc::connection_reset)));
      } else {
        buf.resize(*recv_res);

        if (*recv_res == 0) {
          // connection closed.
          ASSERT_TRUE(!can_share);
        } else {
          ASSERT_GT(*recv_res, 5) << mysql_harness::hexify(buf);
          ASSERT_EQ(buf[4], 0xff) << mysql_harness::hexify(buf);

          auto decode_res =
              classic_protocol::decode<classic_protocol::frame::Frame<
                  classic_protocol::message::server::Error>>(net::buffer(buf),
                                                             caps);
          ASSERT_NO_ERROR(decode_res);

          auto decoded = std::move(*decode_res);
          auto frame = decoded.second;

          auto msg = frame.payload();

          int expected_error_code =
              2003;  // Can't connect to remove MySQL Server
          switch (ndx) {
            case 0:   // sleep
            case 5:   // create-db
            case 6:   // drop-db
            case 8:   // deprecated
            case 10:  // process-info
            case 11:  // connect
            case 13:  // debug
            case 15:  // time
            case 16:  // delayed insert
            case cmd_byte<
                classic_protocol::message::client::ChangeUser>():  // 17
            case 19:                                               // table-dump
            case 20:  // connect-out
            case 29:  // daemon
            case 33:  // subscribe-group-replication-stream
            case 34:  // unused ...
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:

              // unknown command
              expected_error_code = 1047;
              break;
            case cmd_byte<
                classic_protocol::message::client::StmtExecute>():  // 23
            case cmd_byte<
                classic_protocol::message::client::StmtReset>():  // 26
            case cmd_byte<
                classic_protocol::message::client::StmtFetch>():  // 28

              // unknown prepared statement handler.
              expected_error_code = 1243;
              break;
          }

          EXPECT_EQ(msg.error_code(), expected_error_code) << msg.message();
        }
      }
    }
  }
}

/**
 * test if a broken command after reconnect is handled correctly.
 *
 * 1. connect
 * 2. pool connection
 * 3. send broken command after reconnect
 * 4. expect an error
 */
TEST_P(ShareConnectionTestWithRestartedServer,
       classic_protocol_reconnect_all_commands) {
  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connecting to server");

  // open one connection per command
  std::array<MysqlClient, 40> clis;

  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    SCOPED_TRACE("// connecting for cmd " + std::to_string(ndx));

    auto account = SharedServer::native_empty_password_account();

    cli.username(account.username);
    cli.password(account.password);

    // disable encryption as hand-crafted commands will be sent.
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));

    auto connect_res =
        cli.connect(shared_router()->host(), shared_router()->port(GetParam()));
    if (GetParam().client_ssl_mode == kRequired) {
      ASSERT_ERROR(connect_res);
      GTEST_SKIP() << connect_res.error();
    }
    ASSERT_NO_ERROR(connect_res);

    // wait until connection is in the pool.
    if (can_share) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(
          std::min(ndx + 1, kNumServers), 1s));
    }
  }

  // caps for the error-packet parser
  auto caps = classic_protocol::capabilities::protocol_41;

  // send one command per connection.
  for (auto [ndx, cli] : stdx::views::enumerate(clis)) {
    SCOPED_TRACE("// testing command " + std::to_string(ndx));
    std::vector<uint8_t> buf;

    {
      auto encode_res = classic_protocol::encode<
          classic_protocol::frame::Frame<classic_protocol::wire::FixedInt<1>>>(
          {0, {static_cast<uint8_t>(ndx)}}, caps, net::dynamic_buffer(buf));
      ASSERT_NO_ERROR(encode_res);

      auto send_res = net::impl::socket::send(cli.native_handle(), buf.data(),
                                              buf.size(), 0);
      ASSERT_NO_ERROR(send_res);
      EXPECT_EQ(*send_res, buf.size());
    }

    enum class ExpectedResponse {
      None,
      Error,
      Ok,
      Something,
    } expected_response{ExpectedResponse::Error};

    switch (ndx) {
      case cmd_byte<classic_protocol::message::client::StmtParamAppendData>():
      case cmd_byte<classic_protocol::message::client::StmtClose>():
      case cmd_byte<classic_protocol::message::client::Quit>():
        expected_response = ExpectedResponse::None;
        break;
      case cmd_byte<classic_protocol::message::client::ResetConnection>():
      case cmd_byte<classic_protocol::message::client::Ping>():
      case cmd_byte<classic_protocol::message::client::Clone>():
        expected_response = ExpectedResponse::Ok;
        break;
      case cmd_byte<classic_protocol::message::client::Statistics>():
        expected_response = ExpectedResponse::Something;
        break;
    }

    // recv the error-msg
    if (expected_response == ExpectedResponse::Error) {
      buf.resize(1024);  // should be large enough.

      auto recv_res = net::impl::socket::recv(cli.native_handle(), buf.data(),
                                              buf.size(), 0);
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

      int expected_error_code = 1835;  // malformed packet
      switch (ndx) {
        case 0:   // sleep
        case 5:   // create-db
        case 6:   // drop-db
        case 8:   // deprecated
        case 10:  // process-info
        case 11:  // connect
        case 13:  // debug
        case 15:  // time
        case 16:  // delayed insert
        case cmd_byte<classic_protocol::message::client::ChangeUser>():  // 17
        case 19:  // table-dump
        case 20:  // connect-out
        case 29:  // daemon
        case 33:  // subscribe-group-replication-stream
        case 34:  // unused ...
        case 35:
        case 36:
        case 37:
        case 38:
        case 39:

          // unknown command
          expected_error_code = 1047;
          break;
        case cmd_byte<classic_protocol::message::client::StmtExecute>():  // 23
        case cmd_byte<classic_protocol::message::client::StmtReset>():    // 26
        case cmd_byte<classic_protocol::message::client::StmtFetch>():    // 28

          // unknown prepared statement handler | malformed packet.
          expected_error_code = can_share ? 1243 : 1835;
          break;
        case cmd_byte<classic_protocol::message::client::InitSchema>():  // 2

          expected_error_code = 1046;  // no database selected
          break;
        case cmd_byte<classic_protocol::message::client::Query>():  // 3
          expected_error_code =
              (GetParam().client_ssl_mode != kPassthrough) ? 1065 : 1835;
          break;
        case cmd_byte<classic_protocol::message::client::StmtPrepare>():  // 22

          expected_error_code = 1065;  // query was empty
          break;
        case cmd_byte<classic_protocol::message::client::BinlogDump>():  // 18
        case cmd_byte<
            classic_protocol::message::client::BinlogDumpGtid>():  // 30

          expected_error_code = 1227;  // query was empty
          break;
        case cmd_byte<
            classic_protocol::message::client::RegisterReplica>():  // 21

          // access denied
          expected_error_code = 1045;
          break;
      }

      EXPECT_EQ(msg.error_code(), expected_error_code) << msg.message();
    } else if (expected_response == ExpectedResponse::Ok) {
      buf.resize(1024);  // should be large enough.

      auto recv_res = net::impl::socket::recv(cli.native_handle(), buf.data(),
                                              buf.size(), 0);
      ASSERT_NO_ERROR(recv_res);

      buf.resize(*recv_res);

      ASSERT_GT(buf.size(), 5) << mysql_harness::hexify(buf);
      ASSERT_EQ(buf[4], 0x0) << mysql_harness::hexify(buf);

      auto decode_res = classic_protocol::decode<classic_protocol::frame::Frame<
          classic_protocol::message::server::Ok>>(net::buffer(buf), caps);
      ASSERT_NO_ERROR(decode_res);
    } else if (expected_response == ExpectedResponse::Something) {
      buf.resize(1024);  // should be large enough.

      auto recv_res = net::impl::socket::recv(cli.native_handle(), buf.data(),
                                              buf.size(), 0);
      ASSERT_NO_ERROR(recv_res);

      buf.resize(*recv_res);

      ASSERT_GT(buf.size(), 4) << mysql_harness::hexify(buf);

      auto decode_res = classic_protocol::decode<
          classic_protocol::frame::Frame<classic_protocol::wire::String>>(
          net::buffer(buf), caps);
      ASSERT_NO_ERROR(decode_res);
    }
  }
}

/*
 * check that failover and recovery also works with connection-sharing enabled.
 */
TEST_P(ShareConnectionTestWithRestartedServer,
       classic_protocol_failover_and_recover_purged) {
  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connecting to server");

  uint16_t my_port{};
  {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto port_res = query_one<1>(cli, "SELECT @@port");
    ASSERT_NO_ERROR(port_res);

    auto my_port_num_res = from_string((*port_res)[0]);
    ASSERT_NO_ERROR(my_port_num_res);

    my_port = *my_port_num_res;
  }

  if (can_share) {
    SCOPED_TRACE("// wait until connection is pooled.");
    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));

    SCOPED_TRACE("// force a close of the connections in the pool");

    ASSERT_NO_FATAL_FAILURE(
        this->wait_for_connections_to_server_expired(my_port));
  }

  SCOPED_TRACE("// stop the other servers.");
  {
    int nodes_shutdown{0};

    for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
      if (s->server_port() != my_port) {
        auto inter = intermediate_routers()[ndx];

        ASSERT_NO_FATAL_FAILURE(stop_intermediate_router(inter));
        ++nodes_shutdown;
      }
    }
    ASSERT_EQ(nodes_shutdown, 2);
  }

  SCOPED_TRACE(
      "// try again, the connection should work and round-robin to the first "
      "node again.");
  for (size_t round = 0; round < 2; ++round) {
    SCOPED_TRACE("// round: " + std::to_string(round));
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto port_res = query_one<1>(cli, "SELECT @@port");
    ASSERT_NO_ERROR(port_res);

    auto my_port_num_res = from_string((*port_res)[0]);
    ASSERT_NO_ERROR(my_port_num_res);

    EXPECT_EQ(my_port, *my_port_num_res);

    if (can_share) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));

      this->wait_for_connections_to_server_expired(my_port);
    }
  }

  // stop the first router and start another again.
  {
    int started{};
    for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
      if (s->server_port() == my_port) {
        auto inter = intermediate_routers()[ndx];

        ASSERT_NO_FATAL_FAILURE(stop_intermediate_router(inter));
      } else {
        if (started == 0) {
          auto inter = intermediate_routers()[ndx];

          this->start_intermediate_router_for_server(inter, s);

          ++started;
        }
      }
    }

    EXPECT_EQ(started, 1);
  }

  // wait until quarantine is over.
  {
    using clock_type = std::chrono::steady_clock;

    auto end = clock_type::now() + 2s;  // default is 1s
    do {
      MysqlClient cli;

      cli.username("root");
      cli.password("");

      {
        auto connect_res = cli.connect(shared_router()->host(),
                                       shared_router()->port(GetParam()));
        if (!connect_res) {
          if (connect_res.error().value() == 2003) {
            ASSERT_LT(clock_type::now(), end);

            std::this_thread::sleep_for(200ms);
            continue;
          }
        }
        ASSERT_NO_ERROR(connect_res);
      }

      auto port_res = query_one<1>(cli, "SELECT @@port");
      ASSERT_NO_ERROR(port_res);

      auto my_port_num_res = from_string((*port_res)[0]);
      ASSERT_NO_ERROR(my_port_num_res);

      // should be another server now.
      EXPECT_NE(my_port, *my_port_num_res);
      my_port = *my_port_num_res;

      break;
    } while (true);
  }

  // try again, the connection should work and round-robin to the 2nd node
  // again.
  {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto port_res = query_one<1>(cli, "SELECT @@port");
    ASSERT_NO_ERROR(port_res);

    auto my_port_num_res = from_string((*port_res)[0]);
    ASSERT_NO_ERROR(my_port_num_res);

    EXPECT_EQ(my_port, *my_port_num_res);
  }

  // restart the other servers.
  for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
    if (s->server_port() != my_port) {
      auto inter = intermediate_routers()[ndx];

      ASSERT_NO_FATAL_FAILURE(this->restart_intermediate_router(inter, s));
    }
  }
}

/*
 * check that failover and recovery also works with connection-sharing enabled.
 */
TEST_P(ShareConnectionTestWithRestartedServer,
       classic_protocol_failover_and_recover_pooled) {
  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connecting to server");

  uint16_t my_port{};
  {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto port_res = query_one<1>(cli, "SELECT @@port");
    ASSERT_NO_ERROR(port_res);

    auto my_port_num_res = from_string((*port_res)[0]);
    ASSERT_NO_ERROR(my_port_num_res);

    my_port = *my_port_num_res;
  }

  if (can_share) {
    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
  }

  {
    int nodes_shutdown{0};

    for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
      if (s->server_port() != my_port) {
        auto inter = intermediate_routers()[ndx];
        ASSERT_NO_FATAL_FAILURE(stop_intermediate_router(inter));

        ++nodes_shutdown;
      }
    }
    ASSERT_EQ(nodes_shutdown, 2);
  }

  // try again, the connection should work and round-robin to the first node
  // again.
  for (size_t round = 0; round < 2; ++round) {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto port_res = query_one<1>(cli, "SELECT @@port");
    ASSERT_NO_ERROR(port_res);

    auto my_port_num_res = from_string((*port_res)[0]);
    ASSERT_NO_ERROR(my_port_num_res);

    EXPECT_EQ(my_port, *my_port_num_res);
  }

  if (can_share) {
    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
  }

  // stop the first router and start another again.
  {
    int started{};
    for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
      if (s->server_port() == my_port) {
        auto inter = intermediate_routers()[ndx];

        ASSERT_NO_FATAL_FAILURE(this->stop_intermediate_router(inter));
      } else {
        if (started == 0) {
          auto inter = intermediate_routers()[ndx];
          ASSERT_NO_FATAL_FAILURE(
              this->start_intermediate_router_for_server(inter, s));
          ++started;
        }
      }
    }

    EXPECT_EQ(started, 1);
  }

  // wait until quarantine is over.
  {
    using clock_type = std::chrono::steady_clock;

    auto end = clock_type::now() + 2s;
    do {
      MysqlClient cli;

      cli.username("root");
      cli.password("");

      {
        auto connect_res = cli.connect(shared_router()->host(),
                                       shared_router()->port(GetParam()));
        if (!connect_res) {
          if (connect_res.error().value() == 2003) {
            ASSERT_LT(clock_type::now(), end);

            std::this_thread::sleep_for(200ms);
            continue;
          }
        }
        ASSERT_NO_ERROR(connect_res);
      }

      auto port_res = query_one<1>(cli, "SELECT @@port");
      ASSERT_NO_ERROR(port_res);

      auto my_port_num_res = from_string((*port_res)[0]);
      ASSERT_NO_ERROR(my_port_num_res);

      // should be another server now.
      EXPECT_NE(my_port, *my_port_num_res);
      my_port = *my_port_num_res;

      break;
    } while (true);
  }

  // try again, the connection should work and round-robin to the 2nd node
  // again.
  {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto port_res = query_one<1>(cli, "SELECT @@port");
    ASSERT_NO_ERROR(port_res);

    auto my_port_num_res = from_string((*port_res)[0]);
    ASSERT_NO_ERROR(my_port_num_res);

    EXPECT_EQ(my_port, *my_port_num_res);
  }

  // restart the other servers.
  for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
    if (s->server_port() != my_port) {
      auto inter = intermediate_routers()[ndx];

      this->start_intermediate_router_for_server(inter, s);
    }
  }
}

/*
 * check that failover and recovery also works with connection-sharing enabled.
 *
 * that check queries fail properly if they are pooled.
 */
TEST_P(ShareConnectionTestWithRestartedServer,
       classic_protocol_failover_and_recover_purged_query) {
  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connecting to server");

  uint16_t my_port{};
  {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto port_res = query_one<1>(cli, "SELECT @@port");
    ASSERT_NO_ERROR(port_res);

    auto my_port_num_res = from_string((*port_res)[0]);
    ASSERT_NO_ERROR(my_port_num_res);

    my_port = *my_port_num_res;

    if (can_share) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));

      ASSERT_NO_FATAL_FAILURE(
          this->wait_for_connections_to_server_expired(my_port));
    }

    // reconnects
    {
      auto port2_res = query_one<1>(cli, "SELECT @@port");
      ASSERT_NO_ERROR(port2_res);

      auto my_port2_num_res = from_string((*port2_res)[0]);
      ASSERT_NO_ERROR(my_port_num_res);

      EXPECT_EQ(my_port, *my_port2_num_res);  // still on the same port.
    }

    // kill another backend
    {
      int nodes_shutdown{0};

      for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
        if (s->server_port() != my_port) {
          auto inter = intermediate_routers()[ndx];

          ASSERT_NO_FATAL_FAILURE(this->stop_intermediate_router(inter));

          ++nodes_shutdown;

          break;
        }
      }
      ASSERT_EQ(nodes_shutdown, 1);
    }

    // unaffected.
    {
      auto port2_res = query_one<1>(cli, "SELECT @@port");
      ASSERT_NO_ERROR(port2_res);

      auto my_port2_num_res = from_string((*port2_res)[0]);
      ASSERT_NO_ERROR(my_port_num_res);

      EXPECT_EQ(my_port, *my_port2_num_res);  // still on the same port.
    }

    // kill this backend
    {
      int nodes_shutdown{0};

      for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
        if (s->server_port() == my_port) {
          auto inter = intermediate_routers()[ndx];

          ASSERT_NO_FATAL_FAILURE(this->stop_intermediate_router(inter));

          ++nodes_shutdown;

          break;
        }
      }
      ASSERT_EQ(nodes_shutdown, 1);
    }

    if (can_share) {
      // if the connection was pooled, then a SELECT will try to reopen the
      // connection, but fail to reach the backend.
      auto cmd_res = query_one<1>(cli, "SELECT @@port");
      ASSERT_ERROR(cmd_res);
      EXPECT_EQ(cmd_res.error().value(), 2003);  // lost
    }

    // the connection should now be closed.
    {
      auto cmd_res = query_one<1>(cli, "SELECT @@port");
      ASSERT_ERROR(cmd_res);
      EXPECT_EQ(cmd_res.error().value(), 2013);  // close
    }
  }

  // A, B are dead, we should be on C now.
  for (size_t round = 0; round < 2; ++round) {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto port_res = query_one<1>(cli, "SELECT @@port");
    ASSERT_NO_ERROR(port_res);

    auto my_port_num_res = from_string((*port_res)[0]);
    ASSERT_NO_ERROR(my_port_num_res);

    EXPECT_NE(my_port, *my_port_num_res);
  }
}

/*
 * check that failover and recovery also works with connection-sharing enabled.
 *
 * that check queries fail properly if they are pooled.
 */
TEST_P(ShareConnectionTestWithRestartedServer,
       classic_protocol_failover_and_recover_purged_pooled) {
  const bool can_share = GetParam().can_share();

  SCOPED_TRACE("// connecting to server");

  uint16_t my_port{};
  {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto port_res = query_one<1>(cli, "SELECT @@port");
    ASSERT_NO_ERROR(port_res);

    auto my_port_num_res = from_string((*port_res)[0]);
    ASSERT_NO_ERROR(my_port_num_res);

    my_port = *my_port_num_res;

    if (can_share) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
    }

    // reconnects
    {
      auto port2_res = query_one<1>(cli, "SELECT @@port");
      ASSERT_NO_ERROR(port2_res);

      auto my_port2_num_res = from_string((*port2_res)[0]);
      ASSERT_NO_ERROR(my_port_num_res);

      EXPECT_EQ(my_port, *my_port2_num_res);  // still on the same port.
    }

    // kill another backend
    {
      int nodes_shutdown{0};

      for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
        if (s->server_port() != my_port) {
          auto inter = intermediate_routers()[ndx];

          ASSERT_NO_FATAL_FAILURE(this->stop_intermediate_router(inter));

          ++nodes_shutdown;

          break;
        }
      }
      ASSERT_EQ(nodes_shutdown, 1);
    }

    // unaffected.
    {
      auto port2_res = query_one<1>(cli, "SELECT @@port");
      ASSERT_NO_ERROR(port2_res);

      auto my_port2_num_res = from_string((*port2_res)[0]);
      ASSERT_NO_ERROR(my_port_num_res);

      EXPECT_EQ(my_port, *my_port2_num_res);  // still on the same port.
    }

    // kill this backend
    {
      int nodes_shutdown{0};

      for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
        if (s->server_port() == my_port) {
          auto inter = intermediate_routers()[ndx];

          ASSERT_NO_FATAL_FAILURE(this->stop_intermediate_router(inter));

          ++nodes_shutdown;

          break;
        }
      }
      ASSERT_EQ(nodes_shutdown, 1);
    }

    // fails.
    if (can_share) {
      auto cmd_res = query_one<1>(cli, "SELECT @@port");
      ASSERT_ERROR(cmd_res);
      EXPECT_EQ(cmd_res.error().value(), 2003);  // lost
    }

    {
      auto cmd_res = query_one<1>(cli, "SELECT @@port");
      ASSERT_ERROR(cmd_res);
      EXPECT_EQ(cmd_res.error().value(), 2013);  // close
    }
  }

  // A, B are dead, we should be on C now.
  for (size_t round = 0; round < 2; ++round) {
    MysqlClient cli;

    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto port_res = query_one<1>(cli, "SELECT @@port");
    ASSERT_NO_ERROR(port_res);

    auto my_port_num_res = from_string((*port_res)[0]);
    ASSERT_NO_ERROR(my_port_num_res);

    EXPECT_NE(my_port, *my_port_num_res);
  }
}

/**
 * test if a dead server after on-demand connect is handled correctly.
 *
 * 1. connect
 * 2. pool connection
 * 3. kill the current server
 * 4. send command to establish a new connection to server
 * 5. expect an error
 *
 * Additionally,
 *
 * - check that the connection got closed
 * - check that connections to other backends still work.
 */
TEST_P(ShareConnectionTestWithRestartedServer,
       classic_protocol_kill_my_backend_reconnect_select) {
  SCOPED_TRACE("// connecting to server");
  std::array<MysqlClient, 4> clis;  // more clients then destinations.

  for (auto &cli : clis) {
    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));
  }

  auto port_res = query_one<1>(clis[0], "SELECT @@port");
  ASSERT_NO_ERROR(port_res);

  auto my_port_num_res = from_string((*port_res)[0]);
  ASSERT_NO_ERROR(my_port_num_res);

  uint16_t my_port = *my_port_num_res;

  // shut down the server connection is for while the connection is pooled.
  // wait for the server to shutdown
  int nodes_shutdown{0};
  // shut down the intermediate router while the connection is pooled

  for (auto [ndx, s] : stdx::views::enumerate(shared_servers())) {
    if (s->server_port() == my_port) {
      auto inter = intermediate_routers()[ndx];

      ASSERT_NO_FATAL_FAILURE(this->stop_intermediate_router(inter));

      ++nodes_shutdown;
    }
  }
  ASSERT_EQ(nodes_shutdown, 1);

  SCOPED_TRACE("// the query should fail.");
  {
    auto cmd_res = query_one<1>(clis[0], "SELECT @@port");
    ASSERT_ERROR(cmd_res);
    if (!GetParam().can_share()) {
      // not pooled, the connection is closed directly.
      EXPECT_EQ(cmd_res.error().value(), 2013) << cmd_res.error();
      EXPECT_THAT(cmd_res.error().message(),
                  StartsWith("Lost connection to MySQL server during query"))
          << cmd_res.error();
    } else {
      EXPECT_EQ(cmd_res.error().value(), 2003) << cmd_res.error();
      EXPECT_THAT(cmd_res.error().message(),
                  StartsWith("Can't connect to remote MySQL server"))
          << cmd_res.error();
    }
  }

  SCOPED_TRACE("// the query should fail too.");
  {
    auto cmd_res = query_one<1>(clis[0], "SELECT @@port");
    ASSERT_ERROR(cmd_res);

    // the connection is closed even after it was pooled before.
    EXPECT_EQ(cmd_res.error().value(), 2013) << cmd_res.error();
    EXPECT_THAT(cmd_res.error().message(),
                StartsWith("Lost connection to MySQL server during query"))
        << cmd_res.error();
  }

  SCOPED_TRACE("// ... the other pooled connection should fail.");
  {
    auto cmd_res = query_one<1>(clis[3], "SELECT @@port");
    ASSERT_ERROR(cmd_res);

    if (!GetParam().can_share()) {
      // not pooled, the connection is closed directly.
      EXPECT_EQ(cmd_res.error().value(), 2013) << cmd_res.error();
      EXPECT_THAT(cmd_res.error().message(),
                  StartsWith("Lost connection to MySQL server during query"))
          << cmd_res.error();
    } else {
      EXPECT_EQ(cmd_res.error().value(), 2003) << cmd_res.error();
      EXPECT_THAT(cmd_res.error().message(),
                  StartsWith("Can't connect to remote MySQL server"))
          << cmd_res.error();
    }
  }

  SCOPED_TRACE("// ... but a new connection works");
  MysqlClient cli2;

  cli2.username("root");
  cli2.password("");

  ASSERT_NO_ERROR(
      cli2.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto port2_res = query_one<1>(cli2, "SELECT @@port");
    ASSERT_NO_ERROR(port2_res);

    EXPECT_NE(*port_res, *port2_res);
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionTestWithRestartedServer,
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
