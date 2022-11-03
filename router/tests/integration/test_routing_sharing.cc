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
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "scope_guard.h"
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

template <class T, class E>
::testing::AssertionResult StdxExpectedSuccess(const char *expr,
                                               const stdx::expected<T, E> &e) {
  if (e) return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "Expected: " << expr << " succeeds.\n"
                                       << "  Actual: " << e.error() << "\n";
}

template <class T, class E>
::testing::AssertionResult StdxExpectedFailure(const char *expr,
                                               const stdx::expected<T, E> &e) {
  if (!e) return ::testing::AssertionSuccess();

  if constexpr (std::is_void_v<T>) {
    return ::testing::AssertionFailure() << "Expected: " << expr << " fails.\n"
                                         << "  Actual: succeeded\n";
  } else {
    return ::testing::AssertionFailure()
           << "Expected: " << expr << " fails.\n"
           << "  Actual: " << ::testing::PrintToString(e.value()) << "\n";
  }
}

#define EXPECT_NO_ERROR(x) EXPECT_PRED_FORMAT1(StdxExpectedSuccess, (x))
#define ASSERT_NO_ERROR(x) ASSERT_PRED_FORMAT1(StdxExpectedSuccess, (x))

#define EXPECT_ERROR(x) EXPECT_PRED_FORMAT1(StdxExpectedFailure, (x))
#define ASSERT_ERROR(x) ASSERT_PRED_FORMAT1(StdxExpectedFailure, (x))

static constexpr const std::string_view kDisabled{"DISABLED"};
static constexpr const std::string_view kRequired{"REQUIRED"};
static constexpr const std::string_view kPreferred{"PREFERRED"};
static constexpr const std::string_view kPassthrough{"PASSTHROUGH"};
static constexpr const std::string_view kAsClient{"AS_CLIENT"};

std::ostream &operator<<(std::ostream &os, MysqlError e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
}

/*
 * a iterator that wraps an iterable and returns a counter the
 * deref'ed wrapped iterable.
 *
 * @tparam T a iterable
 *
 * @code
 * for (auto [ndx, vc]: enumerate(std::vector<int>{1, 23, 42})) {
 *   std::cerr << "[" << ndx << "] " << v << "\n";
 * }
 *
 * // [0] 1
 * // [1] 23
 * // [2] 42
 * @endcode
 */
template <class T, class TIter = decltype(std::begin(std::declval<T>())),
          class = decltype(std::end(std::declval<T>()))>
constexpr auto enumerate(T &&iterable) {
  struct iterator {
    size_t i;

    TIter iter;

    bool operator!=(const iterator &other) const { return iter != other.iter; }

    void operator++() {
      ++i;
      ++iter;
    }

    auto operator*() const { return std::tie(i, *iter); }
  };

  struct iterable_wrapper {
    T iterable;

    auto begin() { return iterator{0, std::begin(iterable)}; }

    auto end() { return iterator{0, std::end(iterable)}; }
  };

  return iterable_wrapper{std::forward<T>(iterable)};
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
  for (auto [ndx, f] : enumerate(out)) {
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

static stdx::expected<std::vector<std::pair<std::string, uint32_t>>, MysqlError>
changed_event_counters(MysqlClient &cli, const std::string &filter) {
  return changed_event_counters_impl(cli, R"(SELECT EVENT_NAME, COUNT_STAR
 FROM performance_schema.events_statements_summary_by_thread_by_event_name AS e
 JOIN performance_schema.threads AS t ON (e.THREAD_ID = t.THREAD_ID)
WHERE t.PROCESSLIST_ID = CONNECTION_ID()
  AND COUNT_STAR > 0
)" + filter + R"(
ORDER BY EVENT_NAME)");
}

static stdx::expected<std::vector<std::pair<std::string, uint32_t>>, MysqlError>
changed_event_counters(MysqlClient &cli, uint64_t connection_id,
                       const std::string &filter) {
  return changed_event_counters_impl(cli, R"(SELECT EVENT_NAME, COUNT_STAR
 FROM performance_schema.events_statements_summary_by_thread_by_event_name AS e
 JOIN performance_schema.threads AS t ON (e.THREAD_ID = t.THREAD_ID)
WHERE t.PROCESSLIST_ID = )" + std::to_string(connection_id) +
                                              R"(
  AND COUNT_STAR > 0
)" + filter +
                                              R"(
ORDER BY EVENT_NAME)");
}

class Procs : public ProcessManager {
 public:
  [[nodiscard]] mysql_harness::Path get_origin() const {
    return ProcessManager::get_origin();
  }

  /**
   * shutdown and stop monitoring of processes.
   */
  void clear() {
    if (::testing::Test::HasFatalFailure() || dump_logs_) {
      dump_all();
    }

    ProcessManager::clear();
  }

  void shutdown_all() { ProcessManager::shutdown_all(); }

  ~Procs() override {
    shutdown_all();
    ensure_clean_exit();

    if (::testing::Test::HasFatalFailure() || dump_logs_) {
      dump_all();
    }
  }

  void dump_logs() { dump_logs_ = true; }

  auto wait_for_exit(
      std::chrono::milliseconds timeout = kDefaultWaitForExitTimeout) {
    return ProcessManager::wait_for_exit(timeout);
  }

 private:
  bool dump_logs_{false};
};

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

  Procs &process_manager() { return procs_; }
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
                "--innodb_autoextend_increment=1M",
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
        "--secure-file-priv=NULL",           //
        "--innodb_redo_log_capacity=8M",     // fast startups
        "--innodb_autoextend_increment=1M",  //
        "--innodb_buffer_pool_size=5M",      //
        "--gtid_mode=ON",                    // group-replication
        "--enforce_gtid_consistency=ON",     //
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

  Procs procs_;
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
  SharedRouter(TcpPortPool &port_pool, uint64_t pool_size, bool split_routes)
      : port_pool_(port_pool),
        pool_size_{pool_size},
        rest_port_{port_pool_.get_next_available()},
        split_routes_{split_routes} {}

  Procs &process_manager() { return procs_; }

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
      for (auto [route_ndx, dests] : enumerate(split_dests)) {
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
                {"connection_sharing_delay", "0"},
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
  Procs procs_;
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

  Procs &process_manager() { return procs_; }

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
  Procs procs_;

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

    for (auto [ndx, inter] : enumerate(intermediate_routers_)) {
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
    for (auto [ndx, s] : enumerate(shared_servers())) {
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

  static void TearDownTestSuite() { TestWithSharedRouter::TearDownTestSuite(); }

  static std::array<SharedServer *, kNumServers> shared_servers() {
    std::array<SharedServer *, kNumServers> o;

    // get a subset of the started servers
    for (auto [ndx, s] : enumerate(test_env->servers())) {
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

using ShareConnectionTest = ShareConnectionTestTemp<3, 128>;

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
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account()};

  const bool can_share = GetParam().can_share();
  for (auto [ndx, cli] : enumerate(clis)) {
    auto account = accounts[ndx];

    cli.username(account.username);
    cli.password(account.password);

    // wait until connection 0, 1, 2 are in the pool as 3 shall share with 0.
    if (ndx == 3 && can_share) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(3, 1s));
    }

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    // connection goes out of the pool and back to the pool again.
    if (ndx == 3 && can_share) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(3, 1s));
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
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/set_option", 2)));
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
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/set_option", 2)));
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
                              Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 4)));
    } else {
      EXPECT_THAT(*events_res, ::testing::IsEmpty());
    }
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
  // 4 connections are needed as router does round-robin over 3 endpoints
  std::array<MysqlClient, 7> clis;

  std::array<SharedServer::Account, clis.size()> accounts{
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account()};

  std::array<std::pair<uint16_t, uint64_t>, clis.size()> cli_ids{};

  const bool can_share = GetParam().can_share();
  for (auto [ndx, cli] : enumerate(clis)) {
    auto account = accounts[ndx];

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    // wait until the connection is in the pool.
    if (can_share) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
    }

    // find it on one of the servers and kill it.
    for (auto &s : shared_servers()) {
      auto cli_res = s->admin_cli();
      ASSERT_NO_ERROR(cli_res);

      auto srv_cli = std::move(*cli_res);

      auto ids_res = s->user_connection_ids(srv_cli);
      ASSERT_NO_ERROR(ids_res);

      auto ids = *ids_res;

      if (ids.empty()) continue;

      EXPECT_THAT(ids, ::testing::SizeIs(1));

      for (auto id : ids) {
        ASSERT_NO_ERROR(srv_cli.kill(id));

        cli_ids[ndx] = std::make_pair(s->server_port(), id);
      }
    }

    // wait until it is gone from the pool.
    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 1s));
  }

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
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account(),
      SharedServer::native_empty_password_account()};

  std::array<std::pair<uint16_t, uint64_t>, clis.size()> cli_ids{};

  std::map<std::pair<uint16_t, uint64_t>,
           std::vector<std::pair<std::string, uint32_t>>>
      last_events{};

  const bool can_share = GetParam().can_share();
  for (auto [ndx, cli] : enumerate(clis)) {
    SCOPED_TRACE("// connection [" + std::to_string(ndx) + "]");

    auto account = accounts[ndx];

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    // wait until the connection is in the pool.
    if (can_share) {
      size_t expected_pooled_connections = ndx < 3 ? ndx + 1 : 3;

      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(
          expected_pooled_connections, 1s));
    }

    // find the server which received the connection attempt.
    for (auto &s : shared_servers()) {
      auto cli_res = s->admin_cli();
      ASSERT_NO_ERROR(cli_res);

      auto srv_cli = std::move(*cli_res);

      auto ids_res = s->user_connection_ids(srv_cli);
      ASSERT_NO_ERROR(ids_res);

      auto ids = *ids_res;

      if (can_share) {
        EXPECT_THAT(ids, SizeIs(::testing::Lt(2)));
      }

      for (auto id : ids) {
        auto events_res = changed_event_counters(srv_cli, id);
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
      SharedServer::native_empty_password_account(),
      SharedServer::native_password_account(),
      SharedServer::caching_sha2_password_account(),
      SharedServer::caching_sha2_empty_password_account()};

  const bool can_share = GetParam().can_share();
  for (auto [ndx, cli] : enumerate(clis)) {
    auto account = accounts[ndx];

    SCOPED_TRACE("// connect[" + std::to_string(ndx) + "] as " +
                 account.username);

    cli.username(account.username);
    cli.password(account.password);

    // wait until connection 0, 1, 2 are in the pool as 3 shall share with 0.
    if (ndx == 3 && can_share) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(3, 1s));
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
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(3, 1s));
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
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/set_option", 2)));
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
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/set_option", 2)));
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
                              Pair("statement/sql/select", 1),
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

  auto account = SharedServer::native_empty_password_account();

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
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));

      for (auto &s : shared_servers()) {
        s->close_all_connections();
      }

      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 1s));
    }
  }
}

/*
 * check connections get routed to the same backends if the connection pooled.
 */
