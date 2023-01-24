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

#ifndef ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_ARGUMENTS_H_
#define ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_ARGUMENTS_H_

#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "my_macros.h"

#include "mysql/harness/arg_handler.h"
#include "mysql/harness/config_option.h"
#include "mysql/harness/filesystem.h"
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/mysql_session.h"

#include "bootstrap_credentials.h"
#include "bootstrap_mysql_account.h"

using UniqueStrings = std::set<std::string>;
using Strings = std::vector<std::string>;
using CmdArguments = Strings;

class BootstrapArguments {
 private:
  class BooststrapCmdArgHandler : public CmdArgHandler {
   public:
    BooststrapCmdArgHandler(OptionContainer options,
                            bool ignore_unknown_arguments_)
        : CmdArgHandler(false, ignore_unknown_arguments_) {
      for (const auto &o : options) {
        add_option(o);
      }
    }
  };

 public:
  BootstrapArguments() {}

  void analyze(CmdArguments &arguments) {
    router_arguments.clear();

    path_this_application_ =
        mysqlrouter::find_full_executable_path(arguments.front());
    path_router_application_ = get_executable_mysqlrouter();
    arguments.erase(arguments.begin());

    if (arguments.size() == 0) {
      throw std::runtime_error(
          "Bootstrap requires at least one parameter with URI that points "
          "MySQL-Server.");
    }

    auto buri = arguments.front();
    if (arguments.front()[0] != '-') {
      arguments = Strings(arguments.begin() + 1, arguments.end());
    }
    router_parameters_.process(arguments);

    if (bootstrap_mrs && bootstrap_disable_rest)
      throw std::runtime_error(
          "invalid configuration, disabled REST still MRS plugin was "
          "selected for configuration.");

    if (standalone && !bootstrap_mrs)
      throw std::runtime_error(
          "Option --standalone only allowed when used together with --mrs");

    bool skip_next = false;
    for (const auto &a : arguments) {
      if (skip_next) {
        skip_next = false;
        continue;
      }
      if (!is_mrs_parameter(a)) router_arguments.push_back(a);
      skip_next = mrs_parameter_needs_second_argument(a);
    }

    if (!(version || help)) {
      if (buri.empty()) {
        throw std::runtime_error(
            "Bootstrap requires that last parameter is set URI that points "
            "MySQL-Server.");
      }
      bootstrap_uri = buri;
      router_arguments.push_back("-B");
      router_arguments.push_back(bootstrap_uri);
    }
  }

 private:
  const std::string k_mrs_param_mrs{"--mrs"};
  const std::string k_mrs_param_disable_mrs{"--disable-mrs"};
  const std::string k_mrs_param_standalone{"--standalone"};
  const std::string k_mrs_param_mrs_jwt_secret{"--mrs-global-secret"};
  const std::string k_mrs_param_mrs_data_account{"--mrs-data-account"};
  const std::string k_mrs_param_mrs_metadata_account{"--mrs-metadata-account"};

  bool mrs_parameter_needs_second_argument(std::string parameter) {
    const static std::set<std::string> mrs_parameters{
        k_mrs_param_mrs_jwt_secret, k_mrs_param_mrs_data_account,
        k_mrs_param_mrs_metadata_account};

    auto equal_index = parameter.find("=");
    if (equal_index != parameter.npos) return false;
    parameter = parameter.substr(0, equal_index);
    return mrs_parameters.count(parameter);
  }

  bool is_mrs_parameter(std::string parameter) {
    const static std::set<std::string> mrs_parameters{
        k_mrs_param_mrs,
        k_mrs_param_disable_mrs,
        k_mrs_param_mrs_jwt_secret,
        k_mrs_param_mrs_data_account,
        k_mrs_param_mrs_metadata_account,
        k_mrs_param_standalone};

    parameter = parameter.substr(0, parameter.find("="));
    return mrs_parameters.count(parameter);
  }

  std::string get_executable_mysqlrouter() {
    const static std::string k_router_exe{
        IF_WIN("mysqlrouter.exe", "mysqlrouter")};

    auto application = path_this_application_.real_path();

    if (!application.is_directory()) {
      application = application.dirname();
    }

    application.append(k_router_exe);

    return application.str();
  }

