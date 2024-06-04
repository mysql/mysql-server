/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

// TODO: add support for connecting to target and detecting its type (InnoDB
// Cluster, standalone, MDS HA) at the very beginning, so that
// topology specific option validations can be added

#include "router_conf.h"
#include <string>

#include "common.h"  // truncate_string
#include "dim.h"
#include "hostname_validator.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/config_option.h"
#include "mysql/harness/logging/logger_plugin.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"
#include "mysql/harness/utility/string.h"  // string_format
#include "mysql/harness/vt100.h"
#include "mysqlrouter/config_files.h"
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils.h"

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <string.h>
#include <windows.h>
#include "mysqlrouter/windows/password_vault.h"
#include "mysqlrouter/windows/service_operations.h"
#endif

IMPORT_LOG_FUNCTIONS()
using namespace std::string_literals;

using mysql_harness::DIM;
using mysql_harness::make_upper;
using mysql_harness::truncate_string;
using mysql_harness::utility::string_format;
using mysqlrouter::MySQLSession;

static const char *kDefaultKeyringFileName = "keyring";

namespace {
/**
 * assert 'value' is one of the allowed values.
 *
 * value is compared case-insensitive
 *
 * @param key key name to report in case of failure
 * @param value value to check
 * @param allowed_values allowed values.
 *
 * @throws std::invalid_argument if value is not part of allowed_values.
 */
void assert_one_of_ci(const std::string &key, const std::string &value,
                      std::initializer_list<const char *> allowed_values) {
  const auto value_upper = make_upper(value);

  const auto it = std::find_if(allowed_values.begin(), allowed_values.end(),
                               [&value_upper](const auto &allowed_value) {
                                 return value_upper == allowed_value;
                               });

  if (it == allowed_values.end()) {
    throw std::invalid_argument("value '" + value + "' provided to " + key +
                                " is not one of " +
                                mysql_harness::join(allowed_values, ","));
  }
}

void assert_option_value_in_range(const std::string &value, const int min,
                                  const int max) {
  try {
    std::size_t last_char = 0;
    auto val = std::stoi(value, &last_char);
    if (last_char != value.size())
      throw std::invalid_argument{"invalid value: " + value};

    if (val < min || val > max) {
      throw std::out_of_range{std::string{"not in allowed range ["} +
                              std::to_string(min) + ", " + std::to_string(max) +
                              "]"};
    }
  } catch (const std::invalid_argument &) {
    throw std::invalid_argument{"invalid value: " + value};
  }
}

// throws std::runtime_error on invalid data
mysqlrouter::URI parse_server_uri(const std::string &server_uri,
                                  const std::string &bootstrap_socket) {
  // Extract connection information from the bootstrap server URI.
  std::string uri;
  const std::string default_schema = "mysql://";
  if (server_uri.compare(0, default_schema.size(), default_schema) != 0) {
    uri = default_schema + server_uri;
  } else {
    uri = server_uri;
  }

  mysqlrouter::URI u;
  try {
    // don't allow rootless URIs (mailto:foo@...) which would collide with the
    // schema-less URIs are allow too: root:pw@host
    u = mysqlrouter::URIParser::parse(uri, false);
  } catch (const mysqlrouter::URIError &e) {
    throw std::runtime_error(e.what());
  }

  // query, fragment and path should all be empty
  if (!u.fragment.empty()) {
    throw std::runtime_error(
        "the bootstrap URI contains a #fragement, but shouldn't");
  }
  if (!u.query.empty()) {
    throw std::runtime_error(
        "the bootstrap URI contains a ?query, but shouldn't");
  }
  if (!u.path.empty()) {
    throw std::runtime_error(
        "the bootstrap URI contains a /path, but shouldn't");
  }

  if (u.username.empty()) {
    u.username = "root";
  }
  // we need to prompt for the password
  if (u.password.empty()) {
    u.password = mysqlrouter::prompt_password(
        "Please enter MySQL password for " + u.username);
  }

  if (bootstrap_socket.size() > 0) {
    // enforce host == "localhost" if a socket is used to avoid ambiguity with
    // the possible hostname
    if (u.host != "localhost") {
      throw std::runtime_error(
          "--bootstrap-socket given, but --bootstrap option contains a "
          "non-'localhost' hostname: " +
          u.host);
    }
  }

  return u;
}

inline std::string get_opt(const std::map<std::string, std::string> &map,
                           const std::string &key,
                           const std::string &default_value) {
  auto iter = map.find(key);
  if (iter == map.end()) return default_value;
  return iter->second;
}

// throws std::logic_error, std::runtime_error, Error(runtime_error)
/*static*/
void set_ssl_options(MySQLSession *sess,
                     const std::map<std::string, std::string> &options) {
  std::string ssl_mode =
      get_opt(options, "ssl_mode", MySQLSession::kSslModePreferred);
  std::string ssl_cipher = get_opt(options, "ssl_cipher", "");
  std::string tls_version = get_opt(options, "tls_version", "");
  std::string ssl_ca = get_opt(options, "ssl_ca", "");
  std::string ssl_capath = get_opt(options, "ssl_capath", "");
  std::string ssl_crl = get_opt(options, "ssl_crl", "");
  std::string ssl_crlpath = get_opt(options, "ssl_crlpath", "");

  std::string ssl_cert = get_opt(options, "ssl_cert", "");
  std::string ssl_key = get_opt(options, "ssl_key", "");

  // parse ssl_mode option (already validated in cmdline option handling)
  mysql_ssl_mode ssl_enum =
      MySQLSession::parse_ssl_mode(ssl_mode);  // throws std::logic_error

  // set ssl mode + server authentication options
  sess->set_ssl_options(
      ssl_enum, tls_version, ssl_cipher, ssl_ca, ssl_capath, ssl_crl,
      ssl_crlpath);  // throws Error, std::invalid_argument(logic_error)

  // set client authentication options
  if (!ssl_cert.empty() || !ssl_key.empty()) {
    sess->set_ssl_cert(ssl_cert, ssl_key);  // throws Error(runtime_error)
  }
}
}  // namespace

