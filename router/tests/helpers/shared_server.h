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

  static stdx::expected<void, MysqlError> shutdown(MysqlClient &cli);

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
    Account(std::string usr, std::string pwd, std::string with)
        : username(std::move(usr)),
          password(std::move(pwd)),
          auth_method(std::move(with)) {}

    Account(std::string usr, std::string pwd, std::string with,
            std::optional<std::string> as)
        : username(std::move(usr)),
          password(std::move(pwd)),
          auth_method(std::move(with)),
          identified_as(std::move(as)) {}

    std::string username;
    std::string password;
    std::string auth_method;

    std::optional<std::string> identified_as;
  };

  stdx::expected<MysqlClient, MysqlError> admin_cli();

  static void create_schema(MysqlClient &cli, const std::string &schema);

  static void grant_access(MysqlClient &cli, const Account &account,
                           const std::string &rights);

  static void grant_access(MysqlClient &cli, const Account &account,
                           const std::string &rights,
                           const std::string &schema);

  static void create_account(MysqlClient &cli, Account account);

  static void drop_account(MysqlClient &cli, Account account);

  static void setup_mysqld_accounts(MysqlClient &cli);

  // set the openid_connect specific configuration.
  //
  // local, not replicated.
  static stdx::expected<void, MysqlError> local_set_openid_connect_config(
      MysqlClient &cli);

  static stdx::expected<void, MysqlError> local_set_openid_connect_config(
      MysqlClient &cli, const std::string &openid_connect_config);

  // installed a plugin in the server.
  //
  // local, not replicated.
  static stdx::expected<void, MysqlError> local_install_plugin(
      MysqlClient &cli, const std::string &plugin_name) {
    return local_install_plugin(cli, plugin_name, plugin_name);
  }

  // installed a plugin in the server.
  //
  // local, not replicated.
  static stdx::expected<void, MysqlError> local_install_plugin(
      MysqlClient &cli, const std::string &plugin_name,
      const std::string &so_name);

  void flush_privileges();

  static void flush_privileges(MysqlClient &cli);

  // get all connections, but ignore internal connections and this
  // connection.
  static stdx::expected<std::vector<uint64_t>, MysqlError> user_connection_ids(
      MysqlClient &cli) {
    return user_connection_ids(cli, default_usernames());
  }

  static stdx::expected<std::vector<uint64_t>, MysqlError> user_connection_ids(
      MysqlClient &cli, const std::vector<std::string> &usernames);

  static std::vector<std::string> default_usernames() {
    return {
        admin_account().username,
        caching_sha2_empty_password_account().username,
        caching_sha2_password_account().username,
        sha256_empty_password_account().username,
        sha256_password_account().username,
        sha256_short_password_account().username,
        openid_connect_account().username,
    };
  }

  // close all connections.
  stdx::expected<void, MysqlError> close_all_connections() {
    return close_all_connections(default_usernames());
  }

  stdx::expected<void, MysqlError> close_all_connections(
      const std::vector<std::string> &usernames);

  static stdx::expected<void, MysqlError> close_all_connections(
      MysqlClient &cli) {
    return close_all_connections(cli, default_usernames());
  }

  static stdx::expected<void, MysqlError> close_all_connections(
      MysqlClient &cli, const std::vector<std::string> &usernames);

  // set some session-vars back to defaults.
  void reset_to_defaults();

  static void reset_to_defaults(MysqlClient &cli);

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

  static Account sha256_password_account() {
    constexpr const std::string_view pass("sha256passwordlongerthan20chars");

    static_assert(pass.size() > 20);

    return {"sha256_pass", std::string(pass), "sha256_password"};
  }

  static Account sha256_short_password_account() {
    constexpr const std::string_view pass("sha256password");

    static_assert(pass.size() < 20);

    return {"sha256_short", std::string(pass), "sha256_password"};
  }

  static Account sha256_empty_password_account() {
    return {"sha256_empty", "", "sha256_password"};
  }

  static Account admin_account() {
    return {"root", "", "caching_sha2_password"};
  }

  static Account openid_connect_account() {
    // - identity_provider must match the key of the
    //   'authentication_openid_connect_configuration'
    // - user must match the 'sub' of the id-token from the client.
    return {"openid_connect", "", "authentication_openid_connect", R"({
  "identity_provider": "myissuer",
  "user": "openid_user1"
})"};
  }

  static void destroy_statics();

  void has_openid_connect(bool val) { has_openid_connect_ = val; }
  bool has_openid_connect() const { return has_openid_connect_; }

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

  bool has_openid_connect_{false};
};

#endif
