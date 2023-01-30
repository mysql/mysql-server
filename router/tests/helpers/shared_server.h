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

#ifndef ROUTER_HELPER_SHARED_SERVER_H
#define ROUTER_HELPER_SHARED_SERVER_H

#include "mysql/harness/stdx/expected.h"
#include "procs.h"
#include "router/src/routing/tests/mysql_client.h"
#include "tcp_port_pool.h"

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

  ~SharedServer();

  stdx::expected<void, MysqlError> shutdown();

  [[nodiscard]] std::string mysqld_init_once_dir_name() const;

  [[nodiscard]] std::string mysqld_dir_name() const;

  integration_tests::Procs &process_manager() { return procs_; }

  // initialize the server
  //
  // initializes the server once into mysqld_init_once_dir_ and creates copies
  // from that into mysqld_dir_
  void initialize_server(const std::string &datadir);

  void prepare_datadir();

  void spawn_server_with_datadir(
      const std::string &datadir,
      const std::vector<std::string> &extra_args = {});

  void spawn_server(const std::vector<std::string> &extra_args = {});

  struct Account {
    std::string username;
    std::string password;
    std::string auth_method;
  };

  stdx::expected<MysqlClient, MysqlError> admin_cli();

  void create_schema(MysqlClient &cli, const std::string &schema);

  void grant_access(MysqlClient &cli, const Account &account,
                    const std::string &rights);

  void grant_access(MysqlClient &cli, const Account &account,
                    const std::string &rights, const std::string &schema);

  void create_account(MysqlClient &cli, Account account);

  void drop_account(MysqlClient &cli, Account account);

  void setup_mysqld_accounts();

  void flush_privileges();

  void flush_privileges(MysqlClient &cli);

  // get all connections, but ignore internal connections and this
  // connection.
  static stdx::expected<std::vector<uint64_t>, MysqlError> user_connection_ids(
      MysqlClient &cli);

  // close all connections.
  void close_all_connections();

  void close_all_connections(MysqlClient &cli);

  // set some session-vars back to defaults.
  void reset_to_defaults();

  void reset_to_defaults(MysqlClient &cli);

  [[nodiscard]] bool mysqld_failed_to_start() const {
    return mysqld_failed_to_start_;
  }

  [[nodiscard]] uint16_t server_port() const { return server_port_; }

  [[nodiscard]] uint16_t server_mysqlx_port() const {
    return server_mysqlx_port_;
  }
  [[nodiscard]] std::string server_host() const { return server_host_; }

  static Account caching_sha2_password_account() {
    constexpr const std::string_view pass("cachingpasswordlongerthan20chars");

    static_assert(pass.size() > 20);

    return {"caching_sha2", std::string(pass), "caching_sha2_password"};
  }

  static Account caching_sha2_empty_password_account() {
    return {"caching_sha2_empty", "", "caching_sha2_password"};
  }

  static Account caching_sha2_single_use_password_account() {
    return {"caching_sha2_single_use", "notusedyet", "caching_sha2_password"};
  }

  static Account native_password_account() {
    constexpr const std::string_view pass("nativepasswordlongerthan20chars");

    static_assert(pass.size() > 20);

    return {"native", std::string(pass), "mysql_native_password"};
  }

  static Account native_empty_password_account() {
    return {"native_empty", "", "mysql_native_password"};
  }

  static Account sha256_password_account() {
    constexpr const std::string_view pass("sha256passwordlongerthan20chars");

    static_assert(pass.size() > 20);

    return {"sha256_pass", std::string(pass), "sha256_password"};
  }

  static Account sha256_empty_password_account() {
    return {"sha256_empty", "", "sha256_password"};
  }

  static Account admin_account() {
    return {"root", "", "caching_sha2_password"};
  }

  static void destroy_statics();

 private:
  static TempDirectory *mysqld_init_once_dir_;
  TempDirectory mysqld_dir_{"mysqld"};

  integration_tests::Procs procs_;
  TcpPortPool &port_pool_;

  static const constexpr char server_host_[] = "127.0.0.1";
  uint16_t server_port_{port_pool_.get_next_available()};
  uint16_t server_mysqlx_port_{port_pool_.get_next_available()};

  bool mysqld_failed_to_start_{false};

  uint32_t starts_{};
};

#endif