void MySQLRouterConf::prepare_command_options(
    CmdArgHandler &arg_handler, const std::string &bootstrap_uri) noexcept {
  using OptionNames = CmdOption::OptionNames;

  bootstrap_uri_ = bootstrap_uri;

  arg_handler.add_option(
      OptionNames({"--account"}),
      "Account (username) to be used by Router when talking to cluster."
      " (bootstrap)",
      CmdOptionValueReq::required, "account",
      [this](const std::string &username) {
        if (username.empty())
          throw std::runtime_error(
              "Value for --account option cannot be empty");
        if (this->bootstrap_options_.count("account"))
          throw std::runtime_error("Option --account can only be given once");
        this->bootstrap_options_["account"] = username;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--account");
      });

  arg_handler.add_option(
      OptionNames({"--account-create"}),
      "Specifies account creation policy (useful for guarding against "
      "accidentally bootstrapping using a wrong account). <mode> is one of:\n"
      "  'always'        - bootstrap only if account doesn't exist\n"
      "  'never'         - bootstrap only if account exists\n"
      "  'if-not-exists' - bootstrap either way (default)\n"
      "This option can only be used if option '--account' is also used.\n"
      "Argument 'never' cannot be used together with option "
      "'--account-host'\n"
      "(bootstrap)",
      CmdOptionValueReq::required, "mode",
      [this](const std::string &create) {
        if (create != "always" && create != "if-not-exists" &&
            create != "never")
          throw std::runtime_error(
              "Invalid value for --account-create option.  Valid values: "
              "always, if-not-exists, never");
        if (this->bootstrap_options_.count("account-create"))
          throw std::runtime_error(
              "Option --account-create can only be given once");
        this->bootstrap_options_["account-create"] = create;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--account-create");
        if (!this->bootstrap_options_.count("account"))
          throw std::runtime_error(
              "Option --account-create can only be used together with "
              "--account.");
      });

  arg_handler.add_option(
      OptionNames({"--account-host"}),
      "Host pattern to be used when creating Router's database user, "
      "default='%'. "
      "It can be used multiple times to provide multiple patterns. "
      "(bootstrap)",
      CmdOptionValueReq::required, "account-host",
      [this](const std::string &host_pattern) {
        std::vector<std::string> &hostnames =
            this->bootstrap_multivalue_options_["account-host"];
        hostnames.push_back(host_pattern);

        // sort and eliminate any non-unique hostnames; we do this to ensure
        // that CREATE USER does not get called twice for the same user@host
        // later on in the ConfigGenerator
        std::sort(hostnames.begin(), hostnames.end());
        auto it = std::unique(hostnames.begin(), hostnames.end());
        hostnames.resize(std::distance(hostnames.begin(), it));
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--account-host");
        const auto it = this->bootstrap_options_.find("account-create");
        if (it != this->bootstrap_options_.end() && it->second == "never")
          throw std::runtime_error(
              "Option '--account-create never' cannot be used together with "
              "'--account-host <host>'");
      });

  if (bootstrap_uri.empty()) {
    arg_handler.add_option(
        OptionNames({"-B", "--bootstrap"}),
        "Bootstrap and configure Router for operation with a MySQL InnoDB "
        "cluster.",
        CmdOptionValueReq::required, "server_url",
        [this](const std::string &server_url) {
          if (server_url.empty()) {
            throw std::runtime_error("Invalid value for --bootstrap/-B option");
          }
          this->bootstrap_uri_ = server_url;
        });
  }

  arg_handler.add_option(
      OptionNames({"--bootstrap-socket"}),
      "Bootstrap and configure Router via a Unix socket",
      CmdOptionValueReq::required, "socket_name",
      [this](const std::string &socket_name) {
        if (socket_name.empty()) {
          throw std::runtime_error(
              "Invalid value for --bootstrap-socket option");
        }

        this->save_bootstrap_option_not_empty("--bootstrap-socket",
                                              "bootstrap_socket", socket_name);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--bootstrap-socket");
      });

  arg_handler.add_option(
      OptionNames({"--client-ssl-cert"}),
      "name of a PEM file containing a SSL certificate used "
      "for accepting TLS connections between client and router",
      CmdOptionValueReq::required, "path",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--client-ssl-cert",
                                              "client_ssl_cert", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-cert");

        if (!bootstrap_options_["client_ssl_cert"].empty() &&
            bootstrap_options_["client_ssl_key"].empty()) {
          throw std::runtime_error(
              "If --client-ssl-cert is set, --client-ssl-key can't be empty.");
        }
      });

  arg_handler.add_option(
      OptionNames({"--client-ssl-cipher"}),
      "list of one or more colon separated cipher names used for accepting "
      "TLS connections between client and router",
      CmdOptionValueReq::required, "",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--client-ssl-cipher",
                                              "client_ssl_cipher", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-cipher");
      });

  arg_handler.add_option(
      OptionNames({"--client-ssl-curves"}),
      "list of one or more colon separated elliptic curve names used for "
      "accepting TLS connections between client and router",
      CmdOptionValueReq::required, "",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--client-ssl-curves",
                                              "client_ssl_curves", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-curves");
      });

  arg_handler.add_option(
      OptionNames({"--client-ssl-key"}),
      "name of a PEM file containing a SSL private key used "
      "for accepting TLS connections between client and router",
      CmdOptionValueReq::required, "path",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--client-ssl-key",
                                              "client_ssl_key", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-key");

        if (!bootstrap_options_["client_ssl_key"].empty() &&
            bootstrap_options_["client_ssl_cert"].empty()) {
          throw std::runtime_error(
              "If --client-ssl-key is set, --client-ssl-cert can't be empty.");
        }
      });

  arg_handler.add_option(
      OptionNames({"--client-ssl-mode"}),
      "SSL mode for connections from client to router. One "
      "of DISABLED, PREFERRED, REQUIRED or PASSTHROUGH.",
      CmdOptionValueReq::required, "mode",
      [this](const auto &value) {
        assert_one_of_ci("--client-ssl-mode", value,
                         {"DISABLED", "PREFERRED", "REQUIRED", "PASSTHROUGH"});

        this->save_bootstrap_option_not_empty(
            "--client-ssl-mode", "client_ssl_mode", make_upper(value));
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-mode");

        if (bootstrap_options_["client_ssl_mode"] == "PASSTHROUGH") {
          auto server_ssl_mode_it = bootstrap_options_.find("server_ssl_mode");
          if (server_ssl_mode_it != bootstrap_options_.end()) {
            if (server_ssl_mode_it->second != "AS_CLIENT") {
              throw std::runtime_error(
                  "--server-ssl-mode must be AS_CLIENT or not specified, if "
                  "--client-ssl-mode is PASSTHROUGH.");
            }
          }
        }
      });
  arg_handler.add_option(
      OptionNames({"--client-ssl-dh-params"}),
      "name of a PEM file containing DH paramaters",
      CmdOptionValueReq::required, "",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--client-ssl-dh-params",
                                              "client_ssl_dh_params", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-dh-params");
      });

  arg_handler.add_option(
      OptionNames({"--conf-base-port"}),
      "Base port to use for listening router ports. (bootstrap)",
      CmdOptionValueReq::required, "port",
      [this](const std::string &port) {
        this->bootstrap_options_["base-port"] = port;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-base-port");
      });

  arg_handler.add_option(
      OptionNames({"--conf-bind-address"}),
      "IP address of the interface to which router's listening sockets "
      "should bind. (bootstrap)",
      CmdOptionValueReq::required, "address",
      [this](const std::string &address) {
        this->bootstrap_options_["bind-address"] = address;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-bind-address");
      });

