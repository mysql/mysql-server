/*
  Copyright (c) 2016, 2020, Oracle and/or its affiliates.

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
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "auto_cleaner.h"
#include "mysql/harness/filesystem.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/keyring_info.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/uri.h"
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
DECLARE_TEST(ConfigGeneratorTest, create_accounts_using_password_directly);
DECLARE_TEST(ConfigGeneratorTest, create_accounts_using_hashed_password);
DECLARE_TEST(ConfigGeneratorTest,
             create_accounts_using_hashed_password_if_not_exists);
DECLARE_TEST(ConfigGeneratorTest, create_accounts_using_hashed_password);
DECLARE_TEST(ConfigGeneratorTest, create_accounts_multiple_accounts);
DECLARE_TEST(ConfigGeneratorTest,
             create_accounts_multiple_accounts_if_not_exists);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___show_warnings_parser_1);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___show_warnings_parser_2);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___show_warnings_parser_3);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___show_warnings_parser_4);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___show_warnings_parser_5);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___users_exist_parser_1);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___users_exist_parser_2);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___users_exist_parser_3);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___users_exist_parser_4);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___users_exist_parser_5);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___users_exist_parser_6);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___users_exist_parser_7);
DECLARE_TEST(ConfigGeneratorTest, create_accounts___users_exist_parser_8);
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
DECLARE_TEST(ConfigGeneratorTest, get_account_host_args);
DECLARE_TEST(CreateConfigGeneratorTest, create_config_basic);
DECLARE_TEST(CreateConfigGeneratorTest, create_config_system_instance);
DECLARE_TEST(CreateConfigGeneratorTest, create_config_base_port);
DECLARE_TEST(CreateConfigGeneratorTest, create_config_skip_tcp);
DECLARE_TEST(CreateConfigGeneratorTest, create_config_use_sockets);
DECLARE_TEST(CreateConfigGeneratorTest, create_config_bind_address);
DECLARE_TEST(CreateConfigGeneratorTest, create_config_disable_rest);
#endif

namespace mysqlrouter {
class ClusterMetadata;
class MySQLSession;
class SysUserOperationsBase;
class SysUserOperations;
struct ClusterInfo;

class ConfigGenerator {
 public:
  ConfigGenerator(
      std::ostream &out_stream = std::cout, std::ostream &err_stream = std::cerr
#ifndef _WIN32
      ,
      SysUserOperationsBase *sys_user_operations = SysUserOperations::instance()
#endif
  );
  virtual ~ConfigGenerator();

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
      const std::string &config_file_path, const std::string &state_file_path,
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
    Options() {}

    Endpoint rw_endpoint;
    Endpoint ro_endpoint;
    Endpoint rw_x_endpoint;
    Endpoint ro_x_endpoint;

    std::string override_logdir;
    std::string override_logfilename;
    std::string override_rundir;
    std::string override_datadir;
    std::string socketsdir;

    std::string keyring_file_path;
    std::string keyring_master_key;
    std::string keyring_master_key_file_path;

    std::string bind_address;

    int connect_timeout;
    int read_timeout;

    mysqlrouter::SSLOptions ssl_options;

    bool use_gr_notifications;

    bool disable_rest{false};
    std::string https_port_str;

    std::string client_ssl_cert;
    std::string client_ssl_cipher;
    std::string client_ssl_curves;
    std::string client_ssl_mode;
    std::string client_ssl_key;
    std::string client_ssl_dh_params;

    std::string server_ssl_cipher;
    std::string server_ssl_curves;
    std::string server_ssl_mode;
    std::string server_ssl_ca;
    std::string server_ssl_capath;
    std::string server_ssl_crl;
    std::string server_ssl_crlpath;
    std::string server_ssl_verify;
  };

  void set_file_owner(
      const std::map<std::string, std::string> &options,
      const std::string &owner) const;  // throws std::runtime_error

 private:
  /**
   * init() calls this to read and validate several command-line options;
   * results are stored in member fields.
   *
   * @param bootstrap_options options map to process
   *
   * @throws std::runtime_error on an invalid option
   */
  void parse_bootstrap_options(
      const std::map<std::string, std::string> &bootstrap_options);

  /**
   * init() calls this to validate and extract metadata server info from server
   * URI, including user credentials.  It will also:
   * - set user name to "root" if not provided in the URI
   * - prompt for user password if not provided in the URI
   *
   * @param server_uri server URI (--bootstrap|-B argument)
   * @param bootstrap_socket bootstrap (unix) socket (--bootstrap-socket
   * argumenent)
   *
   * @returns URI with required information
   *
   * @throws std::runtime_error on an invalid data
   */
  URI parse_server_uri(const std::string &server_uri,
                       const std::string &bootstrap_socket);

  /**
   * init() calls this to connect to metadata server; sets mysql_ (conection)
   * object.
   *
   * @param u parsed server URL (--bootstrap|-B argument)
   * @param bootstrap_socket bootstrap (unix) socket (--bootstrap-socket
   * argumenent)
   * @param bootstrap_options bootstrap command-line options
   *
   * @throws std::runtime_error
   * @throws std::logic_error
   */
  void connect_to_metadata_server(
      const URI &u, const std::string &bootstrap_socket,
      const std::map<std::string, std::string> &bootstrap_options);

  /**
   * init() calls this to set GR-related member fields.
   *
   * @param u parsed server URL (--bootstrap|-B argument)
   * @param bootstrap_socket bootstrap (unix) socket (--bootstrap-socket
   * argumenent)
   *
   * @throws TODO
   */
  void init_gr_data(const URI &u, const std::string &bootstrap_socket);

  Options fill_options(const std::map<std::string, std::string> &user_options,
                       const std::map<std::string, std::string> &default_paths);

  void create_start_script(const std::string &directory,
                           bool interactive_master_key,
                           const std::map<std::string, std::string> &options);

  void create_stop_script(const std::string &directory,
                          const std::map<std::string, std::string> &options);

  // virtual so we can disable it in unit tests
  virtual void set_script_permissions(
      const std::string &script_path,
      const std::map<std::string, std::string> &options);

  // returns bootstrap report (several lines of human-readable text) if desired
  std::string bootstrap_deployment(
      std::ostream &config_file, std::ostream &state_file,
      const mysql_harness::Path &config_file_path,
      const mysql_harness::Path &state_file_path, const std::string &name,
      const std::map<std::string, std::string> &options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const std::map<std::string, std::string> &default_paths,
      bool directory_deployment, AutoCleaner &auto_clean);

  std::tuple<std::string> try_bootstrap_deployment(
      uint32_t &router_id, std::string &username, std::string &password,
      const std::string &router_name, const std::string &cluster_id,
      const std::map<std::string, std::string> &user_options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const Options &options);

  void create_config(std::ostream &config_file, std::ostream &state_file,
                     uint32_t router_id, const std::string &router_name,
                     const std::string &system_username,
                     const ClusterInfo &cluster_info,
                     const std::string &username, const Options &options,
                     const std::map<std::string, std::string> &default_paths,
                     const std::string &state_file_name = "");

  void print_bootstrap_start_msg(uint32_t router_id, bool directory_deployment,
                                 const mysql_harness::Path &config_file_path);

  std::string get_bootstrap_report_text(const std::string &config_file_name,
                                        const std::string &router_name,
                                        const std::string &metadata_cluster,
                                        const std::string &cluster_type_name,
                                        const std::string &hostname,
                                        bool is_system_deployment,
                                        const Options &options);

  void set_log_file_permissions(
      const std::map<std::string, std::string> &default_paths,
      const std::map<std::string, std::string> &user_options,
      const Options &options);

  static std::string gen_metadata_cache_routing_section(
      bool is_classic, bool is_writable, const Options::Endpoint endpoint,
      const Options &options, const std::string &metadata_key,
      const std::string &metadata_replicaset,
      const std::string &fast_router_key);

  /** @brief Deletes Router accounts just created
   *
   * This method runs as a cleanup after something goes wrong.  Its purpose is
   * to undo CREATE USER [IF NOT EXISTS] for accounts that got created during
   * bootstrap.  Note that it will drop only those accounts which did not exist
   * prior to bootstrap (it may be a subset of account names passed to
   * CREATE USER [IF NOT EXISTS]).  If it is not able to determine what this
   * (sub)set is, it will not drop anything - instead it will advise user on
   * how to clean those up manually.
   */
  void undo_create_user_for_new_accounts() noexcept;

  /** @brief Finds all hostnames given on command-line
   *
   * MySQL accounts are of form `<username>@<hostname>`.  This function returns
   * all `<hostname>` parts that were provided via --account-host switches
   *
   * @param multivalue_options key/list-of-values map of bootstrap config;
   *        carries --account-host inside
   */
  static std::set<std::string> get_account_host_args(
      const std::map<std::string, std::vector<std::string>>
          &multivalue_options) noexcept;

  /** @brief Creates Router accounts
   *
   * Creates Router accounts for all hostnames (ie. `someuser@host1`,
   * `someuser@host2`, `someuser@%`, etc).  It will create such accounts for
   * all hosts that appear in hostnames_cmdline, but not in hostnames_db.
   *
   * @note This is the higher-level method, which drives calls to lower-level
   *       methods like create_account_with_compliant_password() and
   *       create_accounts().
   *
   * @param user_options key/value map of bootstrap config options
   * @param hostnames hostnames provided as --account-host arguments
   * @param username Router account to be created (without the hostname part)
   * @param password_change_ok password is allowed to be changed if needed
   * @param password Router account password, will be overwritten if empty
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
      const std::set<std::string> &hostnames, const std::string &username,
      const std::string &password, bool password_change_ok);

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
   *       lower-level create_accounts() method.
   *
   * @param user_options key/value map of bootstrap config options
   * @param username Router account to be created - the username part
   * @param hostnames Router accounts to be created - the hostname part
   * @param password Password for the account
   * @param password_change_ok password is allowed to be changed if needed
   * @param if_not_exists if true, CREATE USER IF NOT EXISTS will be used
   *        instead of CREATE USER
   *
   * @returns password
   *
   * @throws std::logic_error on not connected
   *         std::runtime_error on bad password
   *         MySQLSession::Error on other (unexpected) SQL error
   */
  std::string create_accounts_with_compliant_password(
      const std::map<std::string, std::string> &user_options,
      const std::string &username, const std::set<std::string> &hostnames,
      const std::string &password, bool password_change_ok, bool if_not_exists);

  /** @brief Creates Router account (low-level function)
   *
   * Creates Router accout using CREATE USER [IF NOT EXISTS] ang give it GRANTs.
   *
   * @param username Router account to be created - the username part
   * @param hostnames Router accounts to be created - the hostnames part
   * @param password Password for the account
   * @param hash_password CREATE USER method:
   *   true: password should be hashed, CREATE USER using mysql_native_password
   *   false: password should remain plaintext, CREATE USER without
   *          mysql_native_password
   * @param if_not_exists if true, CREATE USER IF NOT EXISTS will be used
   *        instead of CREATE USER
   *
   * @throws std::logic_error on not connected
   *         password_too_weak on Server not liking the password
   *         plugin_not_loaded on Server not supporting mysql_native_password
   *         account_exists if running without IF NOT EXISTS and account exists
   * already MySQLSession::Error on other (unexpected) SQL error
   */
  void create_accounts(const std::string &username,
                       const std::set<std::string> &hostnames,
                       const std::string &password, bool hash_password = false,
                       bool if_not_exists = false);

  void create_users(const std::string &username,
                    const std::set<std::string> &hostnames,
                    const std::string &password, bool hash_password,
                    bool if_not_exists);

  void throw_account_exists(const MySQLSession::Error &e,
                            const std::string &username);

  std::set<std::string> get_hostnames_of_created_accounts(
      const std::string &username, const std::set<std::string> &hostnames,
      bool if_not_exists);

  void give_grants_to_users(const std::string &new_accounts);

  std::string make_account_list(const std::string username,
                                const std::set<std::string> &hostnames);

  std::pair<uint32_t, std::string>
  get_router_id_and_username_from_config_if_it_exists(
      const std::string &config_file_path, const std::string &cluster_name,
      bool forcing_overwrite);

  void update_router_info(uint32_t router_id, const Options &options);

  static std::string endpoint_option(const Options &options,
                                     const Options::Endpoint &ep);

  bool backup_config_file_if_different(
      const mysql_harness::Path &config_path, const std::string &new_file_path,
      const std::map<std::string, std::string> &options,
      AutoCleaner *auto_cleaner = nullptr);

  void set_keyring_info_real_paths(std::map<std::string, std::string> &options,
                                   const mysql_harness::Path &path);

  void store_credentials_in_keyring(
      AutoCleaner &auto_clean,
      const std::map<std::string, std::string> &user_options,
      uint32_t router_id, const std::string &username,
      const std::string &password, Options &options);

  std::string fetch_password_from_keyring(const std::string &username,
                                          uint32_t router_id);

  void init_keyring_and_master_key(
      AutoCleaner &auto_clean,
      const std::map<std::string, std::string> &user_options,
      uint32_t router_id);

  void init_keyring_file(uint32_t router_id, bool create_if_needed = true);

  static void set_ssl_options(
      MySQLSession *sess, const std::map<std::string, std::string> &options);

  void ensure_router_id_is_ours(uint32_t &router_id,
                                const std::string &hostname_override);

  uint32_t register_router(const std::string &router_name,
                           const std::string &hostname_override, bool force);

  void verify_router_account(const std::string &username,
                             const std::string &password,
                             const std::string &primary_cluster_name,
                             bool strict);

  /**
   * @brief Create Router configuration that allows to enable the REST services.
   *
   * Create configuration for the following plugins: http_server,
   * http_auth_realm, rest_router, rest_api, http_auth_backend, rest_routing,
   * rest_metadata_cache.
   *
   * @param[in] options Bootstrap config options.
   * @param[in] default_paths Map of predefined default paths.
   *
   * @return Router configuration that enables the REST services.
   */
  std::string generate_config_for_rest(
      const Options &options,
      const std::map<std::string, std::string> &default_paths) const;

  /**
   * @brief Prepare X.509 certificates for the Router.
   *
   * If user provides Router certificate and key files they are used in the
   * first place so no action is taken in this method. If there are no existing
   * certificate files then CA certificate and key along with Router certificate
   * and key will be created.
   *
   * @param[in] user_options Key/value map of bootstrap config options.
   * @param[in] default_paths Map of predefined default paths.
   * @param[in,out] auto_cleaner Automatic file cleanup object that guarantees
   * file cleanup if bootstrap fails at any point.
   *
   * @throws std::runtime_error Data directory contains some certificate files
   * but Router certificate and/or key is missing.
   */
  void prepare_ssl_certificate_files(
      const std::map<std::string, std::string> &user_options,
      const std::map<std::string, std::string> &default_paths,
      AutoCleaner *auto_cleaner) const;

  /**
   * @brief Check if datadir directory contains only files that are allowed
   * before the bootstrap.
   *
   * @param[in] dir Data directory representation.
   *
   * @retval false - datadir contains files that are not allowed before the
   * bootstrap.
   * @retval true - datadir does not contain files that are not allowed before
   * the bootstrap.
   */
  bool datadir_contains_allowed_files(
      const mysql_harness::Directory &dir) const;

 private:
  mysql_harness::UniquePtr<MySQLSession> mysql_;
  std::unique_ptr<ClusterMetadata> metadata_;
  int connect_timeout_;
  int read_timeout_;

  // For GR cluster Group Replication ID, for AR cluster cluster_id from the
  // metadata
  std::string cluster_specific_id_;
  std::string cluster_initial_hostname_;
  unsigned int cluster_initial_port_;
  std::string cluster_initial_username_;
  std::string cluster_initial_password_;
  std::string cluster_initial_socket_;

  KeyringInfo keyring_info_;
  bool keyring_initialized_ = false;

  std::ostream &out_stream_;
  std::ostream &err_stream_;

  struct UndoCreateAccountList {
    enum {
      kNotSet = 1,  // =1 is not a requirement, just defensive programming
      kAllAccounts,
      kNewAccounts
    } type = kNotSet;
    std::string accounts;
  } undo_create_account_list_;

  const struct TLS_filenames {
    std::string ca_key{"ca-key.pem"};
    std::string ca_cert{"ca.pem"};
    std::string router_key{"router-key.pem"};
    std::string router_cert{"router-cert.pem"};
  } tls_filenames_;