TEST_P(ShareConnectionTest, classic_protocol_connection_is_sticky_pooled) {
  // 4 connections are needed as router does round-robin over 3 endpoints
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();

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
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
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
  for (auto [ndx, cli] : enumerate(clis)) {
    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    if (can_share) {
      if (ndx == 0) {
        ASSERT_NO_ERROR(
            shared_router()->wait_for_idle_server_connections(1, 1s));
      } else if (ndx == 3) {
        ASSERT_NO_ERROR(
            shared_router()->wait_for_idle_server_connections(3, 1s));
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
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/set_option", 2)));
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
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/set_option", 2)));
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
                              Pair("statement/sql/select", 1),
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
    auto account = SharedServer::native_password_account();

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
    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(3, 1s));
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
    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(3, 1s));
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
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/set_option", 2)));
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
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/set_option", 2)));
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
                                Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 4)));
      } else {
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/set_option", 2)));
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
                                Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/select", 2),
                                Pair("statement/sql/set_option", 5)));
      } else {
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Reset Connection", 2),
                                Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 3)));
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
                                Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/select", 3),
                                Pair("statement/sql/set_option", 6)));
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

// check that CMD_KILL opens a new connection to the server.
TEST_P(ShareConnectionTest, classic_protocol_kill_zero) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  SCOPED_TRACE("// killing connection 0");
  {
    auto kill_res = cli.kill(0);
    ASSERT_ERROR(kill_res);
    EXPECT_EQ(kill_res.error().value(), 1094) << kill_res.error();
    // unknown thread id.
  }

  SCOPED_TRACE("// ping after kill");

  // nothing was killed and PING should open a new connection.
  ASSERT_NO_ERROR(cli.ping());
}

TEST_P(ShareConnectionTest, classic_protocol_kill_current_connection) {
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
    auto kill_res = cli.kill(connection_id);
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

TEST_P(ShareConnectionTest, classic_protocol_kill_fail) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto kill_res = cli.kill(0);  // should fail.
  ASSERT_ERROR(kill_res);
  EXPECT_EQ(kill_res.error().value(), 1094);  // Unknown thread id: 0
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

// cmd_list_fields -> err
TEST_P(ShareConnectionTest, classic_protocol_list_fields_succeeds) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("mysql");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto cmd_res = cli.list_fields("user");
  ASSERT_NO_ERROR(cmd_res);
}

// cmd_list_fields -> ok
TEST_P(ShareConnectionTest, classic_protocol_list_fields_fails) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("mysql");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.list_fields("does_not_exist");
    ASSERT_ERROR(cmd_res);
    EXPECT_EQ(cmd_res.error().value(), 1146) << cmd_res.error();
  }
}

TEST_P(ShareConnectionTest, classic_protocol_change_user_native_empty) {
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

TEST_P(ShareConnectionTest, classic_protocol_change_user_native) {
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

TEST_P(ShareConnectionTest, classic_protocol_change_user_caching_sha2_empty) {
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

TEST_P(ShareConnectionTest, classic_protocol_change_user_caching_sha2) {
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

TEST_P(ShareConnectionTest,
       classic_protocol_change_user_caching_sha2_with_schema) {
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

TEST_P(ShareConnectionTest,
       classic_protocol_change_user_caching_sha2_with_attributes_with_pool) {
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

TEST_P(ShareConnectionTest,
       classic_protocol_change_user_sha256_password_empty) {
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

TEST_P(ShareConnectionTest, classic_protocol_change_user_sha256_password) {
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

TEST_P(ShareConnectionTest, classic_protocol_refresh) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  EXPECT_NO_ERROR(cli.refresh());

  EXPECT_NO_ERROR(cli.refresh());
}

TEST_P(ShareConnectionTest, classic_protocol_refresh_fail) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  auto account = SharedServer::native_empty_password_account();
  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.refresh();
    ASSERT_ERROR(cmd_res);

    EXPECT_EQ(cmd_res.error().value(), 1227);  // Access Denied
  }
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
  for (auto &s : shared_servers()) {
    s->close_all_connections();
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
  for (auto &s : shared_servers()) {
    s->close_all_connections();
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
                  ElementsAre(Pair("statement/com/Reset Connection", 2),
                              Pair("statement/sql/set_option", 4)));
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
                  ElementsAre(Pair("statement/com/Reset Connection", 4),
                              Pair("statement/sql/select", 2),
                              Pair("statement/sql/set_option", 6)));
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
          ElementsAre(Pair("statement/com/Reset Connection", 3),
                      Pair("statement/sql/do", 1),
                      Pair("statement/sql/error", 1),         // CREATE TABLE
                      Pair("statement/sql/set_option", 4),    //
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
          ElementsAre(Pair("statement/com/Reset Connection", 3),  // from-pool
                      Pair("statement/sql/create_table", 1),
                      Pair("statement/sql/insert_select", 1),
                      Pair("statement/sql/set_option", 4),    // init-trackers
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

  for (auto &s : shared_servers()) {
    s->close_all_connections();
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
    SCOPED_TRACE("// checker: " + checker_name);
    for (const bool close_connection_before_verify : {false, true}) {
      SCOPED_TRACE("// close-connection-before verify: " +
                   std::to_string(close_connection_before_verify));

      for (auto &s : shared_servers()) {
        s->close_all_connections();
      }

      MysqlClient cli;

      auto account = SharedServer::native_password_account();

      cli.username(account.username);
      cli.password(account.password);

      ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                  shared_router()->port(GetParam())));

      ASSERT_NO_FATAL_FAILURE(checker->apply(cli));

      if (can_share && can_fetch_password) {
        ASSERT_NO_ERROR(
            shared_router()->wait_for_idle_server_connections(1, 1s));
      }

      if (close_connection_before_verify) {
        for (auto &s : shared_servers()) {
          s->close_all_connections();
        }
      }

      if (can_share && can_fetch_password) {
        ASSERT_NO_FATAL_FAILURE(checker->verifier()(cli));
      }
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

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Reset Connection", 3),
                              Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 5)));
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

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Reset Connection", 4),
                              Pair("statement/sql/create_table", 1),
                              Pair("statement/sql/lock_tables", 1),
                              Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 5)));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/create_table", 1),
                              Pair("statement/sql/lock_tables", 1),
                              Pair("statement/sql/select", 1)));
    }
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

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Reset Connection", 6),
                              Pair("statement/sql/create_table", 1),
                              Pair("statement/sql/lock_tables", 1),
                              Pair("statement/sql/select", 3),
                              Pair("statement/sql/set_option", 7)));
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
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Reset Connection", 1),  // from-pool
                      Pair("statement/sql/do", 1),                // DO ...()
                      Pair("statement/sql/set_option", 2)  // connect, from-pool
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
                                   // from-pool, explicit, from-pool
                                   Pair("statement/com/Reset Connection", 3),
                                   // DO GET_LOCK()
                                   Pair("statement/sql/do", 1),
                                   // events
                                   Pair("statement/sql/select", 1),
                                   // connect, from-pool, explicit, from-pool
                                   Pair("statement/sql/set_option", 4)));
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
          ElementsAre(Pair("statement/com/Reset Connection", 1),  // from-pool
                      Pair("statement/sql/begin", 1),      // START TRANSACTION
                      Pair("statement/sql/do", 1),         // DO ...()
                      Pair("statement/sql/rollback", 1),   // ROLLBACK
                      Pair("statement/sql/set_option", 2)  // connect, from-pool
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
                                   // from-pool, explicit, from-pool
                                   Pair("statement/com/Reset Connection", 3),
                                   // START TRANSACTION
                                   Pair("statement/sql/begin", 1),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // ROLLBACK
                                   Pair("statement/sql/rollback", 1),
                                   // events
                                   Pair("statement/sql/select", 1),
                                   // connect, from-pool, explicit, from-pool
                                   Pair("statement/sql/set_option", 4)));
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
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Reset Connection", 1),  // from-pool
                      Pair("statement/sql/do", 1),                // DO ...()
                      Pair("statement/sql/set_option", 2)  // connect, from-pool
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
                                   // from-pool, explicit, from-pool
                                   Pair("statement/com/Reset Connection", 3),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // events, metadata-locks
                                   Pair("statement/sql/select", 2),
                                   // connect, from-pool, explicit, from-pool
                                   Pair("statement/sql/set_option", 4)));
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
          ElementsAre(Pair("statement/com/Reset Connection", 1),  // from-pool
                      Pair("statement/sql/begin", 1),      // START TRANSACTION
                      Pair("statement/sql/do", 1),         // DO ...()
                      Pair("statement/sql/rollback", 1),   // ROLLBACK
                      Pair("statement/sql/set_option", 2)  // connect, from-pool
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
                                   // from-pool, explicit, from-pool
                                   Pair("statement/com/Reset Connection", 3),
                                   // START TRANSACTION
                                   Pair("statement/sql/begin", 1),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // ROLLBACK
                                   Pair("statement/sql/rollback", 1),
                                   // events, metadata-locks
                                   Pair("statement/sql/select", 2),
                                   // connect, from-pool, explicit, from-pool
                                   Pair("statement/sql/set_option", 4)));
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

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("DO SERVICE_GET_READ_LOCKS('ns', 'lock1', 0)");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Reset Connection", 1),  // from-pool
                      Pair("statement/sql/do", 1),                // DO ...()
                      Pair("statement/sql/set_option", 2)  // connect, from-pool
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
                                        "SHARED", "GRANTED")));
  }

  ASSERT_NO_ERROR(cli.reset_connection());

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // from-pool, explicit, from-pool
                                   Pair("statement/com/Reset Connection", 3),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // events, metadata-locks
                                   Pair("statement/sql/select", 2),
                                   // connect, from-pool, explicit, from-pool
                                   Pair("statement/sql/set_option", 4)));
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

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("START TRANSACTION");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto cmd_res = cli.query("DO SERVICE_GET_READ_LOCKS('ns', 'lock1', 0)");
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
          ElementsAre(Pair("statement/com/Reset Connection", 1),  // from-pool
                      Pair("statement/sql/begin", 1),      // START TRANSACTION
                      Pair("statement/sql/do", 1),         // DO ...()
                      Pair("statement/sql/rollback", 1),   // ROLLBACK
                      Pair("statement/sql/set_option", 2)  // connect, from-pool
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
                                        "SHARED", "GRANTED")));
  }

  ASSERT_NO_ERROR(cli.reset_connection());

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // from-pool, explicit, from-pool
                                   Pair("statement/com/Reset Connection", 3),
                                   // START TRANSACTION
                                   Pair("statement/sql/begin", 1),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // ROLLBACK
                                   Pair("statement/sql/rollback", 1),
                                   // events, metadata-locks
                                   Pair("statement/sql/select", 2),
                                   // connect, from-pool, explicit, from-pool
                                   Pair("statement/sql/set_option", 4)));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // from-pool, explicit, from-pool
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
 * FR6.1: VERSION_TOKENS_LOCK_SHARED(), no-share until reset
 */
TEST_P(ShareConnectionTest, classic_protocol_version_tokens_lock_shared) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("DO VERSION_TOKENS_LOCK_SHARED('token1', 0)");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Reset Connection", 1),  // from-pool
                      Pair("statement/sql/do", 1),                // DO ...()
                      Pair("statement/sql/set_option", 2)  // connect, from-pool
                      ));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/do", 1)  // DO ...()
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

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(
                                "LOCKING SERVICE", "version_token_locks",
                                "token1", "SHARED", "GRANTED")));
  }

  ASSERT_NO_ERROR(cli.reset_connection());

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // from-pool, explicit, from-pool
                                   Pair("statement/com/Reset Connection", 3),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // events, metadata-locks
                                   Pair("statement/sql/select", 2),
                                   // connect, from-pool, explicit, from-pool
                                   Pair("statement/sql/set_option", 4)));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // from-pool
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
 * FR6.1: VERSION_TOKENS_LOCK_EXCLUSIVE(), no-share until reset
 */
