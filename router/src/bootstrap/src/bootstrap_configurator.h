/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_CONFIGURATOR_H_
#define ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_CONFIGURATOR_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "bootstrap_credentials.h"
#include "keyring_handler.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/vt100.h"
#include "mysqlrouter/uri.h"
#include "router/src/router/src/config_generator.h"
#include "router/src/router/src/router_conf.h"

using String = std::string;
using Strings = std::vector<std::string>;
using UniqueStrings = std::set<std::string>;

class BootstrapConfigurator {
 public:
  BootstrapConfigurator(std::ostream &out_stream, std::ostream &err_stream);

  void init(int argc, char **argv);
  void run();

  static void init_main_logger(mysql_harness::LoaderConfig &config,
                               bool raw_mode = false);

 private:
  class MySQLRouterAndMrsConf : public MySQLRouterConf {
   public:
    MySQLRouterAndMrsConf(bool &is_legacy, KeyringInfo &keyring_info,
                          std::ostream &out_stream, std::ostream &err_stream,
                          std::vector<std::string> &configs_)
        : MySQLRouterConf(keyring_info, out_stream, err_stream),
          is_legacy_{is_legacy},
          config_files_{configs_} {}

    void prepare_command_options(
        CmdArgHandler &arg_handler,
        const std::string &bootstrap_uri = "") noexcept override {
      MySQLRouterConf::prepare_command_options(arg_handler, bootstrap_uri);
      using OptionNames = CmdOption::OptionNames;

#ifndef _WIN32
      arg_handler.add_option(
          OptionNames({"-u", "--user"}),
          "Run the mysqlrouter as the user having the name user_name.",
          CmdOptionValueReq::required, "username",
          [this](const std::string &username) {
            this->bootstrap_options_["_username"] = username;
          },
          [this](const std::string &) {
            if (this->bootstrap_uri_.empty()) {
              this->bootstrap_options_["user_cmd_line"] =
                  this->bootstrap_options_["_username"];
            } else {
              check_user(this->bootstrap_options_["_username"], true,
                         mysqlrouter::SysUserOperations::instance());
              this->bootstrap_options_["user"] =
                  this->bootstrap_options_["_username"];
              // Remove temporary meta-options.
              this->bootstrap_options_.erase("_username");
            }
          });
#endif

      arg_handler.add_option(
          OptionNames({"-c", "--config"}),
          "Only read configuration from given file.",
          CmdOptionValueReq::required, "path",
          [this](const std::string &value) {
            if (!config_files_.empty()) {
              throw std::runtime_error(
                  "Option -c/--config can only be used once; "
                  "use -a/--extra-config instead.");
            }

            check_and_add_conf(config_files_, value);
          });
    }
    bool is_legacy() const override { return is_legacy_; }

   private:
    void check_and_add_conf(std::vector<std::string> &configs,
                            const std::string &value);
    bool &is_legacy_;
    std::vector<std::string> &config_files_;
  };

  std::string router_program_name_;
  mysql_harness::Path origin_;
  CmdArgHandler arg_handler_;

  KeyringHandler keyring_;
  MySQLRouterAndMrsConf bootstrapper_;

  BootstrapCredentials mrs_metadata_account_;
  BootstrapCredentials mrs_data_account_;
  std::string mrs_secret_;

  bool bootstrap_mrs_{false};
  bool is_legacy_{true};
  bool showing_info_{false};
  std::vector<std::string> config_files_;

  struct RoutingConfig {
    std::string key;
    bool is_metadata_cache;
  };
  std::pair<RoutingConfig, RoutingConfig> get_config_classic_sections();

  mysql_harness::Config config_{mysql_harness::Config::allow_keys};

  void parse_command_options(std::vector<std::string> arguments);
  void prepare_command_options(const std::string &bootstrap_uri);

  std::string get_version_line() noexcept;
  void show_help();
  void show_usage() noexcept;

  void configure_mrs(mysqlrouter::MySQLSession *session,
                     const std::string &config_path);

  void check_mrs_metadata(mysqlrouter::MySQLSession *session) const;

  void load_configuration(const std::string &path);

  bool can_configure_mrs(const std::string &config_path) const;

  void create_mrs_users(mysqlrouter::MySQLSession *session,
                        uint64_t mrs_router_id);
  void store_mrs_data_in_keyring();
  void store_mrs_configuration(const std::string &config_path,
                               uint64_t mrs_router_id);
  uint64_t register_mrs_router_instance(mysqlrouter::MySQLSession *session);

  std::string get_configured_router_name() const;
  std::string get_configured_rest_endpoint() const;

  void store_mrs_account_metadata(mysqlrouter::MySQLSession *session,
                                  uint64_t mrs_router_id,
                                  const std::string &key,
                                  const std::string &user,
                                  const std::vector<std::string> &hosts);
};

#endif  // ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_CONFIGURATOR_H_
