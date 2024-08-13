/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_CONFIG_GENERATOR_INCLUDED
#define ROUTER_CONFIG_GENERATOR_INCLUDED

#include <chrono>
#include <functional>
#include <iostream>  // cerr
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "auto_cleaner.h"
#include "mysql/harness/filesystem.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/keyring_info.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/sys_user_operations.h"
#include "mysqlrouter/uri.h"
#include "random_generator.h"
#include "tcp_address.h"
#include "unique_ptr.h"

namespace mysql_harness {
class Path;
}

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
      const std::string &program_name, const std::string &config_file_path,
      const std::string &state_file_path,
      const std::map<std::string, std::string> &options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const std::map<std::string, std::string> &default_paths);

  void bootstrap_directory_deployment(
      const std::string &program_name, const std::string &directory,
      const std::map<std::string, std::string> &options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const std::map<std::string, std::string> &default_paths);

  void set_keyring_info(const KeyringInfo &keyring_info) {
    keyring_info_ = keyring_info;
  }

  void set_plugin_folder(const std::string &val) { plugin_folder_ = val; }

  struct Options {
    struct Endpoint {
      int port;
      std::string socket;
      Endpoint() : port(0) {}
      Endpoint(const std::string &path) : port(0), socket(path) {}
      Endpoint(int port_) : port(port_) {}

      operator bool() const { return port > 0 || !socket.empty(); }
    };
    Options() = default;

    Endpoint rw_endpoint;
    Endpoint ro_endpoint;
    Endpoint rw_split_endpoint;
    Endpoint rw_x_endpoint;
    Endpoint ro_x_endpoint;
    bool disable_rw_split_endpoint{false};

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

    std::chrono::milliseconds ttl;
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

    // only relevant for ClusterSet
    std::string target_cluster;
    std::string target_cluster_by_name;
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
   * init() calls this to connect to metadata server; sets mysql_ (connection)
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

  struct ExistingConfigOptions {
    bool valid{false};
    uint32_t router_id{0};
    std::string username;
    uint16_t rw_x_port{0};
    uint16_t ro_x_port{0};
  };

  Options fill_options(const std::map<std::string, std::string> &user_options,
                       const std::map<std::string, std::string> &default_paths,
                       const ExistingConfigOptions &existing_config_options);

  void create_start_script(const std::string &program_name,
                           const std::string &directory,
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
      const std::string &program_name, std::ofstream &config_file,
      std::ofstream &state_file, const mysql_harness::Path &config_file_path,
      const mysql_harness::Path &state_file_path, const std::string &name,
      const std::map<std::string, std::string> &options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const std::map<std::string, std::string> &default_paths,
      bool directory_deployment, AutoCleaner &auto_clean);

  std::tuple<std::string> try_bootstrap_deployment(
      uint32_t &router_id, std::string &username, std::string &password,
      const std::string &router_name, const ClusterInfo &cluster_info,
      const std::map<std::string, std::string> &user_options,
      const std::map<std::string, std::vector<std::string>> &multivalue_options,
      const Options &options);

  void create_config(
      std::ostream &config_file, std::ostream &state_file, uint32_t router_id,
      const std::string &router_name, const std::string &system_username,
      const ClusterInfo &cluster_info, const std::string &username,
      const Options &options,
      const std::map<std::string, std::string> &default_paths,
      const std::map<std::string, std::string> &config_overwrites,
      const std::string &state_file_name, const bool full);

  void print_bootstrap_start_msg(uint32_t router_id, bool directory_deployment,
                                 const mysql_harness::Path &config_file_path);

  std::string get_bootstrap_report_text(
      const std::string &program_name, const std::string &config_file_name,
      const std::string &router_name, const std::string &metadata_cluster,
      const std::string &cluster_type_name, const std::string &hostname,
      bool is_system_deployment, const Options &options);

  void set_log_file_permissions(
      const std::map<std::string, std::string> &default_paths,
      const std::map<std::string, std::string> &user_options,
      const Options &options);

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
   * Creates Router account using CREATE USER [IF NOT EXISTS] and gives it
   * GRANTs.
   *
   * @param username Router account to be created - the username part
   * @param hostnames Router accounts to be created - the hostnames part
   * @param password Password for the account
   * @param if_not_exists if true, CREATE USER IF NOT EXISTS will be used
   *        instead of CREATE USER
   *
   * @throws std::logic_error on not connected
   *         password_too_weak on Server not liking the password
   *         account_exists if running without IF NOT EXISTS and account exists
   * already MySQLSession::Error on other (unexpected) SQL error
   */
  void create_accounts(const std::string &username,
                       const std::set<std::string> &hostnames,
                       const std::string &password, bool if_not_exists = false);

  void create_users(const std::string &username,
                    const std::set<std::string> &hostnames,
                    const std::string &password, bool if_not_exists);

  void throw_account_exists(const MySQLSession::Error &e,
                            const std::string &username);

  std::set<std::string> get_hostnames_of_created_accounts(
      const std::string &username, const std::set<std::string> &hostnames,
      bool if_not_exists);

  void give_grants_to_users(const std::string &new_accounts);

  std::string make_account_list(const std::string username,
                                const std::set<std::string> &hostnames);

  ExistingConfigOptions get_options_from_config_if_it_exists(
      const std::string &config_file_path,
      const mysqlrouter::ClusterInfo &cluster_info, bool forcing_overwrite);

  void update_router_info(uint32_t router_id, const Options &options);

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
                             const std::string &password, bool strict);

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
  std::unique_ptr<MySQLSession> mysql_;
  std::unique_ptr<ClusterMetadata> metadata_;
  int connect_timeout_;
  int read_timeout_;

  // For GR cluster Group Replication ID, for AR cluster cluster_id from the
  // metadata, for ClusterSet clusterset_id
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

  mysqlrouter::MetadataSchemaVersion schema_version_;

  std::string plugin_folder_;
};
}  // namespace mysqlrouter
#endif  // ROUTER_CONFIG_GENERATOR_INCLUDED