TEST_P(ShareConnectionTest, classic_protocol_version_tokens_lock_exclusive) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");
  cli.use_schema("testing");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  {
    auto cmd_res = cli.query("DO VERSION_TOKENS_LOCK_EXCLUSIVE('token1', 0)");
    ASSERT_NO_ERROR(cmd_res);
  }

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(
          *events_res,
          ElementsAre(Pair("statement/com/Reset Connection", 1),  // from-pool
                      Pair("statement/sql/do", 1),                // DO ...()
                      Pair("statement/sql/set_option", 2)  // connect, from-pool
                      ));
    } else {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/sql/do", 1)  // DO ...()
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

    EXPECT_THAT(*query_res, ElementsAre(ElementsAre(
                                "LOCKING SERVICE", "version_token_locks",
                                "token1", "EXCLUSIVE", "GRANTED")));
  }

  ASSERT_NO_ERROR(cli.reset_connection());

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res, ElementsAre(
                                   // from-pool, explicit, from-pool
                                   Pair("statement/com/Reset Connection", 3),
                                   // DO ...()
                                   Pair("statement/sql/do", 1),
                                   // events, metadata-locks
                                   Pair("statement/sql/select", 2),
                                   // connect, from-pool, explicit, from-pool
                                   Pair("statement/sql/set_option", 4)));
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

TEST_P(ShareConnectionTest, classic_protocol_prepare_fail) {
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  const bool can_share = GetParam().can_share();

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  auto res = cli.prepare("SEL ?");
  ASSERT_ERROR(res);
  EXPECT_EQ(res.error().value(), 1064) << res.error();  // Syntax Error

  {
    auto events_res = changed_event_counters(cli);
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      EXPECT_THAT(*events_res,
                  ElementsAre(Pair("statement/com/Prepare", 1),
                              Pair("statement/com/Reset Connection", 2),
                              Pair("statement/sql/set_option", 3)  //
                              ));
    } else {
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/com/Prepare", 1)));
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
                              // from-pool
                              Pair("statement/com/Reset Connection", 1),
                              // connect, from-pool
                              Pair("statement/sql/set_option", 2)  //
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
                              // from-pool, events, from-pool
                              Pair("statement/com/Reset Connection", 3),
                              // events
                              Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 4)));
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
                              Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sql/set_option", 2)));
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
                              // from-pool
                              Pair("statement/com/Reset Connection", 1),
                              // connect, from-pool
                              Pair("statement/sql/set_option", 2)  //
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
                              // from-pool, events, from-pool
                              Pair("statement/com/Reset Connection", 3),
                              // events
                              Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 4)));
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
                              // from-pool
                              Pair("statement/com/Reset Connection", 1),
                              Pair("statement/sp/stmt", 2),
                              // connect, from-pool
                              Pair("statement/sql/set_option", 2)  //
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
                              // from-pool, events, from-pool
                              Pair("statement/com/Reset Connection", 3),
                              Pair("statement/sp/stmt", 2),
                              // events
                              Pair("statement/sql/select", 1),
                              Pair("statement/sql/set_option", 4)));
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

  auto account = SharedServer::native_empty_password_account();
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

  auto account = SharedServer::native_empty_password_account();
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

  auto account = SharedServer::native_empty_password_account();
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
  SCOPED_TRACE("// connecting to server");
  MysqlClient cli;

  cli.username("root");
  cli.password("");

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));

  EXPECT_NO_ERROR(cli.set_server_option(MYSQL_OPTION_MULTI_STATEMENTS_ON));
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
  ASSERT_NO_ERROR(cli.query("RESET MASTER"));

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
// mysql_native_password
//

TEST_P(ShareConnectionTest, classic_protocol_native_user_no_pass) {
  auto account = SharedServer::native_empty_password_account();

  MysqlClient cli;

  cli.username(account.username);
  cli.password(account.password);

  ASSERT_NO_ERROR(
      cli.connect(shared_router()->host(), shared_router()->port(GetParam())));
}

TEST_P(ShareConnectionTest, classic_protocol_native_user_with_pass) {
  auto account = SharedServer::native_password_account();

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

//
// caching_sha2_password
//

TEST_P(ShareConnectionTest, classic_protocol_caching_sha2_password_with_pass) {
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

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

using ShareConnectionTinyPoolOneServerTest = ShareConnectionTestTemp<1, 1>;
using ShareConnectionTinyPoolTwoServersTest = ShareConnectionTestTemp<2, 1>;
using ShareConnectionSmallPoolTwoServersTest = ShareConnectionTestTemp<2, 2>;
using ShareConnectionSmallPoolFourServersTest = ShareConnectionTestTemp<4, 2>;

/*
 * 1. cli1 connects and starts long running a query
 *    - connect
 *    - query (started)
 * 2. cli2 connects and starts short running query
 *    - connect
 *    - query
 *    - add to pool
 * 3. cli1 query finishes
 *    - pool is full, not added.
 * 4. cli3 connects and starts long running query
 *    - reuse from pool
 *    - start query
 * 5. cli2 runs a query
 *    - connect's a new connection
 *    - runs query
 */
TEST_P(ShareConnectionTinyPoolOneServerTest, overlapping_connections) {
  MysqlClient cli1, cli2, cli3;

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  {
    auto account = SharedServer::native_password_account();

    cli1.username(account.username);
    cli1.password(account.password);

    ASSERT_NO_ERROR(cli1.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));

    // wait until the connection is in the pool.
    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
    }
  }

  // - set-option
  // - add to pool
  // - reset-connection
  // - set-option
  // - do

  ASSERT_NO_ERROR(cli1.send_query("DO SLEEP(0.2)"));

  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 1s));

  {
    auto account = SharedServer::native_password_account();

    cli2.username(account.username);
    cli2.password(account.password);

    ASSERT_NO_ERROR(cli2.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));

    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
    }
  }
  // - connect
  // - set-option
  // - add to pool

  std::string pooled_connection_id{};
  {
    auto cmd_res = query_one_result(cli2, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    auto result = std::move(*cmd_res);
    ASSERT_THAT(result, SizeIs(1));     // 1 row
    ASSERT_THAT(result[0], SizeIs(1));  // first row, has 1 field.

    pooled_connection_id = result[0][0];

    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
    }
  }

  // - reset-connection
  // - set-option
  // - do
  ASSERT_NO_ERROR(cli1.read_query_result());

  // cli2 should be in the pool now
  // cli1 should be not in pool, but still attached
  {
    auto events_res = changed_event_counters(cli1);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        // cli1: set-option
        // cli1: reset-connection + set-option + do (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/do", 1),
                                Pair("statement/sql/set_option", 2)));
      } else {
        // cli1: set-option
        // cli1: do (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/do", 1),
                                Pair("statement/sql/set_option", 1)));
      }
    } else {
      // no sharing possible, router is not injection SET statements.
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/do", 1)));
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
                    ElementsAre(Pair("statement/com/Reset Connection", 2),
                                Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 3)));
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

  // cli3 takes connection from the pool.
  {
    auto account = SharedServer::native_password_account();

    cli3.username(account.username);
    cli3.password(account.password);

    ASSERT_NO_ERROR(cli3.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));

    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
    }
  }

  // start a long running query, takes the connection from the pool.
  ASSERT_NO_ERROR(cli3.send_query("SELECT SLEEP(0.2), CONNECTION_ID()"));

  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 1s));

  // opens a new connection as cli3 grapped the pooled connection.
  {
    auto cmd_res = query_one_result(cli2, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    if (can_share) {
      if (can_fetch_password) {
        EXPECT_THAT(
            *cmd_res,
            ElementsAre(ElementsAre(::testing::Ne(pooled_connection_id))));

        ASSERT_NO_ERROR(
            shared_router()->wait_for_idle_server_connections(1, 1s));
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
        EXPECT_THAT(result, ElementsAre(ElementsAre(::testing::_,
                                                    pooled_connection_id)));
      }
    }
  }
}

