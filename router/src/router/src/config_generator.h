/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTER_CONFIG_GENERATOR_INCLUDED
#define ROUTER_CONFIG_GENERATOR_INCLUDED

#include <functional>
#include <map>
#include <ostream>
#include <string>
#include <vector>
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/keyring_info.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils.h"
#include "random_generator.h"
#include "tcp_address.h"
#include "unique_ptr.h"

namespace mysql_harness {
class Path;
}

// GCC 4.8.4 requires all classes to be forward-declared before used with
// "friend class <friendee>", if they're in a different namespace than the
// friender
#ifdef FRIEND_TEST
#include "mysqlrouter/utils.h"  // DECLARE_TEST
DECLARE_TEST(ConfigGeneratorTest, fetch_bootstrap_servers_one);
DECLARE_TEST(ConfigGeneratorTest, fetch_bootstrap_servers_three);
DECLARE_TEST(ConfigGeneratorTest, fetch_bootstrap_servers_multiple_replicasets);
DECLARE_TEST(ConfigGeneratorTest, fetch_bootstrap_servers_invalid);
DECLARE_TEST(ConfigGeneratorTest, create_config_single_master);
DECLARE_TEST(ConfigGeneratorTest, create_config_multi_master);
DECLARE_TEST(ConfigGeneratorTest, delete_account_for_all_hosts);
DECLARE_TEST(ConfigGeneratorTest, create_acount);
DECLARE_TEST(ConfigGeneratorTest, create_router_accounts);
DECLARE_TEST(ConfigGeneratorTest, fill_options);
DECLARE_TEST(ConfigGeneratorTest, bootstrap_invalid_name);
DECLARE_TEST(ConfigGeneratorTest, ssl_stage1_cmdline_arg_parse);
DECLARE_TEST(ConfigGeneratorTest, ssl_stage2_bootstrap_connection);
DECLARE_TEST(ConfigGeneratorTest, ssl_stage3_create_config);
DECLARE_TEST(ConfigGeneratorTest, empty_config_file);
DECLARE_TEST(ConfigGeneratorTest, warn_on_no_ssl);
DECLARE_TEST(ConfigGeneratorTest, set_file_owner_no_user);
DECLARE_TEST(ConfigGeneratorTest, set_file_owner_user_empty);
DECLARE_TEST(ConfigGeneratorTest, start_sh);
DECLARE_TEST(ConfigGeneratorTest, stop_sh);
DECLARE_TEST(ConfigGeneratorTest, register_router_error_message);
DECLARE_TEST(ConfigGeneratorTest, ensure_router_id_is_ours_error_message);
#endif

class AutoCleaner;

namespace mysqlrouter {
class MySQLInnoDBClusterMetadata;
class MySQLSession;
class SysUserOperationsBase;
class SysUserOperations;

class ConfigGenerator {
 public:
  ConfigGenerator(
#ifndef _WIN32
      SysUserOperationsBase *sys_user_operations = SysUserOperations::instance()
#endif
  );
  virtual ~ConfigGenerator() = default;

  /** @brief first part of the bootstrap process
   *
   * This function does a lot of initialisation before bootstrap starts making
   * changes.
   *
   * @param server_url server to bootstrap from
   * @param bootstrap_options bootstrap options
   *
   * @throws std::runtime_error
   */
  void init(const std::string &server_url,
            const std::map<std::string, std::string> &bootstrap_options);

  /** @brief logs warning and returns false if SSL mode is set to PREFERRED and
   *         SSL is not being used, true otherwise
   *
   * @param options map of commandline options
   *
   * @returns false if SSL mode is set to PREFERRED and SSL is not being used,
   *          true otherwise
   *
   * @throws std::runtime_error
   */
  bool warn_on_no_ssl(const std::map<std::string, std::string> &options);

  void bootstrap_system_deployment(
      const std::string &config_file_path,
      const std::map<std::string, std::string> &options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const std::map<std::string, std::string> &default_paths);

  void bootstrap_directory_deployment(
      const std::string &directory,
      const std::map<std::string, std::string> &options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const std::map<std::string, std::string> &default_paths);

  void set_keyring_info(const KeyringInfo &keyring_info) {
    keyring_info_ = keyring_info;
  }

  struct Options {
    struct Endpoint {
      int port;
      std::string socket;
      Endpoint() : port(0) {}
      Endpoint(const std::string &path) : port(0), socket(path) {}
      Endpoint(int port_) : port(port_) {}

      operator bool() const { return port > 0 || !socket.empty(); }
    };
    Options() : multi_master(false) {}

    Endpoint rw_endpoint;
    Endpoint ro_endpoint;
    Endpoint rw_x_endpoint;
    Endpoint ro_x_endpoint;