#ifndef _WIN32
  arg_handler.add_option(
      OptionNames({"--conf-skip-tcp"}),
      "Whether to disable binding of a TCP port for incoming connections. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const std::string &) {
        this->bootstrap_options_["skip-tcp"] = "1";
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-skip-tcp");
      });
  arg_handler.add_option(
      OptionNames({"--conf-use-sockets"}),
      "Whether to use Unix domain sockets. (bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const std::string &) {
        this->bootstrap_options_["use-sockets"] = "1";
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-use-sockets");
      });
#endif

  arg_handler.add_option(
      OptionNames({"--conf-skip-x-protocol"}),
      "Whether to disable routing of X protocol connections. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const std::string &) {
        this->bootstrap_options_["skip-x-protocol"] = "1";
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-skip-x-protocol");
      });

  arg_handler.add_option(
      OptionNames({"--connect-timeout"}),
      "The time in seconds after which trying to connect to metadata server "
      "should timeout. It is used when bootstrapping and also written to the "
      "configuration file (bootstrap)",
      CmdOptionValueReq::optional, "",
      [this](const std::string &connect_timeout) {
        this->bootstrap_options_["connect-timeout"] = connect_timeout;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--connect-timeout");
      });

  arg_handler.add_option(
      OptionNames({"--conf-use-gr-notifications"}),
      "Whether to enable handling of cluster state change GR notifications.",
      CmdOptionValueReq::optional, "",
      [this](const std::string &value) {
        if (value == "0" || value == "1") {
          this->bootstrap_options_["use-gr-notifications"] = value;
        } else if (value.empty()) {
          this->bootstrap_options_["use-gr-notifications"] = "1";
        } else {
          throw std::runtime_error(
              "Value for parameter '--conf-use-gr-notifications' needs to be "
              "one of: ['0', '1']");
        }
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-use-gr-notifications");
      });

  arg_handler.add_option(
      OptionNames({"--conf-target-cluster"}),
      "Router's target Cluster from the ClusterSet('current' or 'primary').",
      CmdOptionValueReq::required, "",
      [this](const std::string &value) {
        if (this->bootstrap_options_.count("target-cluster-by-name") > 0) {
          throw std::runtime_error(
              "Parameters '--conf-target-cluster' and "
              "'--conf-target-cluster-by-name' are mutually exclusive and "
              "can't be used together");
        }

        std::string value_lowercase{value};
        std::transform(value_lowercase.begin(), value_lowercase.end(),
                       value_lowercase.begin(), ::tolower);

        if (value_lowercase != "primary" && value_lowercase != "current") {
          throw std::runtime_error(
              "Value for parameter '--conf-target-cluster' needs to be one of: "
              "['primary', 'current']");
        }

        this->bootstrap_options_["target-cluster"] = value_lowercase;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-target-cluster");
      });

  arg_handler.add_option(
      OptionNames({"--conf-target-cluster-by-name"}),
      "Name of the target Cluster for the Router when bootstrapping against "
      "the ClusterSet",
      CmdOptionValueReq::required, "",
      [this](const std::string &value) {
        if (this->bootstrap_options_.count("target-cluster") > 0) {
          throw std::runtime_error(
              "Parameters '--conf-target-cluster' and "
              "'--conf-target-cluster-by-name' are mutually exclusive and "
              "can't be used together");
        }
        if (value.empty()) {
          throw std::runtime_error(
              "Value for parameter '--conf-target-cluster-by-name' can't be "
              "empty");
        }
        this->bootstrap_options_["target-cluster-by-name"] = value;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-target-cluster-by-name");
      });

  arg_handler.add_option(
      OptionNames({"-d", "--directory"}),
      "Creates a self-contained directory for a new instance of the Router. "
      "(bootstrap)",
      CmdOptionValueReq::required, "directory",
      [this](const std::string &path) {
        if (path.empty()) {
          throw std::runtime_error("Invalid value for --directory option");
        }
        this->bootstrap_directory_ = path;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("-d/--directory");
      });

  arg_handler.add_option(
      OptionNames({"--force"}),
      "Force reconfiguration of a possibly existing instance of the router. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const std::string &) { this->bootstrap_options_["force"] = "1"; },
      [this](const std::string &) { this->assert_bootstrap_mode("--force"); });

  arg_handler.add_option(
      OptionNames({"--force-password-validation"}),
      "When autocreating database account do not use HASHED password - this is "
      "the default behavior now, this parameter is not needed, it is kept for "
      "backward compatibility."
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [](const std::string &) {
        // this is now always assumed, so this parameter is ignored, kept for
        // backward compatibility
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--force-password-validation");
      });

  arg_handler.add_option(
      OptionNames({"--master-key-reader"}),
      "The tool that can be used to read master key, it has to be used "
      "together with --master-key-writer. (bootstrap)",
      CmdOptionValueReq::required, "",
      [this](const std::string &master_key_reader) {
        this->keyring_info_.set_master_key_reader(master_key_reader);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--master-key-reader");
        if (this->keyring_info_.get_master_key_reader().empty() !=
            this->keyring_info_.get_master_key_writer().empty())
          throw std::runtime_error(
              "Option --master-key-reader can only be used together with "
              "--master-key-writer.");
      });

  arg_handler.add_option(
      OptionNames({"--master-key-writer"}),
      "The tool that can be used to store master key, it has to be used "
      "together with --master-key-reader. (bootstrap)",
      CmdOptionValueReq::required, "",
      [this](const std::string &master_key_writer) {
        this->keyring_info_.set_master_key_writer(master_key_writer);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--master-key-writer");
        if (this->keyring_info_.get_master_key_reader().empty() !=
            this->keyring_info_.get_master_key_writer().empty())
          throw std::runtime_error(
              "Option --master-key-writer can only be used together with "
              "--master-key-reader.");
      });

  arg_handler.add_option(
      OptionNames({"--name"}),
      "Gives a symbolic name for the router instance. (bootstrap)",
      CmdOptionValueReq::optional, "name",
      [this](const std::string &name) {
        this->bootstrap_options_["name"] = name;
      },
      [this](const std::string &) { this->assert_bootstrap_mode("--name"); });

  arg_handler.add_option(
      OptionNames({"--password-retries"}),
      "Number of the retries for generating the router's user password. "
      "(bootstrap)",
      CmdOptionValueReq::optional, "password-retries",
      [this](const std::string &retries) {
        this->bootstrap_options_["password-retries"] = retries;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--password-retries");
      });

  arg_handler.add_option(
      OptionNames({"--read-timeout"}),
      "The time in seconds after which reads from metadata server should "
      "timeout. It is used when bootstrapping and is also written to "
      "configuration file. (bootstrap)",
      CmdOptionValueReq::optional, "",
      [this](const std::string &read_timeout) {
        this->bootstrap_options_["read-timeout"] = read_timeout;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--read-timeout");
      });
  arg_handler.add_option(
      OptionNames({"--report-host"}),
      "Host name of this computer (it will be queried from OS if not "
      "provided). "
      "It is used as suffix (the part after '@') in Router's database user "
      "name; "
      "should match host name as seen by the cluster nodes (bootstrap)",
      CmdOptionValueReq::required, "report-host",
      [this](const std::string &hostname) {
        if (!mysql_harness::is_valid_hostname(hostname.c_str()))
          throw std::runtime_error(
              "Option --report-host has an invalid value.");

        auto pr = this->bootstrap_options_.insert({"report-host", hostname});
        if (pr.second == false)
          throw std::runtime_error(
              "Option --report-host can only be used once.");
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--report-host");
      });

  arg_handler.add_option(
      OptionNames({"--server-ssl-ca"}),
      "path name of the Certificate Authority (CA) certificate file in PEM "
      "format. Used when forwarding a client connection from router to a "
      "server.",
      CmdOptionValueReq::required, "path",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-ca",
                                              "server_ssl_ca", value);
      },
      [this](const auto &) { this->assert_bootstrap_mode("--server-ssl-ca"); });

  arg_handler.add_option(
      OptionNames({"--server-ssl-capath"}),
      "path name of the directory that contains trusted SSL Certificate "
      "Authority (CA) certificate files in PEM format. Used when forwarding "
      "a client connection from router to a server.",
      CmdOptionValueReq::required, "directory",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-capath",
                                              "server_ssl_capath", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-capath");
      });

  arg_handler.add_option(
      OptionNames({"--server-ssl-cipher"}),
      "list of one or more colon separated cipher names. Used when "
      "forwarding "
      "client connection from router to a server.",
      CmdOptionValueReq::required, "",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-cipher",
                                              "server_ssl_cipher", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-cipher");
      });

  arg_handler.add_option(
      OptionNames({"--server-ssl-crl"}),
      "path name of the file containing certificate revocation lists in PEM "
      "format. Used when forwarding a client connection from router to a "
      "server.",
      CmdOptionValueReq::required, "path",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-crl",
                                              "server_ssl_crl", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-crl");
      });

  arg_handler.add_option(
      OptionNames({"--server-ssl-crlpath"}),
      "path name of the directory that contains certificate revocation-list "
      "files in PEM format. Used when forwarding a client connection from "
      "router to a server.",
      CmdOptionValueReq::required, "directory",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-crlpath",
                                              "server_ssl_crlpath", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-crlpath");
      });

  arg_handler.add_option(
      OptionNames({"--server-ssl-curves"}),
      "list of one or more colon separated elliptic curve names. Used when "
      "forwarding a client connection from router to a server.",
      CmdOptionValueReq::required, "",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-curves",
                                              "server_ssl_curves", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-curves");
      });

  arg_handler.add_option(
      OptionNames({"--server-ssl-mode"}),
      "SSL mode to use when forwarding a client connection from router to a "
      "server. One of DISABLED, PREFERRED, REQUIRED or AS_CLIENT.",
      CmdOptionValueReq::required, "ssl-mode",
      [this](const auto &value) {
        assert_one_of_ci("--server-ssl-mode", value,
                         {"DISABLED", "PREFERRED", "REQUIRED", "AS_CLIENT"});

        this->save_bootstrap_option_not_empty(
            "--server-ssl-mode", "server_ssl_mode", make_upper(value));
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-mode");
      });

  arg_handler.add_option(
      OptionNames({"--server-ssl-verify"}),
      "verification mode when forwarding a client connection from router to "
      "server. One of DISABLED, VERIFY_CA or VERIFY_IDENTITY.",
      CmdOptionValueReq::required, "verify-mode",
      [this](const auto &value) {
        assert_one_of_ci("--server-ssl-verify", value,
                         {"DISABLED", "VERIFY_CA", "VERIFY_IDENTITY"});

        this->save_bootstrap_option_not_empty(
            "--server-ssl-verify", "server_ssl_verify", make_upper(value));
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-verify");
      });

  arg_handler.add_option(
      OptionNames({"--ssl-ca"}),
      "Path to SSL CA file to verify server's certificate against when "
      "connecting to the metadata servers",
      CmdOptionValueReq::required, "path",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-ca", "ssl_ca", path);
      },
      [this](const std::string &) { this->assert_bootstrap_mode("--ssl-ca"); });

  arg_handler.add_option(
      OptionNames({"--ssl-capath"}),
      "Path to directory containing SSL CA files to verify server's "
      "certificate against when connecting to the metadata servers.",
      CmdOptionValueReq::required, "directory",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-capath", "ssl_capath",
                                              path);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-capath");
      });

  arg_handler.add_option(
      OptionNames({"--ssl-cert"}),
      "Path to a SSL certificate, to be used if client certificate "
      "verification is required when connecting to the metadata servers.",
      CmdOptionValueReq::required, "path",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-cert", "ssl_cert", path);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-cert");
      });

  arg_handler.add_option(
      OptionNames({"--ssl-cipher"}),
      ": separated list of SSL ciphers to allow when connecting to the "
      "metadata servers, if SSL is enabled.",
      CmdOptionValueReq::required, "ciphers",
      [this](const std::string &cipher) {
        this->save_bootstrap_option_not_empty("--ssl-cipher", "ssl_cipher",
                                              cipher);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-cipher");
      });

  arg_handler.add_option(
      OptionNames({"--ssl-crl"}),
      "Path to SSL CRL file to use when connecting to metadata-servers and "
      "verifying their SSL certificate",
      CmdOptionValueReq::required, "path",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-crl", "ssl_crl", path);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-crl");
      });

  arg_handler.add_option(
      OptionNames({"--ssl-crlpath"}),
      "Path to directory containing SSL CRL files to use when connecting to "
      "metadata-servers and verifying their SSL certificate.",
      CmdOptionValueReq::required, "directory",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-crlpath", "ssl_crlpath",
                                              path);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-crlpath");
      });

  arg_handler.add_option(
      OptionNames({"--ssl-key"}),
      "Path to private key for client SSL certificate, to be used if client "
      "certificate verification is required when connecting to "
      "metadata-servers.",
      CmdOptionValueReq::required, "path",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-key", "ssl_key", path);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-key");
      });

  arg_handler.add_option(
      OptionNames({"--disable-rw-split"}),
      "Do not generate routing section for RW Split endpoint",
      CmdOptionValueReq::none, "",
      [this](const std::string &) {
        this->bootstrap_options_["disable-rw-split"] = "1";
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--disable-rw-split");
      });

  arg_handler.add_option(
      OptionNames({"--disable-rest"}),
      "Disable REST web service for Router monitoring", CmdOptionValueReq::none,
      "",
      [this](const std::string &) {
        this->bootstrap_options_["disable-rest"] = "1";
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--disable-rest");
      });

  arg_handler.add_option(
      OptionNames({"--https-port"}),
      "HTTPS port for Router monitoring REST web service",
      CmdOptionValueReq::required, "https-port",
      [this](const std::string &https_port) {
        this->bootstrap_options_["https-port"] = https_port;
      },
      [this](const std::string &https_port) {
        this->assert_bootstrap_mode("--https-port");
        if (this->bootstrap_options_.count("disable-rest") != 0) {
          throw std::runtime_error(
              "Option --disable-rest is not allowed when using --https-port "
              "option");
        }
        try {
          assert_option_value_in_range(https_port, 1, 65535);
        } catch (const std::exception &e) {
          throw std::runtime_error{
              std::string{"processing --https-port option failed, "} +
              e.what()};
        }
      });

  char ssl_mode_vals[128];
  char ssl_mode_desc[384];
  snprintf(ssl_mode_vals, sizeof(ssl_mode_vals), "%s|%s|%s|%s|%s",
           MySQLSession::kSslModeDisabled, MySQLSession::kSslModePreferred,
           MySQLSession::kSslModeRequired, MySQLSession::kSslModeVerifyCa,
           MySQLSession::kSslModeVerifyIdentity);
  snprintf(ssl_mode_desc, sizeof(ssl_mode_desc),
           "SSL connection mode for use during bootstrap and normal operation, "
           "when connecting to the metadata server. Analogous to --ssl-mode in "
           "mysql client. One of %s. Default = %s. (bootstrap)",
           ssl_mode_vals, MySQLSession::kSslModePreferred);

  arg_handler.add_option(
      OptionNames({"--ssl-mode"}), ssl_mode_desc, CmdOptionValueReq::required,
      "mode",
      [this](const std::string &ssl_mode) {
        try {
          MySQLSession::parse_ssl_mode(
              ssl_mode);  // we only care if this succeeds
          bootstrap_options_["ssl_mode"] = ssl_mode;
        } catch (const std::logic_error &) {
          throw std::runtime_error("Invalid value for --ssl-mode option");
        }
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-mode");
      });

  arg_handler.add_option(
      OptionNames({"--strict"}),
      "Upgrades account verification failure warning into a fatal error. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const std::string &) { this->bootstrap_options_["strict"] = "1"; },
      [this](const std::string &) { this->assert_bootstrap_mode("--strict"); });

  arg_handler.add_option(
      OptionNames({"--tls-version"}),
      ", separated list of TLS versions to request, if SSL is enabled.",
      CmdOptionValueReq::required, "versions",
      [this](const std::string &version) {
        this->save_bootstrap_option_not_empty("--tls-version", "tls_version",
                                              version);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--tls-version");
      });

  arg_handler.add_option(
      OptionNames({"--conf-set-option"}),
      "Allows forcing selected option in the configuration file when "
      "bootstrapping (--conf-set-option=section_name.option_name=value)",
      CmdOptionValueReq::required, "conf-set-option",
      [this](const std::string &conf_option) {
        std::vector<std::string> &conf_options =
            this->bootstrap_multivalue_options_["conf-set-option"];
        conf_options.push_back(conf_option);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-set-option");
      });