  BooststrapCmdArgHandler router_parameters_{
      {{{"--password-retries"},
        "Number of the retries for generating the router's user password. "
        "(bootstrap)",
        CmdOptionValueReq::optional,
        "password-retries",
        [this](const std::string &retries) {
          user_options.password_retries =
              mysql_harness::option_as_uint<unsigned long>(
                  retries, "--password-retries", 1, kMaxPasswordRetries);
        }},
       {{"--name"},
        "",
        CmdOptionValueReq::optional,
        "name",
        [this](const std::string &name) { router_name = name; }},
       {{"--account-create"},
        "",
        CmdOptionValueReq::required,
        "mode",
        [this](const std::string &create) {
          if (create != "always" && create != "if-not-exists" &&
              create != "never")
            throw std::runtime_error(
                "Invalid value for --account-create option.  Valid values: "
                "always, if-not-exists, never");
          user_options.account_create = create;
        }},
       {{"--account"},
        "",
        CmdOptionValueReq::required,
        "mode",
        [this](const std::string &account) { user_options.account = account; }},
       {{"--force-password-validation"},
        "",
        CmdOptionValueReq::none,
        "",
        [this](const std::string &) {
          user_options.force_password_validation = true;
        }},
       {{"--version", "-V"},
        " ",
        CmdOptionValueReq::none,
        "version",
        [this](const std::string &) { version = true; }},
       {{"--help"},
        " ",
        CmdOptionValueReq::none,
        "help",
        [this](const std::string &) { help = true; }},
       {{"--account-host"},
        "",
        CmdOptionValueReq::required,
        "account-host",
        [this](const std::string &account_host) {
          bootstrap_account_hosts.insert(account_host);
        }},
       {{"--bootstrap-socket"},
        "Bootstrap and configure Router via a Unix socket",
        CmdOptionValueReq::required,
        "socket_name",
        [this](const std::string &socket_name) {
          bootstrap_socket = socket_name;
        }},
       {{"--disable-rest"},
        "Disable REST web service for Router monitoring",
        CmdOptionValueReq::none,
        "",
        [this](const std::string &) { bootstrap_disable_rest = true; }},
       {{"--user"},
        "",
        CmdOptionValueReq::required,
        "",
        [this](const std::string &user) { bootstrap_user = user; }},
       {{"-d", "--directory"},
        "Creates a self-contained directory for a new instance of the Router. "
        "(bootstrap)",
        CmdOptionValueReq::required,
        "directory",
        [this](const std::string &path) { bootstrap_directory = path; }},

       {{"--ssl-cipher"},
        "",
        CmdOptionValueReq::required,
        "ssl-cipher",
        [this](const std::string &cipher) { ssl_cipher = cipher; }},
       {{"--tls-version"},
        "",
        CmdOptionValueReq::required,
        "tls-version",
        [this](const std::string &tls_ver) { tls_version = tls_ver; }},
       {{"--ssl-ca"},
        "",
        CmdOptionValueReq::required,
        "ssl-cat",
        [this](const std::string &ca) { ssl_ca = ca; }},
       {{"--ssl-capath"},
        "",
        CmdOptionValueReq::required,
        "ssl-capath",
        [this](const std::string &capath) { ssl_capath = capath; }},
       {{"--ssl-crl"},
        "",
        CmdOptionValueReq::required,
        "ssl-crl",
        [this](const std::string &crl) { ssl_crl = crl; }},
       {{"--ssl-crlpath"},
        "",
        CmdOptionValueReq::required,
        "ssl-crlpath",
        [this](const std::string &crlpath) { ssl_crlpath = crlpath; }},
       {{"--ssl-cert"},
        "",
        CmdOptionValueReq::required,
        "ssl-cert",
        [this](const std::string &cert) { ssl_cert = cert; }},
       {{"--ssl-key"},
        "",
        CmdOptionValueReq::required,
        "ssl-key",
        [this](const std::string &key) { ssl_key = key; }},
       {{"--ssl-mode"},
        "",
        CmdOptionValueReq::required,
        "ssl-mode",
        [this](const std::string &key) { ssl_mode = key; }},

       {{k_mrs_param_standalone},
        "Bootstrap Router in standalone server mode (no InnoDB Cluster). Only "
        "for use with MRS.",
        CmdOptionValueReq::none,
        "standalone",
        [this](const std::string &) { standalone = true; }},
       {{k_mrs_param_mrs_metadata_account},
        " ",
        CmdOptionValueReq::required,
        "mrs-metadata-account",
        [this](const std::string &account) {
          mrs_metadata_account.user = account;
        }},
       {{k_mrs_param_mrs_data_account},
        " ",
        CmdOptionValueReq::required,
        "mrs-data-account",
        [this](const std::string &account) {
          mrs_data_account.user = account;
        }},
       {{k_mrs_param_mrs_jwt_secret},
        " ",
        CmdOptionValueReq::required,
        "mrs-global-secret",
        [this](const std::string &secret) { mrs_secret = secret; }},
       {{k_mrs_param_mrs},
        "Enable MRS plugin. Write configurations and setup MySQL accounts for "
        "MRS.",
        CmdOptionValueReq::none,
        "mrs",
        [this](const std::string &) { bootstrap_mrs = true; }},
       {{k_mrs_param_disable_mrs},
        "Disable MRS plugin. Do not write configuration and skip "
        "initialization of MySQL account for MRS.",
        CmdOptionValueReq::none,
        "disable-mrs",
        [this](const std::string &) { bootstrap_mrs = false; }}},
      true};

 public:
  mysql_harness::Path path_this_application_;
  mysql_harness::Path path_router_application_;
  uint64_t connect_timeout{mysqlrouter::MySQLSession::kDefaultConnectTimeout};
  uint64_t read_timeout{mysqlrouter::MySQLSession::kDefaultReadTimeout};
  std::string ssl_mode{mysqlrouter::MySQLSession::kSslModePreferred};
  std::string ssl_cipher;
  std::string tls_version;
  std::string ssl_ca;
  std::string ssl_capath;
  std::string ssl_crl;
  std::string ssl_crlpath;
  std::string ssl_cert;
  std::string ssl_key;
  std::string router_name = "system";
  bool version{false};
  bool help{false};

  UserOptions user_options;

  CmdArguments router_arguments;
  bool standalone{false};  // standalone vs innodb cluster
  std::string bootstrap_uri;
  std::string bootstrap_socket;
  std::string bootstrap_directory;
  std::string bootstrap_user;
  UniqueStrings bootstrap_account_hosts;
  bool bootstrap_disable_rest{false};
  bool bootstrap_mrs{false};
  BootstrapCredentials mrs_metadata_account{};
  BootstrapCredentials mrs_data_account{};
  std::string mrs_secret{};
};

#endif  // ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_ARGUMENTS_H_