TEST_P(ShareConnectionTinyPoolOneServerTest,
       overlapping_connections_different_accounts) {
  MysqlClient cli1, cli2, cli3;

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  {
    auto account = SharedServer::native_password_account();

    cli1.username(account.username);
    cli1.password(account.password);

    ASSERT_NO_ERROR(cli1.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));

    // wait until the connection is in the pool.
    if (can_share && can_fetch_password) {
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
    }
  }
  // - set-option
  // - add to pool

  SCOPED_TRACE("// block the 1st connection for a bit.");
  ASSERT_NO_ERROR(cli1.send_query("DO SLEEP(0.2)"));
  // - reset-connection
  // - set-option
  // - do
  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 1s));

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
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
    }
  }
  // - connect
  // - set-option
  // - add to pool

  SCOPED_TRACE("// check connection id of 2nd connection.");
  std::string pooled_connection_id{};
  {
    auto cmd_res = query_one_result(cli2, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    auto result = std::move(*cmd_res);
    ASSERT_THAT(result, SizeIs(1));
    ASSERT_THAT(result[0], SizeIs(1));
    pooled_connection_id = result[0][0];
  }

  if (can_share && can_fetch_password) {
    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
  }

  // - reset-connection
  // - set-option
  // - select

  SCOPED_TRACE("// wait until 1st connection finished SLEEP()ing.");
  ASSERT_NO_ERROR(cli1.read_query_result());

  // cli2 should be in the pool now
  // cli1 should be not in pool, but still attached
  {
    auto events_res = changed_event_counters(cli1);  // the (+ select)
    ASSERT_NO_ERROR(events_res);

    if (can_share) {
      if (can_fetch_password) {
        // cli1: set-option
        // cli1: reset-connection + set-option + do (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Reset Connection", 1),
                                Pair("statement/sql/do", 1),
                                Pair("statement/sql/set_option", 2)));
      } else {
        // cli1: set-option
        // cli1: do (+ select)
        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/sql/do", 1),
                                Pair("statement/sql/set_option", 1)));
      }
    } else {
      // no sharing possible, router is not injection SET statements.
      EXPECT_THAT(*events_res, ElementsAre(Pair("statement/sql/do", 1)));
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
                    ElementsAre(Pair("statement/com/Reset Connection", 2),
                                Pair("statement/sql/select", 1),
                                Pair("statement/sql/set_option", 3)));
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
    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
  }

  // cli3 takes connection from the pool.
  {
    auto account = SharedServer::native_password_account();

    cli3.username(account.username);
    cli3.password(account.password);

    ASSERT_NO_ERROR(cli3.connect(shared_router()->host(),
                                 shared_router()->port(GetParam())));
  }

  if (can_share && can_fetch_password) {
    ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
  }

  SCOPED_TRACE(
      "// start a long running query, takes the connection from the pool.");
  ASSERT_NO_ERROR(cli3.send_query("SELECT SLEEP(0.2), CONNECTION_ID()"));

  ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(0, 1s));

  SCOPED_TRACE(
      "// opens a new connection as cli3 grapped the pooled connection.");
  {
    auto cmd_res = query_one_result(cli2, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    if (can_share) {
      if (can_fetch_password) {
        EXPECT_THAT(
            *cmd_res,
            ElementsAre(ElementsAre(::testing::Ne(pooled_connection_id))));

        ASSERT_NO_ERROR(
            shared_router()->wait_for_idle_server_connections(1, 1s));
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
        EXPECT_THAT(result, ElementsAre(ElementsAre(::testing::_,
                                                    pooled_connection_id)));
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
       classic_protocol_kill_other_connection) {
  SCOPED_TRACE("// connecting to server");

  // open two connections on the same server:
  //
  // - one to be killed
  // - one to kill
  std::array<MysqlClient, 2> clis;
  std::array<uint32_t, 2> conn_ids;

  for (auto [ndx, cli] : enumerate(clis)) {
    cli.username("root");
    cli.password("");

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    // block the connection to ensure that even with connection-sharing, a new
    // connection gets opened that can be successfully killed.
    ASSERT_NO_ERROR(cli.query("SET @block_connection = 1"));

    auto conn_id_res = query_one<1>(cli, "SELECT CONNECTION_ID()");
    ASSERT_NO_ERROR(conn_id_res);

    auto conn_num_res = from_string((*conn_id_res)[0]);
    ASSERT_NO_ERROR(conn_num_res);

    conn_ids[ndx] = *conn_num_res;
  }

  // there should be no pooling.
  ASSERT_NE(conn_ids[0], conn_ids[1]);

  ASSERT_NO_ERROR(clis[0].kill(conn_ids[1]));

  // should fail as connection is killed.
  ASSERT_ERROR(clis[1].query("DO 1"));
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

  SCOPED_TRACE("// connecting to server");

  SharedServer replica_server(port_pool_);

  replica_server.prepare_datadir();
  replica_server.spawn_server(
      {"--report-host=some_funky_host", "--server-id=2"});

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
    ASSERT_LT(cur, end);

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

  auto source_res = shared_servers()[0]->admin_cli();
  ASSERT_NO_ERROR(source_res);

  auto source = std::move(*source_res);

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

  SCOPED_TRACE("// connecting to server");

  SharedServer recipient_server(port_pool_);

  recipient_server.prepare_datadir();
  recipient_server.spawn_server();

  SCOPED_TRACE("// connection to the recipient server directly");
  auto recipient_res = recipient_server.admin_cli();
  ASSERT_NO_ERROR(recipient_res);

  auto recipient = std::move(*recipient_res);
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
  c.pid, c.state
 FROM performance_schema.clone_status AS c
)");
      ASSERT_NO_ERROR(cmd_res);

      auto result = *cmd_res;
      ASSERT_THAT(result, Not(IsEmpty()));
      ASSERT_EQ(result[0].size(), 2);

      if (result[0][1] == "In Progress") {
        ASSERT_NO_ERROR(monitor.query("KILL QUERY " + result[0][0]));
        break;
      }

      std::this_thread::sleep_for(100ms);
    }
  }

  {
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

    // router will return the response and inject a SHOW WARNINGS
    // before returning the connection to the pool.
    //
    // c->r   : stmt
    //    r->s: stmt
    //    r<-s: ok, warnings
    // c<-r   : ok, warnings
    //    r->s: SHOW WARNINGS
    //    r<-s: row, ... row_end
    //    r--s: idle ... to pool
    //
    // As the checker may expect that the connection is pooled right after the
    // client gets the Ok, it may sent the next command while the SHOW WARNINGS
    // still runs.
    //
    // By sending a ping, it can be ensured that the SHOW WARNINGS finished and
    // the connection is in the pool.
    //
    // c->r   : stmt
    //    r->s: stmt
    //    r<-s: ok, warnings
    // c<-r   : ok, warnings
    //    r->s: SHOW WARNINGS
    // c->r   : ping
    //    r<-s: row, ... row_end
    //    r->s: ping
    //    r<-s: ok
    // c<-r   : ok
    //    r--s: idle ... to pool

    ASSERT_NO_ERROR(cli.ping());
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
    accounts.push_back(SharedServer::native_password_account());

    scenarios.emplace_back("one account", std::move(accounts));
  }

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::native_password_account());
    accounts.push_back(SharedServer::native_password_account());

    scenarios.emplace_back("same account, twice", std::move(accounts));
  }

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::native_password_account());
    accounts.push_back(SharedServer::native_empty_password_account());
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
        for (auto &s : shared_servers()) {
          s->flush_privileges();       // reset the auth-cache
          s->close_all_connections();  // reset the router's connection-pool
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
          // only close server's connections the connections are expected to be
          // in the pool.
          for (auto &s : shared_servers()) {
            s->close_all_connections();
          }

          // wait a bit to give the connection pool time to notice that the
          // connection got closed.
          std::this_thread::sleep_for(50ms);
        }

        // verify variables that were set are reapplied.
        for (auto [ndx, cli] : enumerate(clis)) {
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

// check a statement blocks sharing, and a 2nd statement unblocks sharing again.
class BlockChecker : public Checker {
 public:
  using test_values_type = std::vector<std::pair<std::string, std::string>>;

  BlockChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {}

  void apply(MysqlClient &cli) override {
    auto block_stmt = test_values_[ndx_].first;

    ASSERT_NO_ERROR(cli.query(block_stmt));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    return [unblock_stmt = test_values_[ndx_].second](MysqlClient &cli) {
      // check if the connection is NOT shareable.
      // should not add another "Reset"

      ASSERT_NO_ERROR(cli.query("DO 1"));

      ASSERT_NO_ERROR(cli.query(unblock_stmt));

      // check if the connection is shareable again.
      // ... an additional 'Reset Connection'
      {
        auto events_res =
            changed_event_counters(cli, "AND EVENT_NAME LIKE '%/com/%'");
        ASSERT_NO_ERROR(events_res);

        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Reset Connection", 2)));
      }
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

// check a statement blocks ... and 2nd statement does not unblock sharing.
class NotUnblockChecker : public Checker {
 public:
  using test_values_type = std::vector<std::pair<std::string, std::string>>;

  NotUnblockChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {}

  void apply(MysqlClient &cli) override {
    auto block_stmt = test_values_[ndx_].first;

    ASSERT_NO_ERROR(cli.query(block_stmt));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    return [unblock_stmt = test_values_[ndx_].second](MysqlClient &cli) {
      // check if the connection is NOT shareable.
      {
        auto events_res =
            changed_event_counters(cli, "AND EVENT_NAME LIKE '%/com/%'");
        ASSERT_NO_ERROR(events_res);

        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Reset Connection", 1)));
      }

      ASSERT_NO_ERROR(cli.query(unblock_stmt));

      // check if the connection is still not shareable
      {
        auto events_res =
            changed_event_counters(cli, "AND EVENT_NAME LIKE '%/com/%'");
        ASSERT_NO_ERROR(events_res);

        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Reset Connection", 1)));
      }
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
 * check that "reset-connection" allows sharing connections again.
 */
class ResetChecker : public Checker {
 public:
  using test_values_type = std::vector<std::string>;

  ResetChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {}

  void apply(MysqlClient &cli) override {
    auto stmt = test_values_[ndx_];

    // + reset (+ select)
    {
      auto cmd_res = cli.query(stmt);
      ASSERT_NO_ERROR(cmd_res) << "\nstatement: " << stmt;

      auto results = std::move(*cmd_res);
      for (const auto &result : results) {
        if (result.field_count() > 0) {
          for (const auto &row : result.rows()) {
            // drain the resultset
            (void)row;
          }
        }
      }
    }
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    return [](MysqlClient &cli) {
      // check if the connection is NOT shareable.
      {
        auto events_res =
            changed_event_counters(cli, "AND EVENT_NAME LIKE '%/com/%'");
        ASSERT_NO_ERROR(events_res);

        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Reset Connection", 1)));
      }
      // - (+ select)

      ASSERT_NO_ERROR(cli.reset_connection());
      // - reset

      // check if the connection is sharable again.
      //
      // - reset (+ select)
      {
        auto events_res =
            changed_event_counters(cli, "AND EVENT_NAME LIKE '%/com/%'");
        ASSERT_NO_ERROR(events_res);

        EXPECT_THAT(*events_res,
                    ElementsAre(Pair("statement/com/Reset Connection", 3)));
      }
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

// check a statement fails when not run in a transaction, but works if in a
// transaction.
class FailsIfSharableChecker : public Checker {
 public:
  using test_values_type = std::vector<std::string>;

  FailsIfSharableChecker(test_values_type test_values)
      : test_values_(std::move(test_values)) {}

  void apply(MysqlClient &cli) override {
    auto failing_stmt = test_values_[ndx_];

    // + reset (+ select)
    ASSERT_ERROR(cli.query(failing_stmt));
  }

  std::function<void(MysqlClient &cli)> verifier() override {
    return [stmt = test_values_[ndx_]](MysqlClient &cli) {
      ASSERT_NO_ERROR(cli.query("BEGIN"));

      {
        auto cmd_res = cli.query(stmt);
        ASSERT_NO_ERROR(cmd_res);

        auto results = std::move(*cmd_res);
        for (const auto &result : results) {
          if (result.field_count() > 0) {
            for (const auto &row : result.rows()) {
              // drain the resultset
              (void)row;
            }
          }
        }
      }

      ASSERT_NO_ERROR(cli.query("ROLLBACK"));
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

/**
 * check that a connection isn't sharable if certain queries are sent.
 *
 *
 *
 * testref: WL12772::RT_MPX_UNSHARABLE_TRIGGER
 */
TEST_P(ShareConnectionTinyPoolOneServerTest, not_sharable) {
  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  if (!test_env->run_slow_tests() && GetParam().redundant_combination()) {
    GTEST_SKIP() << "skipped as RUN_SLOW_TESTS environment-variable is not set";
  }

  if (!can_share) {
    GTEST_SKIP() << "configuration doesn't allow sharing";
  }
  if (!can_fetch_password) {
    GTEST_SKIP() << "can't fetch passwords and therefore not share";
  }

  // checkers
  std::vector<std::pair<std::string, std::unique_ptr<Checker>>> checkers;

  // FR5.1
  checkers.emplace_back(
      "begin-commit", std::make_unique<BlockChecker>(
                          BlockChecker::test_values_type{{"BEGIN", "COMMIT"}}));

  checkers.emplace_back(
      "begin-rollback",
      std::make_unique<BlockChecker>(
          BlockChecker::test_values_type{{"BEGIN", "ROLLBACK"}}));

  checkers.emplace_back(
      "start-transaction-rollback",
      std::make_unique<BlockChecker>(
          BlockChecker::test_values_type{{"START TRANSACTION", "ROLLBACK"}}));

  checkers.emplace_back(
      "start-transaction-reset",
      std::make_unique<ResetChecker>(
          ResetChecker::test_values_type{"START TRANSACTION"}));

  // FR5.2
  checkers.emplace_back(
      "lock-tables",
      std::make_unique<BlockChecker>(BlockChecker::test_values_type{
          {"LOCK TABLES testing.t1 READ", "UNLOCK TABLES"}}));

  // FR5.3
  checkers.emplace_back(
      "set-isolation-level-rollback",
      std::make_unique<BlockChecker>(BlockChecker::test_values_type{
          {"SET TRANSACTION ISOLATION LEVEL SERIALIZABLE", "ROLLBACK"}}));

  checkers.emplace_back(
      "flush-all-tables-with-read-lock",
      std::make_unique<ResetChecker>(
          ResetChecker::test_values_type{{"FLUSH TABLES WITH READ LOCK"}}));

  // session-tracker doesn't report global-locks (yet).
  //
  // FLUSH TABLES WITH READ LOCK    -- no session-tracker, but blocks sharing
  // UNLOCK TABLES                  -- does not unblock sharing
  //
  // FLUSH TABLES t1 WITH READ LOCK -- session-tracker, blocks sharing
  // UNLOCK TABLES                  -- unblocks sharing
  //
  checkers.emplace_back(
      "flush-all-tables-with-read-lock-unlock",
      std::make_unique<NotUnblockChecker>(NotUnblockChecker::test_values_type{
          {"FLUSH TABLES WITH READ LOCK", "UNLOCK TABLES"}}));

  checkers.emplace_back(
      "flush-some-tables-with-read-lock",
      std::make_unique<BlockChecker>(BlockChecker::test_values_type{
          {"FLUSH TABLES testing.t1 WITH READ LOCK", "UNLOCK TABLES"}}));

  checkers.emplace_back(
      "flush-some-tables-for-export",
      std::make_unique<BlockChecker>(BlockChecker::test_values_type{
          {"FLUSH TABLES testing.t1 FOR EXPORT", "UNLOCK TABLES"}}));

  checkers.emplace_back(
      "lock-instance-for-backup",
      std::make_unique<ResetChecker>(
          ResetChecker::test_values_type{{"LoCK instance for backup"}}));

  checkers.emplace_back(
      "set-user-var-rollback",
      std::make_unique<NotUnblockChecker>(
          NotUnblockChecker::test_values_type{{"SET @user := 1", "ROLLBACK"}}));

  checkers.emplace_back(
      "set-isolation-level-reset",
      std::make_unique<ResetChecker>(ResetChecker::test_values_type{
          "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE"}));

  checkers.emplace_back("set-user-var-eq-reset",
                        std::make_unique<ResetChecker>(
                            ResetChecker::test_values_type{"SET @user = 1"}));

  checkers.emplace_back("set-user-var-assign-reset",
                        std::make_unique<ResetChecker>(
                            ResetChecker::test_values_type{"SET @user := 1"}));

  checkers.emplace_back(
      "select-user-var-reset",
      std::make_unique<ResetChecker>(
          ResetChecker::test_values_type{"SELECT @user := 1"}));

  checkers.emplace_back(
      "select-into-user-var-reset",
      std::make_unique<ResetChecker>(
          ResetChecker::test_values_type{"SELECT 1 INTO @user"}));

  // FR6.1
  checkers.emplace_back(
      "get-lock", std::make_unique<ResetChecker>(
                      ResetChecker::test_values_type{"DO GET_LOCK('abc', 0)"}));

  checkers.emplace_back(
      "service-get-write-locks",
      std::make_unique<ResetChecker>(ResetChecker::test_values_type{
          "DO service_get_WRITE_locks('ns', 'lock1', 0)"}));

  checkers.emplace_back(
      "service-get-read-locks",
      std::make_unique<ResetChecker>(ResetChecker::test_values_type{
          "SELECT service_get_READ_locks('ns', 'lock2', 0)"}));

  // FR6.2
  checkers.emplace_back(
      "create-temp-table",
      std::make_unique<ResetChecker>(ResetChecker::test_values_type{
          "create temporary table testing.temp ( id int )"}));

  // FR6.3
  checkers.emplace_back(
      "prepare-stmt-reset",
      std::make_unique<ResetChecker>(
          ResetChecker::test_values_type{"PREPARE stmt FROM 'SELECT 1'"}));

  // FR6.4
  checkers.emplace_back(
      "sql-calc-found-rows",
      std::make_unique<ResetChecker>(ResetChecker::test_values_type{
          "SELECT SQL_CALC_FOUND_ROWS * FROM testing.t1 LIMIT 0"}));

  // scenarios
  std::vector<std::pair<std::string, std::vector<SharedServer::Account>>>
      scenarios;

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::native_password_account());

    scenarios.emplace_back("one native account", std::move(accounts));
  }

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::native_password_account());
    accounts.push_back(SharedServer::native_password_account());

    scenarios.emplace_back("two native accounts", std::move(accounts));
  }

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::native_password_account());
    accounts.push_back(SharedServer::caching_sha2_password_account());

    scenarios.emplace_back("two different accounts", std::move(accounts));
  }

  SCOPED_TRACE("// prepare servers");

  // step: 0
  std::vector<MysqlClient> srv_clis;
  for (auto &s : shared_servers()) {
    auto cli_res = s->admin_cli();
    ASSERT_NO_ERROR(cli_res);

    auto cli = std::move(*cli_res);

    ASSERT_NO_ERROR(cli.query("DROP TABLE IF EXISTS testing.t1"));

    // the FLUSH TABLES t1 WITH READ LOCK needs a table.

    ASSERT_NO_ERROR(cli.query("CREATE TABLE testing.t1 (word varchar(20))"));

    // limit the number of connections to the backend.
    ASSERT_NO_ERROR(cli.query("SET GLOBAL max_connections = 2"));

    srv_clis.push_back(std::move(cli));
  }

  // below, tests assume there is only one server.
  ASSERT_THAT(srv_clis, SizeIs(1));

  const auto query_before_release_combinations =
      test_env->run_slow_tests() ? std::vector<bool>{false, true}
                                 : std::vector<bool>{true};

  const auto make_second_connection_not_sharable_combinations =
      test_env->run_slow_tests() ? std::vector<bool>{false, true}
                                 : std::vector<bool>{true};

  for (const auto &[scenario_name, accounts] : scenarios) {
    SCOPED_TRACE("// scenario: " + scenario_name);
    for (auto &[checker_name, checker] : checkers) {
      SCOPED_TRACE("// checker: " + checker_name);
      for (auto query_before_release : query_before_release_combinations) {
        SCOPED_TRACE("// send query before release: " +
                     (query_before_release ? "yes"s : "no"s));
        for (auto make_second_connection_not_sharable :
             make_second_connection_not_sharable_combinations) {
          SCOPED_TRACE("// make second connection not sharable: " +
                       (make_second_connection_not_sharable ? "yes"s : "no"s));

          for (auto [ndx, s] : enumerate(shared_servers())) {
            SCOPED_TRACE("// reset server state: " +
                         std::to_string(s->server_port()));

            auto &srv_cli = srv_clis[ndx];

            // reset the auth-cache
            ASSERT_NO_FATAL_FAILURE(s->flush_privileges(srv_cli));

            // reset the router's connection-pool
            ASSERT_NO_FATAL_FAILURE(s->close_all_connections(srv_cli));
          }

          uint16_t connection_id{};

          std::vector<std::pair<std::string, uint32_t>> events;
          // step: 1
          {
            auto account = accounts[0];

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
            }

            {
              auto ids_res = SharedServer::user_connection_ids(srv_clis[0]);
              ASSERT_NO_ERROR(ids_res);
              ASSERT_THAT(*ids_res, SizeIs(1));

              connection_id = (*ids_res)[0];
            }

            // step: 2
            SCOPED_TRACE("// checker::apply");
            ASSERT_NO_FATAL_FAILURE(checker->apply(cli));

            // run a failing query which shouldn't end up in the pool.
            //
            // step: 6
            if (accounts.size() > 1) {
              auto account = accounts[1];

              SCOPED_TRACE("// account: " + account.username);
              MysqlClient cli2;
              {
                cli2.set_option(MysqlClient::GetServerPublicKey(true));
                cli2.username(account.username);
                cli2.password(account.password);

                // step: 7
                auto connect_res = cli2.connect(
                    shared_router()->host(), shared_router()->port(GetParam()));
                ASSERT_ERROR(connect_res);
              }
            }

            // step: 12
            //
            // check if an statement between BLOCK/UNBLOCK has no impact.
            if (query_before_release) {
              ASSERT_NO_ERROR(cli.query("DO 1"));

              {
                auto ids_res = SharedServer::user_connection_ids(srv_clis[0]);
                ASSERT_NO_ERROR(ids_res);
                ASSERT_THAT(*ids_res, SizeIs(1));

                EXPECT_EQ(connection_id, (*ids_res)[0]);
              }
            }

            // step: 15
            SCOPED_TRACE("// checker::verify");
            // verify the connection is not shared.
            checker->verifier()(cli);

            SCOPED_TRACE("// capture the current com-events");
            {
              auto events_res = changed_event_counters(
                  srv_clis[0], connection_id, "AND EVENT_NAME LIKE '%/com/%'");
              ASSERT_NO_ERROR(events_res);

              events = *events_res;
            }
            // step: 19
            //
            // disconnect cli
          }

          SCOPED_TRACE("// wait until connection is pooled.");
          ASSERT_NO_ERROR(
              shared_router()->wait_for_idle_server_connections(1, 1s));

          // step: 21
          if (accounts.size() > 1) {
            SCOPED_TRACE("// check that connection from the pool is sharable");
            auto account = accounts[1];

            // step: 22
            SCOPED_TRACE("// account: " + account.username);
            MysqlClient cli;
            {
              cli.set_option(MysqlClient::GetServerPublicKey(true));
              cli.username(account.username);
              cli.password(account.password);

              auto connect_res = cli.connect(shared_router()->host(),
                                             shared_router()->port(GetParam()));
              if (!connect_res) {
                // auth may fail with DISABLED as the router has no public-key
                // cert
                GTEST_SKIP() << connect_res.error();
              }
              ASSERT_NO_ERROR(connect_res);
            }

            // should be pooled again as it is reusable.
            ASSERT_NO_ERROR(
                shared_router()->wait_for_idle_server_connections(1, 1s));

            SCOPED_TRACE("// check the previous connection was reused.");
            {
              auto ids_res = SharedServer::user_connection_ids(srv_clis[0]);
              ASSERT_NO_ERROR(ids_res);
              ASSERT_THAT(*ids_res, SizeIs(1));

              EXPECT_EQ(connection_id, (*ids_res)[0]);
            }

            // step: 24
            if (make_second_connection_not_sharable) {
              // step: 25
              //
              // + reset
              checker->apply(cli);

              // connection should stay attached to the connection and therefore
              // not be pooled.
              ASSERT_NO_ERROR(
                  shared_router()->wait_for_idle_server_connections(0, 1s));
            }

            // step: 27
            //
            // a simple query to see if the connection still works.
            {
              // + reset if !make_second_connection_not_sharable
              ASSERT_NO_ERROR(cli.query("DO 1"));
            }

            {
              // connect directly to the backend as the client connection may
              // use LOCK TABLES
              auto &srv_cli = srv_clis[0];

              auto events_res = changed_event_counters(
                  srv_cli, connection_id, "AND EVENT_NAME LIKE '%/com/%'");
              ASSERT_NO_ERROR(events_res);
              if (can_share) {
                EXPECT_EQ(events[0].first, "statement/com/Reset Connection");

                // cli[0]
                // - connect
                // - ...
                // cli[1]
                // - change-user
                // - reset-connection
                // - DO 1
                EXPECT_THAT(*events_res,
                            ElementsAre(Pair("statement/com/Change user", 1),
                                        Pair("statement/com/Reset Connection",
                                             events[0].second + 1)));
              } else {
                EXPECT_THAT(*events_res, ::testing::IsEmpty());
              }
            }
            // step: 28
            //
            // disconnect
          }

          // ... and check that the connection is back in the pool.
          SCOPED_TRACE("// should be back in the pool");
          ASSERT_NO_ERROR(
              shared_router()->wait_for_idle_server_connections(1, 1s));
        }
      }
    }
  }
}

TEST_P(ShareConnectionTinyPoolOneServerTest, forbidden_statements_if_sharing) {
  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  if (!test_env->run_slow_tests() && GetParam().redundant_combination()) {
    GTEST_SKIP() << "skipped as RUN_SLOW_TESTS environment-variable is not set";
  }

  if (!can_share) {
    GTEST_SKIP() << "configuration doesn't allow sharing";
  }
  if (!can_fetch_password) {
    GTEST_SKIP() << "can't fetch passwords and therefore not share";
  }

  // checkers
  std::vector<std::pair<std::string, std::unique_ptr<Checker>>> checkers;

  // FR7.1
  checkers.emplace_back("get-diagnostics",
                        std::make_unique<FailsIfSharableChecker>(
                            FailsIfSharableChecker::test_values_type{
                                "GET DIAGNOSTICS @p1 = NUMBER"}));

  // FR7.2
  checkers.emplace_back(
      "last-insert-id",
      std::make_unique<FailsIfSharableChecker>(
          FailsIfSharableChecker::test_values_type{"SELECT LAST_INSERT_ID()"}));

  // scenarios
  std::vector<std::pair<std::string, std::vector<SharedServer::Account>>>
      scenarios;

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::native_password_account());

    scenarios.emplace_back("one native account", std::move(accounts));
  }

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::native_password_account());
    accounts.push_back(SharedServer::native_password_account());

    scenarios.emplace_back("two native accounts", std::move(accounts));
  }

  {
    std::vector<SharedServer::Account> accounts;
    accounts.push_back(SharedServer::native_password_account());
    accounts.push_back(SharedServer::caching_sha2_password_account());

    scenarios.emplace_back("two different accounts", std::move(accounts));
  }

  for (const auto &[scenario_name, accounts] : scenarios) {
    SCOPED_TRACE("// scenario: " + scenario_name);
    for (auto &[checker_name, checker] : checkers) {
      SCOPED_TRACE("// checker: " + checker_name);

      for (auto &s : shared_servers()) {
        s->flush_privileges();       // reset the auth-cache
        s->close_all_connections();  // reset the router's connection-pool
      }

      std::string connection_id;

      std::vector<std::pair<std::string, uint32_t>> events;
      {
        auto account = accounts[0];

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
        }

        SCOPED_TRACE("// checker::apply");
        ASSERT_NO_FATAL_FAILURE(checker->apply(cli));

        // run a failing query which shouldn't end up in the pool.
        if (accounts.size() > 1) {
          auto account = accounts[1];

          SCOPED_TRACE("// account: " + account.username);
          MysqlClient cli2;
          {
            cli2.set_option(MysqlClient::GetServerPublicKey(true));
            cli2.username(account.username);
            cli2.password("wrong password");

            auto connect_res = cli2.connect(shared_router()->host(),
                                            shared_router()->port(GetParam()));
            ASSERT_ERROR(connect_res);
          }
        }

        SCOPED_TRACE("// checker::verify");
        // verify the connection is not shared.
        checker->verifier()(cli);

        {
          auto row_res = query_one<1>(cli, "SELECT connection_id()");
          ASSERT_NO_ERROR(row_res);

          connection_id = (*row_res)[0];
        }

        SCOPED_TRACE("// capture the current com-events");
        {
          auto events_res =
              changed_event_counters(cli, "AND EVENT_NAME LIKE '%/com/%'");
          ASSERT_NO_ERROR(events_res);

          events = *events_res;
        }
      }

      SCOPED_TRACE("// wait until connection is pooled.");
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));

      if (accounts.size() > 1) {
        SCOPED_TRACE("// check that connection from the pool is sharable");
        auto account = accounts[1];

        SCOPED_TRACE("// account: " + account.username);
        MysqlClient cli;
        {
          cli.set_option(MysqlClient::GetServerPublicKey(true));
          cli.username(account.username);
          cli.password(account.password);

          auto connect_res = cli.connect(shared_router()->host(),
                                         shared_router()->port(GetParam()));
          if (!connect_res) {
            // auth may fail with DISABLED as the router has no public-key
            // cert
            GTEST_SKIP() << connect_res.error();
          }
          ASSERT_NO_ERROR(connect_res);
        }

        // should be pooled again as it is reusable.
        ASSERT_NO_ERROR(
            shared_router()->wait_for_idle_server_connections(1, 1s));

        SCOPED_TRACE("// check the previous connection was reused.");
        {
          auto row_res = query_one<1>(cli, "SELECT connection_id()");
          ASSERT_NO_ERROR(row_res);

          EXPECT_EQ(connection_id, (*row_res)[0]);
        }

        checker->apply(cli);
        checker->verifier()(cli);

        // a neutral query.
        {
          auto row_res = query_one<1>(cli, "SELECT connection_id()");
          ASSERT_NO_ERROR(row_res);
        }

        {
          auto conn_id_res = from_string(connection_id);
          EXPECT_NO_ERROR(conn_id_res);

          // connect directly to the backend as the client connection may use
          // LOCK TABLES
          auto srv_cli_res = shared_servers()[0]->admin_cli();
          ASSERT_NO_ERROR(srv_cli_res);

          auto events_res = changed_event_counters(
              *srv_cli_res, *conn_id_res, "AND EVENT_NAME LIKE '%/com/%'");
          ASSERT_NO_ERROR(events_res);
          if (can_share) {
            EXPECT_EQ(events[0].first, "statement/com/Reset Connection");

            // cli[0]
            // - connect
            // - ...
            // cli[1]
            // - change-user
            // - reset-connection
            // - select connection-id
            // - reset-connection
            // - (+ select)
            EXPECT_THAT(*events_res,
                        ElementsAre(Pair("statement/com/Change user", 1),
                                    Pair("statement/com/Reset Connection",
                                         events[0].second + 3)));
          } else {
            EXPECT_THAT(*events_res, ::testing::IsEmpty());
          }
        }
      }

      SCOPED_TRACE("// should be back in the pool");
      ASSERT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionTinyPoolOneServerTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

TEST_P(ShareConnectionSmallPoolTwoServersTest, round_robin_all_in_pool) {
  std::array<MysqlClient, 6> clis;

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  for (auto &cli : clis) {
    auto account = SharedServer::native_password_account();

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));
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
  for (auto [ndx, cli] : enumerate(clis)) {
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
    auto account = SharedServer::native_password_account();

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
      for (auto &s : shared_servers()) {
        s->flush_privileges();       // reset the auth-cache
        s->close_all_connections();  // reset the router's connection-pool
      }
    }
  }

  std::array<std::pair<std::string, std::string>, clis.size()>
      cli_connection_ids{};

  // get the connection-id and the port of the server each connection is
  // assigned to.
  for (size_t ndx{}; ndx < clis.size(); ++ndx) {
    {
      auto cmd_res =
          query_one_result(clis[ndx], "SELECT @@port, CONNECTION_ID()");
      ASSERT_NO_ERROR(cmd_res);

      auto result = std::move(*cmd_res);

      ASSERT_THAT(result, ::testing::SizeIs(1));
      ASSERT_THAT(result[0], ::testing::SizeIs(2));
      cli_connection_ids[ndx] = std::make_pair(result[0][0], result[0][1]);
    }

    if (can_share && can_fetch_password) {
      for (auto &s : shared_servers()) {
        s->close_all_connections();  // reset the router's connection-pool
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

using ShareConnectionSmallPoolTwoRoutesTest =
    ShareConnectionTestTemp<3, 2, true>;

/*
 * check if multiple routes don't harm sharing.
 *
 * pool-size: 2
 *
 * routes:
 * - [0]: s[0]
 * - [1]: s[1..2] (round-robin)
 *
 * connections:
 * 1. route[1] -> s[1] - new, to-pool
 * 2. route[1] -> s[2] - new, to-pool
 * 3. route[1] -> s[1] - from-pool, to-pool
 * 4. route[1] -> s[2] - from-pool, to-pool
 */
TEST_P(ShareConnectionSmallPoolTwoRoutesTest, round_robin_one_route) {
  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();
  const bool can_reuse = GetParam().can_reuse();
  const bool can_pool_connection_at_close =
      GetParam().can_pool_connection_at_close();

  constexpr const int max_clients = 4;
  constexpr const int max_rounds = 2;

  auto expected_idle_conns_after_select = [=](int round, size_t ndx) -> size_t {
    if (can_share && can_fetch_password) {
      // can_share and fetch-password
      if (round == 0) {
        return std::min(ndx + 1, size_t{kMaxPoolSize});
      } else {
        // in the 2nd round the pool should stay full
        return kMaxPoolSize;
      }
    } else if (can_reuse) {
      // will be reused for new connections, but not shared.
      if (round == 0) {
        return 0;
      } else {
        return ndx < kMaxPoolSize ? kMaxPoolSize - ndx - 1 : 0;
      }
    } else if (can_pool_connection_at_close) {
      if (round == 0) {
        return 0;
      } else {
        return kMaxPoolSize;
      }
    } else {
      return 0;
    }
  };

  auto expected_idle_conns_after_close = [=](int round, size_t ndx) -> size_t {
    if (can_share && can_fetch_password) {
      // sharing already placed the connections into the pool
      return kMaxPoolSize;
    } else if (can_pool_connection_at_close) {
      if (round == 0 || can_share) {
        // closing connections, adds connections to the pool.
        return std::min(ndx + 1, size_t{kMaxPoolSize});
      } else {
        return kMaxPoolSize;
      }
    } else {
      // no connection is pooled.
      return 0;
    }
  };

  for (int round{}; round < max_rounds; ++round) {
    SCOPED_TRACE("// round " + std::to_string(round));

    std::array<MysqlClient, max_clients> clis{};
    std::array<std::array<std::string, 2>, clis.size()> cli_connection_ids{};

    for (auto [ndx, cli] : enumerate(clis)) {
      const auto account = SharedServer::native_password_account();

      cli.username(account.username);
      cli.password(account.password);

      const size_t route_ndx = 1;

      ASSERT_NO_ERROR(
          cli.connect(shared_router()->host(),
                      shared_router()->port(GetParam(), route_ndx)));

      {
        auto cmd_res = query_one<2>(cli, "SELECT @@port, CONNECTION_ID()");
        ASSERT_NO_ERROR(cmd_res);

        cli_connection_ids[ndx] = *cmd_res;
      }

      SCOPED_TRACE("// check the state of the connection pool");
      {
        using clock_type = std::chrono::steady_clock;

        const auto end = clock_type::now() + 1s;
        const int expected =
            static_cast<int>(expected_idle_conns_after_select(round, ndx));

        do {
          auto idle_conns_res = shared_router()->idle_server_connections();
          ASSERT_NO_ERROR(idle_conns_res);

          auto idle_conns = *idle_conns_res;

          if (idle_conns >= expected) {
            break;
          } else if (clock_type::now() > end) {
            ASSERT_GE(idle_conns, expected);
            break;
          }

          std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
        } while (true);
      }
    }

    if (can_share && can_fetch_password) {
      EXPECT_EQ(cli_connection_ids[0], cli_connection_ids[2]);
      EXPECT_EQ(cli_connection_ids[1], cli_connection_ids[3]);

    } else {
      EXPECT_THAT(cli_connection_ids,
                  ::testing::AllOf(
                      ::testing::Contains(cli_connection_ids[0]).Times(1),
                      ::testing::Contains(cli_connection_ids[1]).Times(1),
                      ::testing::Contains(cli_connection_ids[2]).Times(1),
                      ::testing::Contains(cli_connection_ids[3]).Times(1)));
    }

    SCOPED_TRACE("// close connections in a predictable manner");
    for (auto [ndx, cli] : enumerate(clis)) {
      SCOPED_TRACE("// connection: " + std::to_string(ndx));

      cli.close();

      // wait until the closed connection enters the pool.
      {
        using clock_type = std::chrono::steady_clock;

        const auto end = clock_type::now() + 1s;
        const int expected =
            static_cast<int>(expected_idle_conns_after_close(round, ndx));

        do {
          auto idle_conns_res = shared_router()->idle_server_connections();
          ASSERT_NO_ERROR(idle_conns_res);

          auto idle_conns = *idle_conns_res;

          if (idle_conns >= expected) {
            break;
          } else if (clock_type::now() > end) {
            ASSERT_GE(idle_conns, expected);
            break;
          }

          std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
        } while (true);
      }
    }
  }
}

/*
 * check if multiple routes access the same pool.
 *
 * pool-size: 2
 *
 * routes:
 * - [0]: s[0]
 * - [1]: s[1..2] (round-robin)
 *
 * connections:
 * 1. route[1] -> s[1] - new, to-pool
 * 2. route[0] -> s[0] - new, to-pool
 * 3. route[1] -> s[2] - new, full-pool
 * 4. route[0] -> s[0] - from-pool, to-pool
 * 5. route[1] -> s[1] - from-pool, to-pool
 * 6. route[1] -> s[2] - new, full-pool
 *
 * round 2:
 * 1. route[1] -> s[1] - from-pool, to-pool
 * 2. route[0] -> s[0] - from-pool, to-pool
 * 3. route[1] -> s[2] - new, full-pool
 * 4. route[0] -> s[0] - from-pool, to-pool
 * 5. route[1] -> s[1] - from-pool, to-pool
 * 6. route[1] -> s[2] - new, full-pool
 */
TEST_P(ShareConnectionSmallPoolTwoRoutesTest, round_robin_two_routes) {
  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();
  const bool can_reuse = GetParam().can_reuse();
  const bool can_pool_connection_at_close =
      GetParam().can_pool_connection_at_close();

  constexpr const int max_clients = 6;
  constexpr const int max_rounds = 2;

  auto expected_idle_conns_after_select = [=](int round, size_t ndx) -> size_t {
    if (can_share && can_fetch_password) {
      // can_share and fetch-password
      if (round == 0) {
        return std::min(ndx + 1, size_t{kMaxPoolSize});
      } else {
        // in the 2nd round the pool should stay full
        return kMaxPoolSize;
      }
    } else if (can_reuse) {
      // will be reused for new connections, but not shared.
      if (round == 0) {
        return 0;
      } else {
        return ndx < kMaxPoolSize ? kMaxPoolSize - ndx - 1 : 0;
      }
    } else if (can_pool_connection_at_close) {
      if (round == 0) {
        return 0;
      } else {
        return kMaxPoolSize;
      }
    } else {
      return 0;
    }
  };

  auto expected_idle_conns_after_close = [=](int round, size_t ndx) -> size_t {
    if (can_share && can_fetch_password) {
      // sharing already placed the connections into the pool
      return kMaxPoolSize;
    } else if (can_pool_connection_at_close) {
      if (round == 0 || can_share) {
        // closing connections, adds connections to the pool.
        return std::min(ndx + 1, size_t{kMaxPoolSize});
      } else {
        return kMaxPoolSize;
      }
    } else {
      // no connection is pooled.
      return 0;
    }
  };

  for (int round{}; round < max_rounds; ++round) {
    SCOPED_TRACE("// round " + std::to_string(round));

    std::array<MysqlClient, max_clients> clis{};
    std::array<std::array<std::string, 2>, clis.size()> cli_connection_ids{};

    for (auto [ndx, cli] : enumerate(clis)) {
      SCOPED_TRACE("// connection: " + std::to_string(ndx));

      const auto account = SharedServer::native_password_account();

      cli.username(account.username);
      cli.password(account.password);

      const size_t route_ndx =
          (ndx == 0 || ndx == 2 || ndx == 4 || ndx == 5) ? 1 : 0;

      auto connect_res =
          cli.connect(shared_router()->host(),
                      shared_router()->port(GetParam(), route_ndx));
      ASSERT_NO_ERROR(connect_res);

      {
        auto cmd_res = query_one<2>(cli, "SELECT @@port, CONNECTION_ID()");
        ASSERT_NO_ERROR(cmd_res);

        cli_connection_ids[ndx] = *cmd_res;
      }

      SCOPED_TRACE("// check the state of the connection pool");
      {
        using clock_type = std::chrono::steady_clock;

        const auto end = clock_type::now() + 1s;
        const int expected =
            static_cast<int>(expected_idle_conns_after_select(round, ndx));

        do {
          auto idle_conns_res = shared_router()->idle_server_connections();
          ASSERT_NO_ERROR(idle_conns_res);

          auto idle_conns = *idle_conns_res;

          if (idle_conns >= expected) {
            break;
          } else if (clock_type::now() > end) {
            ASSERT_GE(idle_conns, expected);
            break;
          }

          std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
        } while (true);
      }
    }

    if (can_share && can_fetch_password) {
      EXPECT_EQ(cli_connection_ids[0], cli_connection_ids[4]);
      EXPECT_EQ(cli_connection_ids[1], cli_connection_ids[3]);

      EXPECT_THAT(cli_connection_ids,
                  ::testing::AllOf(
                      ::testing::Contains(cli_connection_ids[2]).Times(1),
                      ::testing::Contains(cli_connection_ids[5]).Times(1)));
    } else {
      EXPECT_THAT(cli_connection_ids,
                  ::testing::AllOf(
                      ::testing::Contains(cli_connection_ids[0]).Times(1),
                      ::testing::Contains(cli_connection_ids[1]).Times(1),
                      ::testing::Contains(cli_connection_ids[2]).Times(1),
                      ::testing::Contains(cli_connection_ids[3]).Times(1),
                      ::testing::Contains(cli_connection_ids[4]).Times(1),
                      ::testing::Contains(cli_connection_ids[5]).Times(1)));
    }

    SCOPED_TRACE("// close connections in a predictable manner");
    for (auto [ndx, cli] : enumerate(clis)) {
      SCOPED_TRACE("// connection: " + std::to_string(ndx));

      cli.close();

      // wait until the closed connection enters the pool.
      {
        using clock_type = std::chrono::steady_clock;

        const auto end = clock_type::now() + 1s;
        const int expected =
            static_cast<int>(expected_idle_conns_after_close(round, ndx));

        do {
          auto idle_conns_res = shared_router()->idle_server_connections();
          ASSERT_NO_ERROR(idle_conns_res);

          auto idle_conns = *idle_conns_res;

          if (idle_conns >= expected) {
            break;
          } else if (clock_type::now() > end) {
            ASSERT_GE(idle_conns, expected);
            break;
          }

          std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
        } while (true);
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionSmallPoolTwoRoutesTest,
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
 * 1. route[1] -> s[1] - new, to-pool
 * 2. route[1] -> s[2] - new, full-pool
 * 3. route[1] -> s[1] - from-pool, to-pool
 * 4. route[1] -> s[2] - new, full-pool
 */
TEST_P(ShareConnectionTinyPoolTwoRoutesTest, round_robin_one_route) {
  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();
  const bool can_reuse = GetParam().can_reuse();
  const bool can_pool_connection_at_close =
      GetParam().can_pool_connection_at_close();

  auto expected_idle_conns_after_select = [=](int round, size_t ndx) -> size_t {
    if (can_share && can_fetch_password) {
      // can_share and fetch-password
      if (round == 0) {
        return std::min(ndx + 1, size_t{kMaxPoolSize});
      } else {
        // in the 2nd round the pool should stay full
        return kMaxPoolSize;
      }
    } else if (can_reuse) {
      // will be reused for new connections, but not shared.
      if (round == 0) {
        return 0;
      } else {
        return ndx < kMaxPoolSize ? kMaxPoolSize - ndx - 1 : 0;
      }
    } else if (can_pool_connection_at_close) {
      if (round == 0) {
        return 0;
      } else {
        return kMaxPoolSize;
      }
    } else {
      return 0;
    }
  };

  auto expected_idle_conns_after_close = [=](int round, size_t ndx) -> size_t {
    if (can_share && can_fetch_password) {
      // sharing already placed the connections into the pool
      return kMaxPoolSize;
    } else if (can_pool_connection_at_close) {
      if (round == 0 || can_share) {
        // closing connections, adds connections to the pool.
        return std::min(ndx + 1, size_t{kMaxPoolSize});
      } else {
        return kMaxPoolSize;
      }
    } else {
      // no connection is pooled.
      return 0;
    }
  };

  for (size_t round{}; round < 2; ++round) {
    SCOPED_TRACE("// round " + std::to_string(round));

    std::array<MysqlClient, 4> clis{};
    std::array<std::array<std::string, 2>, clis.size()> cli_connection_ids{};

    for (auto [ndx, cli] : enumerate(clis)) {
      const auto account = SharedServer::native_password_account();

      cli.username(account.username);
      cli.password(account.password);

      const size_t route_ndx = 1;

      ASSERT_NO_ERROR(
          cli.connect(shared_router()->host(),
                      shared_router()->port(GetParam(), route_ndx)));

      {
        auto cmd_res = query_one<2>(cli, "SELECT @@port, CONNECTION_ID()");
        ASSERT_NO_ERROR(cmd_res);

        cli_connection_ids[ndx] = *cmd_res;
      }

      SCOPED_TRACE("// check the state of the connection pool");
      {
        using clock_type = std::chrono::steady_clock;

        const auto end = clock_type::now() + 1s;
        const int expected =
            static_cast<int>(expected_idle_conns_after_select(round, ndx));

        do {
          auto idle_conns_res = shared_router()->idle_server_connections();
          ASSERT_NO_ERROR(idle_conns_res);

          auto idle_conns = *idle_conns_res;

          if (idle_conns >= expected) {
            break;
          } else if (clock_type::now() > end) {
            ASSERT_GE(idle_conns, expected);
            break;
          }

          std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
        } while (true);
      }
    }

    if (can_share && can_fetch_password) {
      EXPECT_EQ(cli_connection_ids[0], cli_connection_ids[2]);

      EXPECT_THAT(cli_connection_ids,
                  ::testing::AllOf(
                      ::testing::Contains(cli_connection_ids[0]).Times(2),
                      ::testing::Contains(cli_connection_ids[1]).Times(1),
                      ::testing::Contains(cli_connection_ids[3]).Times(1)));
    } else {
      EXPECT_THAT(cli_connection_ids,
                  ::testing::AllOf(
                      ::testing::Contains(cli_connection_ids[0]).Times(1),
                      ::testing::Contains(cli_connection_ids[1]).Times(1),
                      ::testing::Contains(cli_connection_ids[2]).Times(1),
                      ::testing::Contains(cli_connection_ids[3]).Times(1)));
    }

    SCOPED_TRACE("// close connections in a predictable manner");
    for (auto [ndx, cli] : enumerate(clis)) {
      SCOPED_TRACE("// connection: " + std::to_string(ndx));

      cli.close();

      // wait until the closed connection enters the pool.
      {
        using clock_type = std::chrono::steady_clock;

        const auto end = clock_type::now() + 1s;
        const int expected =
            static_cast<int>(expected_idle_conns_after_close(round, ndx));

        do {
          auto idle_conns_res = shared_router()->idle_server_connections();
          ASSERT_NO_ERROR(idle_conns_res);

          auto idle_conns = *idle_conns_res;

          if (idle_conns >= expected) {
            break;
          } else if (clock_type::now() > end) {
            ASSERT_GE(idle_conns, expected);
            break;
          }

          std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
        } while (true);
      }
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
 * 1. route[1] -> s[1] - new, to-pool
 * 2. route[0] -> s[0] - new, full-pool
 * 3. route[1] -> s[2] - new, full-pool
 * 4. route[0] -> s[0] - new, full-pool
 * 5. route[1] -> s[1] - from-pool, to-pool
 * 6. route[1] -> s[2] - new, full-pool
 */
TEST_P(ShareConnectionTinyPoolTwoRoutesTest, round_robin_two_routes) {
  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();
  const bool can_reuse = GetParam().can_reuse();
  const bool can_pool_connection_at_close =
      GetParam().can_pool_connection_at_close();

  constexpr const int max_clients = 6;
  constexpr const int max_rounds = 2;

  auto expected_idle_conns_after_select = [=](int round, size_t ndx) -> size_t {
    if (can_share && can_fetch_password) {
      // can_share and fetch-password
      if (round == 0) {
        return std::min(ndx + 1, size_t{kMaxPoolSize});
      } else {
        // in the 2nd round the pool should stay full
        return kMaxPoolSize;
      }
    } else if (can_reuse) {
      // will be reused for new connections, but not shared.
      if (round == 0) {
        return 0;
      } else {
        return ndx < kMaxPoolSize ? kMaxPoolSize - ndx - 1 : 0;
      }
    } else if (can_pool_connection_at_close) {
      if (round == 0) {
        return 0;
      } else {
        return kMaxPoolSize;
      }
    } else {
      return 0;
    }
  };

  auto expected_idle_conns_after_close = [=](int round, size_t ndx) -> size_t {
    if (can_share && can_fetch_password) {
      // sharing already placed the connections into the pool
      return kMaxPoolSize;
    } else if (can_pool_connection_at_close) {
      if (round == 0 || can_share) {
        // closing connections, adds connections to the pool.
        return std::min(ndx + 1, size_t{kMaxPoolSize});
      } else {
        return kMaxPoolSize;
      }
    } else {
      // no connection is pooled.
      return 0;
    }
  };

  for (size_t round{}; round < max_rounds; ++round) {
    SCOPED_TRACE("// round " + std::to_string(round));

    if (round != 0 && GetParam().can_reuse()) {
      // in the 2nd round, the connections should be in the pool from the last
      // round.
      EXPECT_NO_ERROR(shared_router()->wait_for_idle_server_connections(1, 1s));
    }

    std::array<MysqlClient, max_clients> clis{};
    std::array<std::array<std::string, 2>, clis.size()> cli_connection_ids{};

    for (auto [ndx, cli] : enumerate(clis)) {
      const auto account = SharedServer::native_password_account();

      cli.username(account.username);
      cli.password(account.password);

      const size_t route_ndx =
          (ndx == 0 || ndx == 2 || ndx == 4 || ndx == 5) ? 1 : 0;

      ASSERT_NO_ERROR(
          cli.connect(shared_router()->host(),
                      shared_router()->port(GetParam(), route_ndx)));

      {
        auto cmd_res = query_one<2>(cli, "SELECT @@port, CONNECTION_ID()");
        ASSERT_NO_ERROR(cmd_res);

        cli_connection_ids[ndx] = *cmd_res;
      }

      SCOPED_TRACE("// check the state of the connection pool");
      {
        using clock_type = std::chrono::steady_clock;

        const auto end = clock_type::now() + 1s;
        const int expected =
            static_cast<int>(expected_idle_conns_after_select(round, ndx));

        do {
          auto idle_conns_res = shared_router()->idle_server_connections();
          ASSERT_NO_ERROR(idle_conns_res);

          auto idle_conns = *idle_conns_res;

          if (idle_conns >= expected) {
            break;
          } else if (clock_type::now() > end) {
            ASSERT_GE(idle_conns, expected);
            break;
          }

          std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
        } while (true);
      }
    }

    if (can_share && can_fetch_password) {
      EXPECT_EQ(cli_connection_ids[0], cli_connection_ids[4]);

      EXPECT_THAT(cli_connection_ids,
                  ::testing::AllOf(
                      ::testing::Contains(cli_connection_ids[1]).Times(1),
                      ::testing::Contains(cli_connection_ids[2]).Times(1),
                      ::testing::Contains(cli_connection_ids[3]).Times(1),
                      ::testing::Contains(cli_connection_ids[5]).Times(1)));
    } else {
      EXPECT_THAT(cli_connection_ids,
                  ::testing::AllOf(
                      ::testing::Contains(cli_connection_ids[0]).Times(1),
                      ::testing::Contains(cli_connection_ids[1]).Times(1),
                      ::testing::Contains(cli_connection_ids[2]).Times(1),
                      ::testing::Contains(cli_connection_ids[3]).Times(1),
                      ::testing::Contains(cli_connection_ids[4]).Times(1),
                      ::testing::Contains(cli_connection_ids[5]).Times(1)));
    }

    SCOPED_TRACE("// close connections in a predictable manner");
    for (auto [ndx, cli] : enumerate(clis)) {
      SCOPED_TRACE("// connection: " + std::to_string(ndx));

      cli.close();

      // wait until the closed connection enters the pool.
      {
        using clock_type = std::chrono::steady_clock;

        const auto end = clock_type::now() + 1s;
        const int expected =
            static_cast<int>(expected_idle_conns_after_close(round, ndx));

        do {
          auto idle_conns_res = shared_router()->idle_server_connections();
          ASSERT_NO_ERROR(idle_conns_res);

          auto idle_conns = *idle_conns_res;

          if (idle_conns >= expected) {
            break;
          } else if (clock_type::now() > end) {
            ASSERT_GE(idle_conns, expected);
            break;
          }

          std::this_thread::sleep_for(kIdleServerConnectionsSleepTime);
        } while (true);
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionTinyPoolTwoRoutesTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

// pool-size:   2
// servers:     4
// connections: 8
TEST_P(ShareConnectionSmallPoolFourServersTest, round_robin_all_in_pool) {
  std::array<MysqlClient, 8> clis{};

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  std::array<std::array<std::string, 2>, clis.size()> cli_connection_ids{};
  for (auto [ndx, cli] : enumerate(clis)) {
    auto account = SharedServer::native_password_account();

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto cmd_res = query_one<2>(cli, "SELECT @@port, CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    cli_connection_ids[ndx] = *cmd_res;
  }

  // round-robin: adjacent connections are on distinct backends/connections.
  for (size_t ndx{}; ndx < cli_connection_ids.size() - 1; ++ndx) {
    EXPECT_NE(cli_connection_ids[ndx][0], cli_connection_ids[ndx + 1][0]);
  }

  if (can_share && can_fetch_password) {
    EXPECT_THAT(
        cli_connection_ids,
        ::testing::AllOf(::testing::Contains(cli_connection_ids[0]).Times(2),
                         ::testing::Contains(cli_connection_ids[1]).Times(2),
                         ::testing::Contains(cli_connection_ids[2]).Times(1),
                         ::testing::Contains(cli_connection_ids[3]).Times(1),
                         // 0 = 4
                         // 1 = 5
                         ::testing::Contains(cli_connection_ids[6]).Times(1),
                         ::testing::Contains(cli_connection_ids[7]).Times(1)));
  } else {
    EXPECT_THAT(
        cli_connection_ids,
        ::testing::AllOf(::testing::Contains(cli_connection_ids[0]).Times(1),
                         ::testing::Contains(cli_connection_ids[1]).Times(1),
                         ::testing::Contains(cli_connection_ids[2]).Times(1),
                         ::testing::Contains(cli_connection_ids[3]).Times(1),
                         ::testing::Contains(cli_connection_ids[4]).Times(1),
                         ::testing::Contains(cli_connection_ids[5]).Times(1),
                         ::testing::Contains(cli_connection_ids[6]).Times(1),
                         ::testing::Contains(cli_connection_ids[7]).Times(1)));
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionSmallPoolFourServersTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

TEST_P(ShareConnectionTinyPoolTwoServersTest, round_robin_all_in_pool) {
  std::array<MysqlClient, 4> clis{};

  const bool can_fetch_password = !(GetParam().client_ssl_mode == kDisabled);
  const bool can_share = GetParam().can_share();

  std::array<std::array<std::string, 2>, clis.size()> cli_connection_ids{};

  for (auto [ndx, cli] : enumerate(clis)) {
    auto account = SharedServer::native_password_account();

    cli.username(account.username);
    cli.password(account.password);

    ASSERT_NO_ERROR(cli.connect(shared_router()->host(),
                                shared_router()->port(GetParam())));

    auto cmd_res = query_one<2>(cli, "SELECT @@port, CONNECTION_ID()");
    ASSERT_NO_ERROR(cmd_res);

    cli_connection_ids[ndx] = *cmd_res;
  }

  // round-robin: adjacent connections are on distinct backends/connections.
  for (size_t ndx{}; ndx < clis.size() - 1; ++ndx) {
    EXPECT_NE(cli_connection_ids[ndx][0], cli_connection_ids[ndx + 1][0]);
  }

  if (can_share && can_fetch_password) {
    EXPECT_THAT(
        cli_connection_ids,
        ::testing::AllOf(::testing::Contains(cli_connection_ids[0]).Times(2),
                         ::testing::Contains(cli_connection_ids[1]).Times(1),
                         ::testing::Contains(cli_connection_ids[3]).Times(1)));
  } else {
    // all IDs should be unique.
    EXPECT_THAT(
        cli_connection_ids,
        ::testing::AllOf(::testing::Contains(cli_connection_ids[0]).Times(1),
                         ::testing::Contains(cli_connection_ids[1]).Times(1),
                         ::testing::Contains(cli_connection_ids[2]).Times(1),
                         ::testing::Contains(cli_connection_ids[3]).Times(1)));
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, ShareConnectionTinyPoolTwoServersTest,
                         ::testing::ValuesIn(share_connection_params),
                         [](auto &info) {
                           return "ssl_modes_" + info.param.testname;
                         });

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
  for (auto [ndx, cli] : enumerate(clis)) {
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
  for (auto [ndx, cli] : enumerate(clis)) {
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
      ASSERT_NO_ERROR(recv_res);
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

        int expected_error_code = 2003;  // Can't connect to remove MySQL Server
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
          case cmd_byte<
              classic_protocol::message::client::StmtExecute>():          // 23
          case cmd_byte<classic_protocol::message::client::StmtReset>():  // 26
          case cmd_byte<classic_protocol::message::client::StmtFetch>():  // 28

            // unknown prepared statement handler.
            expected_error_code = 1243;
            break;
        }

        EXPECT_EQ(msg.error_code(), expected_error_code) << msg.message();
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

  for (auto [ndx, cli] : enumerate(clis)) {
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
  for (auto [ndx, cli] : enumerate(clis)) {
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

    for (auto [ndx, s] : enumerate(shared_servers())) {
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
    for (auto [ndx, s] : enumerate(shared_servers())) {
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
  for (auto [ndx, s] : enumerate(shared_servers())) {
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

    for (auto [ndx, s] : enumerate(shared_servers())) {
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
    for (auto [ndx, s] : enumerate(shared_servers())) {
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
  for (auto [ndx, s] : enumerate(shared_servers())) {
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

      for (auto [ndx, s] : enumerate(shared_servers())) {
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

      for (auto [ndx, s] : enumerate(shared_servers())) {
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

      for (auto [ndx, s] : enumerate(shared_servers())) {
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

      for (auto [ndx, s] : enumerate(shared_servers())) {
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

  for (auto [ndx, s] : enumerate(shared_servers())) {
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