// These are additional Windows-specific options, added (at the time of writing)
// in check_service_operations(). Grep after '--install-service' and you shall
// find.
#ifdef _WIN32
  arg_handler.add_option(
      CmdOption::OptionNames({"--clear-all-credentials"}),
      "Clear the vault, removing all the credentials stored on it",
      CmdOptionValueReq::none, "", [](const std::string &) {
        PasswordVault pv;
        pv.clear_passwords();
        log_info("Removed successfully all passwords from the vault.");
        throw silent_exception();
      });

  // in this context we only want the service-related options to be known and
  // displayed with --help; they are handled elsewhere (main-windows.cc)
  add_service_options(arg_handler);

  arg_handler.add_option(
      CmdOption::OptionNames({"--remove-credentials-section"}),
      "Removes the credentials for the given section",
      CmdOptionValueReq::required, "section_name",
      [](const std::string &value) {
        PasswordVault pv;
        pv.remove_password(value);
        pv.store_passwords();
        log_info("The password was removed successfully.");
        throw silent_exception();
      });

  arg_handler.add_option(
      CmdOption::OptionNames({"--update-credentials-section"}),
      "Updates the credentials for the given section",
      CmdOptionValueReq::required, "section_name",
      [](const std::string &value) {
        std::string prompt = string_format(
            "Enter password for config section '%s'", value.c_str());
        std::string pass = mysqlrouter::prompt_password(prompt);
        PasswordVault pv;
        pv.update_password(value, pass);
        pv.store_passwords();
        log_info("The password was stored in the vault successfully.");
        throw silent_exception();
      });
