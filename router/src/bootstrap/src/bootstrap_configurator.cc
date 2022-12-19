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

#include "bootstrap_configurator.h"

#include "bootstrap_arguments.h"

#include "keyring/keyring_manager.h"
#include "mysql/harness/filesystem.h"
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"
#include "random_generator.h"

using namespace mysqlrouter;
using namespace std::string_literals;

static std::string generate_username(
    const std::string &prefix, uint32_t router_id,
    const mysql_harness::RandomGeneratorInterface &rg =
        mysql_harness::RandomGenerator()) {
  const unsigned kNumRandomChars = 12;

  using RandomGen = mysql_harness::RandomGeneratorInterface;
  return prefix + std::to_string(router_id) + "_" +
         rg.generate_identifier(
             kNumRandomChars,
             RandomGen::AlphabetDigits | RandomGen::AlphabetLowercase);
}

URI parse_server_uri(const std::string &server_uri,
                     const std::string &bootstrap_socket) {
  // Extract connection information from the bootstrap server URI.
  std::string uri;
  const std::string default_schema = "mysql://";
  if (server_uri.compare(0, default_schema.size(), default_schema) != 0) {
    uri = default_schema + server_uri;
  } else {
    uri = server_uri;
  }

  URI u;
  try {
    // don't allow rootless URIs (mailto:foo@...) which would collide with the
    // schema-less URIs are allow too: root:pw@host
    u = URIParser::parse(uri, false);
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
    u.password =
        prompt_password("Please enter MySQL password for " + u.username);
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

BootstrapConfigurator::BootstrapConfigurator(BootstrapArguments *arguments)
    : arguments_{arguments} {
  load_configuration();

  mysql_harness::Path keyring = get_config_keyring_path();
  mysql_harness::Path master_key = get_config_master_key_path();
  if (!keyring.exists() || !keyring.is_regular())
    throw std::runtime_error("Keyring file specified as '"s + keyring.str() +
                             "' is missing.");
  if (!master_key.exists() || !master_key.is_regular())
    throw std::runtime_error("Master key file specified as '"s +
                             master_key.str() + "' is missing.");

  keyring_info_.set_master_key_file(master_key.str());
  keyring_info_.set_keyring_file(keyring.str());
}

void BootstrapConfigurator::load_configuration() {
  auto cnf_file_name = get_generated_configuration_file();
  config_.read(cnf_file_name);
}

void BootstrapConfigurator::create_mrs_users() {
  try {
    auto u = parse_server_uri(arguments_->bootstrap_uri,
                              arguments_->bootstrap_socket);

    // parse ssl_mode option (already validated in cmdline option handling)
    auto ssl_enum = MySQLSession::parse_ssl_mode(
        arguments_->ssl_mode);  // throws std::logic_error

    auto sess = std::make_unique<mysqlrouter::MySQLSession>();

    // set ssl mode + server authentication options
    sess->set_ssl_options(
        ssl_enum, arguments_->tls_version, arguments_->ssl_cipher,
        arguments_->ssl_ca, arguments_->ssl_capath, arguments_->ssl_crl,
        arguments_
            ->ssl_crlpath);  // throws Error, std::invalid_argument(logic_error)

    // set client authentication options
    if (!arguments_->ssl_cert.empty() || !arguments_->ssl_key.empty()) {
      sess->set_ssl_cert(arguments_->ssl_cert,
                         arguments_->ssl_key);  // throws Error(runtime_error)
    }

    sess->connect(u.host, u.port, u.username, u.password,
                  arguments_->bootstrap_socket, "", arguments_->connect_timeout,
                  arguments_->read_timeout);

    auto &metadata_account = arguments_->mrs_metadata_account;
    if (metadata_account.user.empty()) {
      metadata_account.user = generate_username("mrs", get_config_router_id());
    } else {
      metadata_account.pass =
          prompt_password("Please enter MySQL password for MRS metadata-user:" +
                          metadata_account.user);
    }

    auto &data_account = arguments_->mrs_data_account;
    if (data_account.user.empty()) {
      do {
        data_account.user = generate_username("mrs", get_config_router_id());
      } while (metadata_account.user == data_account.user);
    } else {
      data_account.pass = prompt_password(
          "Please enter MySQL password for MRS data-user:" + data_account.user);
    }

  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error("Unable to connect to the server: "s + e.what());
  }
}

void BootstrapConfigurator::store_mrs_data_in_keyring() {}

void BootstrapConfigurator::store_configuration() {}

BootstrapCredentials BootstrapConfigurator::get_config_mrs_metadata_user() {
  auto sections = config_.get("rest_mrs");
  for (auto s : sections) {
    if (s->has("mysql_user")) return s->get("mysql_user");
  }

  return {};
}

BootstrapCredentials BootstrapConfigurator::get_config_mrs_data_user() {
  auto sections = config_.get("rest_mrs");
  for (auto s : sections) {
    if (s->has("mysql_user")) return s->get("mysql_user");
  }

  return {};
}

uint64_t BootstrapConfigurator::get_config_router_id() {
  const static std::string kRouterId = "router_id";
  auto sections = config_.get("metadata_cache");
  char *end_ptr;

  for (auto s : sections) {
    if (s->has(kRouterId))
      return strtoull(s->get(kRouterId).c_str(), &end_ptr, 0);
  }
  return 0;
}

String BootstrapConfigurator::get_generated_configuration_file() const {
  auto default_paths = mysqlrouter::get_default_paths(
      arguments_->path_this_application_.dirname());
  auto directory = arguments_->bootstrap_directory;
  if (directory.empty()) {
    directory = default_paths.at("config_folder"s);
  }
  mysql_harness::Path config_file_path(directory);

  if (!config_file_path.exists()) {
    throw std::runtime_error("Deployment directory doesn't exist");
  }
  config_file_path = config_file_path.real_path();
  config_file_path =
      config_file_path.join(mysql_harness::Path("mysqlrouter.conf"));

  if (!config_file_path.exists()) {
    throw std::runtime_error(
        "Configuration file is missing in deployment directory");
  }

  return config_file_path.str();
}

Strings BootstrapConfigurator::get_account_host_args() {
  auto result = arguments_->bootstrap_account_hosts;

  if (result.empty()) result.emplace_back("%");

  return result;
}

std::string BootstrapConfigurator::get_config_keyring_path() {
  const static std::string kKeyringPath = "keyring_path";
  auto &section = config_.get_default_section();
  if (section.has(kKeyringPath)) return {};

  return section.get(kKeyringPath);
}

std::string BootstrapConfigurator::get_config_master_key_path() {
  const static std::string kMasterKeyPath = "master_key_path";
  auto &section = config_.get_default_section();
  if (section.has(kMasterKeyPath)) return {};

  return section.get(kMasterKeyPath);
}