    std::string override_logdir;
    std::string override_rundir;
    std::string override_datadir;
    std::string socketsdir;

    std::string keyring_file_path;
    std::string keyring_master_key;
    std::string keyring_master_key_file_path;

    bool multi_master;
    std::string bind_address;

    int connect_timeout;
    int read_timeout;

    mysqlrouter::SSLOptions ssl_options;
  };

  void set_file_owner(const std::map<std::string, std::string> &options,
                      const std::string &owner);  // throws std::runtime_error

 private:
  friend class MySQLInnoDBClusterMetadata;

  Options fill_options(bool multi_master,
                       const std::map<std::string, std::string> &user_options);

  void create_start_script(const std::string &directory,
                           bool interactive_master_key,
                           const std::map<std::string, std::string> &options);

  void create_stop_script(const std::string &directory,
                          const std::map<std::string, std::string> &options);

  // virtual so we can disable it in unit tests
  virtual void set_script_permissions(
      const std::string &script_path,
      const std::map<std::string, std::string> &options);

  void bootstrap_deployment(
      std::ostream &config_file, const mysql_harness::Path &config_file_path,
      const std::string &name,
      const std::map<std::string, std::string> &options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const std::map<std::string, std::string> &default_paths,
      bool directory_deployment, AutoCleaner &auto_clean);

  std::tuple<std::string> try_bootstrap_deployment(
      uint32_t &router_id, std::string &username,
      const std::string &router_name,
      mysql_harness::RandomGeneratorInterface &rg,
      const std::map<std::string, std::string> &user_options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const std::string &rw_endpoint, const std::string &ro_endpoint,
      const std::string &rw_x_endpoint, const std::string &ro_x_endpoint);

  void init_keyring_file(uint32_t router_id);

  void fetch_bootstrap_servers(std::string &bootstrap_servers,
                               std::string &metadata_cluster,
                               std::string &metadata_replicaset,
                               bool &multi_master);

  void create_config(std::ostream &config_file, uint32_t router_id,
                     const std::string &router_name,
                     const std::string &system_username,
                     const std::string &bootstrap_server_addresses,
                     const std::string &metadata_cluster,
                     const std::string &metadata_replicaset,
                     const std::string &username, const Options &options,
                     bool print_configs = false);

  /** @brief Deletes (old) Router accounts
   *
   * Deletes all accounts (for all hosts) for a particular username (ie. for
   * user "someuser" it will delete `someuser@host1`, `someuser@host2`,
   * `someuser@%`, etc)
   *
   * @param username Router account to be deleted (without the hostname part)
   *
   * @throws std::logic_error on not connected
   *         MySQLSession::Error on SQL error
   */
  void delete_account_for_all_hosts(const std::string &username);

  /** @brief Creates Router accounts
   *
   * Creates Router account for all needed hostnames (ie. `someuser@host1`,
   * `someuser@host2`, `someuser@%`, etc).
   *
   * @note This is the higher-level method, which drives calls to lower-level
   *       methods like create_account_with_compliant_password() and
   *       create_account().
   *
   * @param user_options key/value map of bootstrap config options
   * @param multivalue_options key/list-of-values map of bootstrap config
   * options, including list of hostnames
   * @param username Router account to be created (without the hostname part)
   *
   * @returns auto-generated password
   *
   * @throws std::logic_error on not connected
   *         std::runtime_error on bad password or Server's password policy
   *                               changing during bootstrap
   *         MySQLSession::Error on other (unexpected) SQL error
   */
  std::string create_router_accounts(
      const std::map<std::string, std::string> &user_options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const std::string &username);

  /** @brief Creates Router account with compliant password
   *
   * Creates Router account with a (self-generated) password that will pass
   * Server's password policy. It first tries creating a hashed password using
   * mysql_native_password plugin. If that fails, it falls back to using
   * plaintext password, which the Server may reject for not being strong
   * enough. If that's the case, it will generate another password and try again
   * 2 more times (for a total of 3 password-generation attempts), after which
   * it will give up.
   *
   * @note This is a higher-level method, with smart logic that drives calls to
   *       lower-level create_account() method.
   *
   * @param user_options key/value map of bootstrap config options
   * @param username Router account to be created - the username part
   * @param hostname Router account to be created - the hostname part
   *
   * @returns std::pair, where:
   *   - std::string contains the auto-generated password
   *   - bool states if account was created with hashed password
   *     (with mysql_native_password)
   *
   * @throws std::logic_error on not connected
   *         std::runtime_error on bad password
   *         MySQLSession::Error on other (unexpected) SQL error
   */
  std::pair<std::string, bool> create_account_with_compliant_password(
      const std::map<std::string, std::string> &user_options,
      const std::string &username, const std::string &hostname);

