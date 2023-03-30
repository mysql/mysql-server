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

#include "shared_server.h"

#include <gtest/gtest.h>

#include "mysql/harness/stdx/expected_ostream.h"
#include "mysqlrouter/utils.h"  // copy_file
#include "stdx_expected_no_error.h"

using namespace std::chrono_literals;

static std::ostream &operator<<(std::ostream &os, MysqlError e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
}

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

SharedServer::~SharedServer() {
  // shutdown via API to get a clean exit-code on windows.
  shutdown();
  process_manager().wait_for_exit();
}

stdx::expected<void, MysqlError> SharedServer::shutdown() {
  auto cli_res = admin_cli();
  if (!cli_res) return stdx::make_unexpected(cli_res.error());

  auto shutdown_res = cli_res->shutdown();
  if (!shutdown_res) return stdx::make_unexpected(shutdown_res.error());

  return {};
}

[[nodiscard]] std::string SharedServer::mysqld_init_once_dir_name() const {
  return mysqld_init_once_dir_->name();
}

[[nodiscard]] std::string SharedServer::mysqld_dir_name() const {
  return mysqld_dir_.name();
}

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
void SharedServer::initialize_server(const std::string &datadir) {
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
                  mysql_harness::Path::directory_separator + "mysqld-init.err",
          });
  proc.set_logging_path(datadir, "mysqld-init.err");
  try {
    proc.wait_for_exit(90s);  // throws when it times out.
  } catch (const std::exception &e) {
    process_manager().dump_logs();

    FAIL() << e.what();
  }

  if (proc.exit_code() != 0) {
    mysqld_failed_to_start_ = true;

    process_manager().dump_logs();
  }
}

void SharedServer::prepare_datadir() {
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

void SharedServer::spawn_server_with_datadir(
    const std::string &datadir, const std::vector<std::string> &extra_args) {
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

  if (mysqld_failed_to_start_) process_manager().dump_logs();

  ++starts_;
}

void SharedServer::spawn_server(const std::vector<std::string> &extra_args) {
  spawn_server_with_datadir(mysqld_dir_name(), extra_args);
}

struct Account {
  std::string username;
  std::string password;
  std::string auth_method;
};

stdx::expected<MysqlClient, MysqlError> SharedServer::admin_cli() {
  MysqlClient cli;

  auto account = admin_account();

  cli.username(account.username);
  cli.password(account.password);

  auto connect_res = cli.connect(server_host(), server_port());
  if (!connect_res) return connect_res.get_unexpected();

  return cli;
}

void SharedServer::create_schema(MysqlClient &cli, const std::string &schema) {
  std::ostringstream oss;
  oss << "CREATE SCHEMA " << std::quoted(schema, '`');

  auto q = oss.str();

  SCOPED_TRACE("// " + q);
  ASSERT_NO_ERROR(cli.query(q)) << q;
}

void SharedServer::grant_access(MysqlClient &cli, const Account &account,
                                const std::string &rights) {
  std::ostringstream oss;
  oss << "GRANT " << rights << " ON *.* TO "
      << std::quoted(account.username, '`');

  auto q = oss.str();

  SCOPED_TRACE("// " + q);
  ASSERT_NO_ERROR(cli.query(q)) << q;
}

void SharedServer::grant_access(MysqlClient &cli, const Account &account,
                                const std::string &rights,
                                const std::string &schema) {
  std::ostringstream oss;
  oss << "GRANT " << rights << "  ON " << std::quoted(schema, '`') << ".* TO "
      << std::quoted(account.username, '`');

  auto q = oss.str();

  SCOPED_TRACE("// " + q);
  ASSERT_NO_ERROR(cli.query(q)) << q;
}

void SharedServer::create_account(MysqlClient &cli, Account account) {
  const std::string q = "CREATE USER " + account.username + " " +         //
                        "IDENTIFIED WITH " + account.auth_method + " " +  //
                        "BY '" + account.password + "'";

  SCOPED_TRACE("// " + q);
  ASSERT_NO_ERROR(cli.query(q)) << q;
}

void SharedServer::drop_account(MysqlClient &cli, Account account) {
  const std::string q = "DROP USER " + account.username;

  SCOPED_TRACE("// " + q);
  ASSERT_NO_ERROR(cli.query(q)) << q;
}

void SharedServer::setup_mysqld_accounts() {
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

void SharedServer::flush_privileges() {
  SCOPED_TRACE("// flushing privileges");
  auto cli_res = admin_cli();
  ASSERT_NO_ERROR(cli_res);

  flush_privileges(*cli_res);
}

void SharedServer::flush_privileges(MysqlClient &cli) {
  ASSERT_NO_ERROR(cli.query("FLUSH PRIVILEGES"));
}

// get all connections, but ignore internal connections and this
// connection.
stdx::expected<std::vector<uint64_t>, MysqlError>
SharedServer::user_connection_ids(MysqlClient &cli) {
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
void SharedServer::close_all_connections() {
  SCOPED_TRACE("// closing all connections at the server.");

  auto cli_res = admin_cli();
  ASSERT_NO_ERROR(cli_res);

  close_all_connections(*cli_res);
}

void SharedServer::close_all_connections(MysqlClient &cli) {
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
void SharedServer::reset_to_defaults() {
  auto cli_res = admin_cli();
  ASSERT_NO_ERROR(cli_res);

  reset_to_defaults(*cli_res);
}

// set global settings to default values.
void SharedServer::reset_to_defaults(MysqlClient &cli) {
  ASSERT_NO_ERROR(cli.query("SET GLOBAL max_connections = DEFAULT"));
}

void SharedServer::destroy_statics() {
  if (mysqld_init_once_dir_) {
    delete mysqld_init_once_dir_;
    mysqld_init_once_dir_ = nullptr;
  }
}

TempDirectory *SharedServer::mysqld_init_once_dir_ = nullptr;