#ifndef _WIN32
  SysUserOperationsBase *sys_user_operations_;
#endif

#ifdef FRIEND_TEST
  FRIEND_TEST(::ConfigGeneratorTest, fetch_bootstrap_servers_one);
  FRIEND_TEST(::ConfigGeneratorTest, fetch_bootstrap_servers_three);
  FRIEND_TEST(::ConfigGeneratorTest,
              fetch_bootstrap_servers_multiple_replicasets);
  FRIEND_TEST(::ConfigGeneratorTest, fetch_bootstrap_servers_invalid);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts_using_password_directly);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts_using_hashed_password);
  FRIEND_TEST(::ConfigGeneratorTest,
              create_accounts_using_hashed_password_if_not_exists);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts_multiple_accounts);
  FRIEND_TEST(::ConfigGeneratorTest,
              create_accounts_multiple_accounts_if_not_exists);

  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___show_warnings_parser_1);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___show_warnings_parser_2);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___show_warnings_parser_3);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___show_warnings_parser_4);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___show_warnings_parser_5);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___users_exist_parser_1);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___users_exist_parser_2);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___users_exist_parser_3);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___users_exist_parser_4);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___users_exist_parser_5);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___users_exist_parser_6);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___users_exist_parser_7);
  FRIEND_TEST(::ConfigGeneratorTest, create_accounts___users_exist_parser_8);
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
  FRIEND_TEST(::ConfigGeneratorTest, get_account_host_args);

  FRIEND_TEST(::CreateConfigGeneratorTest, create_config_basic);
  FRIEND_TEST(::CreateConfigGeneratorTest, create_config_system_instance);
  FRIEND_TEST(::CreateConfigGeneratorTest, create_config_base_port);
  FRIEND_TEST(::CreateConfigGeneratorTest, create_config_skip_tcp);
  FRIEND_TEST(::CreateConfigGeneratorTest, create_config_use_sockets);
  FRIEND_TEST(::CreateConfigGeneratorTest, create_config_bind_address);
  FRIEND_TEST(::CreateConfigGeneratorTest, create_config_disable_rest);
#endif
};
}  // namespace mysqlrouter
#endif  // ROUTER_CONFIG_GENERATOR_INCLUDED