  /** @brief Creates Router account (low-level function)
   *
   * Creates Router accout using CREATE USER ang give it GRANTs.
   *
   * @param username Router account to be created - the username part
   * @param hostname Router account to be created - the hostname part
   * @param password Password for the account
   * @param hash_password CREATE USER method:
   *   true: password should be hashed, CREATE USER using mysql_native_password
   *   false: password should remain plaintext, CREATE USER without
   * mysql_native_password
   *
   * @throws std::logic_error on not connected
   *         password_too_weak on Server not liking the password
   *         plugin_not_loaded on Server not supporting mysql_native_password
   *         MySQLSession::Error on other (unexpected) SQL error
   */
  void create_account(const std::string &username, const std::string &hostname,
                      const std::string &password, bool hash_password = false);

  std::pair<uint32_t, std::string> get_router_id_and_name_from_config(
      const std::string &config_file_path, const std::string &cluster_name,
      bool forcing_overwrite);

  void update_router_info(uint32_t router_id, const Options &options);

  std::string endpoint_option(const Options &options,
                              const Options::Endpoint &ep);

  bool backup_config_file_if_different(
      const mysql_harness::Path &config_path, const std::string &new_file_path,
      const std::map<std::string, std::string> &options,
      AutoCleaner *auto_cleaner = nullptr);

  void set_keyring_info_real_paths(std::map<std::string, std::string> &options,
                                   const mysql_harness::Path &path);

  void init_keyring_and_master_key(
      AutoCleaner &auto_clean,
      const std::map<std::string, std::string> &user_options,
      uint32_t router_id);

  static void set_ssl_options(
      MySQLSession *sess, const std::map<std::string, std::string> &options);

  void ensure_router_id_is_ours(uint32_t &router_id, std::string &username,
                                const std::string &hostname_override,
                                MySQLInnoDBClusterMetadata &metadata);

  void register_router_and_set_username(
      uint32_t &router_id, const std::string &router_name,
      std::string &username, const std::string &hostname_override, bool force,
      MySQLInnoDBClusterMetadata &metadata,
      mysql_harness::RandomGeneratorInterface &rg);

 private:
  mysql_harness::UniquePtr<MySQLSession> mysql_;
  int connect_timeout_;
  int read_timeout_;

  std::string gr_initial_hostname_;
  unsigned int gr_initial_port_;
  std::string gr_initial_username_;
  std::string gr_initial_password_;
  std::string gr_initial_socket_;

  KeyringInfo keyring_info_;

#ifndef _WIN32
  SysUserOperationsBase *sys_user_operations_;
#endif

#ifdef FRIEND_TEST
  FRIEND_TEST(::ConfigGeneratorTest, fetch_bootstrap_servers_one);
  FRIEND_TEST(::ConfigGeneratorTest, fetch_bootstrap_servers_three);
  FRIEND_TEST(::ConfigGeneratorTest,
              fetch_bootstrap_servers_multiple_replicasets);
  FRIEND_TEST(::ConfigGeneratorTest, fetch_bootstrap_servers_invalid);
  FRIEND_TEST(::ConfigGeneratorTest, create_config_single_master);
  FRIEND_TEST(::ConfigGeneratorTest, create_config_multi_master);
  FRIEND_TEST(::ConfigGeneratorTest, delete_account_for_all_hosts);
  FRIEND_TEST(::ConfigGeneratorTest, create_acount);
  FRIEND_TEST(::ConfigGeneratorTest, create_router_accounts);
  FRIEND_TEST(::ConfigGeneratorTest, fill_options);
  FRIEND_TEST(::ConfigGeneratorTest, bootstrap_invalid_name);
  FRIEND_TEST(::ConfigGeneratorTest, ssl_stage1_cmdline_arg_parse);
  FRIEND_TEST(::ConfigGeneratorTest, ssl_stage2_bootstrap_connection);
  FRIEND_TEST(::ConfigGeneratorTest, ssl_stage3_create_config);
  FRIEND_TEST(::ConfigGeneratorTest, empty_config_file);
  FRIEND_TEST(::ConfigGeneratorTest, warn_on_no_ssl);
  FRIEND_TEST(::ConfigGeneratorTest, set_file_owner_no_user);
  FRIEND_TEST(::ConfigGeneratorTest, set_file_owner_user_empty);
  FRIEND_TEST(::ConfigGeneratorTest, start_sh);
  FRIEND_TEST(::ConfigGeneratorTest, stop_sh);
  FRIEND_TEST(::ConfigGeneratorTest, register_router_error_message);
  FRIEND_TEST(::ConfigGeneratorTest, ensure_router_id_is_ours_error_message);
#endif
};
}  // namespace mysqlrouter
#endif  // ROUTER_CONFIG_GENERATOR_INCLUDED