#endif
}

void MySQLRouterConf::save_bootstrap_option_not_empty(
    const std::string &option_name, const std::string &save_name,
    const std::string &option_value) {
  if (option_value.empty())
    throw std::runtime_error("Value for option '" + option_name +
                             "' can't be empty.");

  bootstrap_options_[save_name] = option_value;
}

void MySQLRouterConf::assert_bootstrap_mode(
    const std::string &option_name) const {
  if (this->bootstrap_uri_.empty())
    throw std::runtime_error("Option " + option_name +
                             " can only be used together with -B/--bootstrap");
}

int MySQLRouterConf::get_connect_timeout() const {
  const auto it = bootstrap_options_.find("connect-timeout");
  if (it != bootstrap_options_.end()) {
    return mysql_harness::option_as_uint<uint16_t>(it->second,
                                                   "--connect-timeout", 1);
  } else {
    return MySQLSession::kDefaultConnectTimeout;
  }
}

int MySQLRouterConf::get_read_timeout() const {
  const auto it = bootstrap_options_.find("read-timeout");
  if (it != bootstrap_options_.end()) {
    return mysql_harness::option_as_uint<uint16_t>(it->second, "--read-timeout",
                                                   1);
  } else {
    return MySQLSession::kDefaultReadTimeout;
  }
}

std::string MySQLRouterConf::get_bootstrap_socket() const {
  const auto &it = bootstrap_options_.find("bootstrap_socket");
  return (it != bootstrap_options_.end()) ? it->second : "";
}

void MySQLRouterConf::connect() {
  // connect to (what should be a) metadata server
  if (!mysql_)
    mysql_ = std::make_unique<MySQLSession>(
        std::make_unique<MySQLSession::LoggingStrategyDebugLogger>());
  try {
    // throws std::logic_error, std::runtime_error, Error(runtime_error)
    set_ssl_options(mysql_.get(), bootstrap_options_);
    mysql_->connect(target_uri_.host, target_uri_.port, target_uri_.username,
                    target_uri_.password, get_bootstrap_socket(), "",
                    get_connect_timeout(), get_read_timeout());
  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error("Unable to connect to the metadata server: "s +
                             e.what());
  }

  const auto result = mysqlrouter::setup_metadata_session(*mysql_);
  if (!result) {
    throw std::runtime_error("Failed setting up a metadata session: "s +
                             result.error().c_str());
  }
}

// throws MySQLSession::Error, std::runtime_error, std::out_of_range,
// std::logic_error, ... ?
std::string MySQLRouterConf::bootstrap(
    const std::string &program_name, const mysql_harness::Path &origin,
    bool allow_standalone, const std::string &plugin_folder
#ifndef _WIN32
    ,
    mysqlrouter::SysUserOperationsBase *sys_user_operations
#endif
) {
  auto print_skip_config = [this](const std::string &config_path) {
    assert(!is_legacy());
    // TODO add a --reconfigure option
    out_stream_ << Vt100::foreground(Vt100::Color::Yellow)
                << "# Skipping Router bootstrap"
                << Vt100::render(Vt100::Render::ForegroundDefault)
                << "\n\nConfiguration file already exists at " + config_path +
                       ", Router bootstrap skipped\n\n";
  };

  if (!mysql_)
    mysql_ = std::make_unique<MySQLSession>(
        std::make_unique<MySQLSession::LoggingStrategyDebugLogger>());

  mysqlrouter::ConfigGenerator config_gen(
      out_stream_, err_stream_
#ifndef _WIN32
      ,
      sys_user_operations ? sys_user_operations
                          : mysqlrouter::SysUserOperations::instance()
#endif
  );
  auto bootstrap_socket = get_bootstrap_socket();
  target_uri_ = parse_server_uri(bootstrap_uri_, bootstrap_socket);

  config_gen.init(
      bootstrap_options_, target_uri_, mysql_.get(), get_connect_timeout(),
      get_read_timeout());  // throws MySQLSession::Error, std::runtime_error,
                            // std::out_of_range, std::logic_error

  connect();
  config_gen.check_target(bootstrap_options_, allow_standalone);

  config_gen.warn_on_no_ssl(bootstrap_options_);  // throws std::runtime_error
  config_gen.set_plugin_folder(plugin_folder);

#ifdef _WIN32
  // Cannot run bootstrap mode as windows service since it requires console
  // interaction.
  if (mysqlrouter::is_running_as_service()) {
    std::string msg = "Cannot run router in boostrap mode as Windows service.";
    mysqlrouter::write_windows_event_log(msg);
    throw std::runtime_error(msg);
  }
#endif

  auto default_paths = mysqlrouter::get_default_paths(origin);

  if (bootstrap_directory_.empty()) {
    std::string config_file_path =
        mysql_harness::Path(default_paths.at("config_folder"s))
            .join("mysqlrouter.conf"s)
            .str();
    std::string state_file_path =
        mysql_harness::Path(default_paths.at("data_folder"s))
            .join("state.json"s)
            .str();
    std::string master_key_path =
        mysql_harness::Path(default_paths.at("config_folder"s))
            .join("mysqlrouter.key"s)
            .str();
    std::string default_keyring_file = default_paths.at("data_folder"s);
    mysql_harness::Path keyring_dir(default_keyring_file);
    if (!keyring_dir.exists()) {
      if (mysql_harness::mkdir(default_keyring_file,
                               mysqlrouter::kStrictDirectoryPerm, true) < 0) {
        log_error(
            "Cannot create directory '%s': %s",
            truncate_string(default_keyring_file).c_str(),
            std::error_code{errno, std::generic_category()}.message().c_str());
        throw std::runtime_error("Could not create keyring directory");
      } else {
        // sets the directory owner for the --user if provided
        config_gen.set_file_owner(bootstrap_options_, default_keyring_file);
        default_keyring_file = keyring_dir.real_path().str();
      }
    }
    default_keyring_file.append("/").append(kDefaultKeyringFileName);

    keyring_info_.set_keyring_file(default_keyring_file);
    keyring_info_.set_master_key_file(master_key_path);
    config_gen.set_keyring_info(keyring_info_);

    if (!is_legacy() && !config_gen.needs_bootstrap(config_file_path)) {
      print_skip_config(config_file_path);
      skipped_ = true;
    } else {
      config_gen.bootstrap_system_deployment(
          program_name, config_file_path, state_file_path, bootstrap_options_,
          bootstrap_multivalue_options_, default_paths);
    }
    return config_file_path;
  } else {
    keyring_info_.set_keyring_file(kDefaultKeyringFileName);
    keyring_info_.set_master_key_file("mysqlrouter.key");
    config_gen.set_keyring_info(keyring_info_);

    auto config_file_path =
        config_gen.config_file_path_for_directory(bootstrap_directory_);
    if (!is_legacy() && !config_gen.needs_bootstrap(config_file_path)) {
      print_skip_config(config_file_path);
      skipped_ = true;
    } else {
      config_gen.bootstrap_directory_deployment(
          program_name, bootstrap_directory_, bootstrap_options_,
          bootstrap_multivalue_options_, default_paths);
    }

    return config_file_path;
  }
}
