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

#include "bootstrap_configurator.h"

#include <fstream>
#include <iostream>

#include "bootstrap_arguments.h"
#include "bootstrap_mysql_account.h"
#include "mysqld_error.h"
#include "router_config.h"

#include "config_builder.h"
#include "keyring/keyring_manager.h"
#include "mysql/harness/filesystem.h"
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"
#include "random_generator.h"
#include "socket_operations.h"

using namespace mysqlrouter;
using namespace std::string_literals;

const std::string kConfigMrsSection = "rest_mrs";

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
    : arguments_{arguments} {}

void BootstrapConfigurator::load_configuration() {
  auto cnf_file_name = get_generated_configuration_file();
  config_.read(cnf_file_name);

  ki_handler_.init(config_, false);
}

void BootstrapConfigurator::connect(std::string *out_password) {
  // parse ssl_mode option (already validated in cmdline option handling)
  auto ssl_enum = MySQLSession::parse_ssl_mode(
      arguments_->ssl_mode);  // throws std::logic_error

  auto u =
      parse_server_uri(arguments_->bootstrap_uri, arguments_->bootstrap_socket);

  *out_password = u.password;

  session_ = std::make_unique<mysqlrouter::MySQLSession>();

  // set ssl mode + server authentication options
  session_->set_ssl_options(
      ssl_enum, arguments_->tls_version, arguments_->ssl_cipher,
      arguments_->ssl_ca, arguments_->ssl_capath, arguments_->ssl_crl,
      arguments_
          ->ssl_crlpath);  // throws Error, std::invalid_argument(logic_error)

  // set client authentication options
  if (!arguments_->ssl_cert.empty() || !arguments_->ssl_key.empty()) {
    session_->set_ssl_cert(arguments_->ssl_cert,
                           arguments_->ssl_key);  // throws Error(runtime_error)
  }

  session_->connect(u.host, u.port, u.username, u.password,
                    arguments_->bootstrap_socket, "",
                    arguments_->connect_timeout, arguments_->read_timeout);
}

void BootstrapConfigurator::configure_mrs(bool accounts_if_not_exists) {
  std::cout << Vt100::foreground(Vt100::Color::Yellow)
            << "# Configuring `MRS` plugin..."
            << Vt100::render(Vt100::Render::ForegroundDefault) << std::endl;

  std::cout << "- Creating account(s) "
            << (accounts_if_not_exists ? "(only those that are needed, if any)"
                                       : "")
            << "\n";
  create_mrs_users();

  std::cout << "- Storing account in keyring\n";
  store_mrs_data_in_keyring();

  std::cout << "- Adjusting configuration file "
            << get_generated_configuration_file() << "\n";
  store_mrs_configuration();

  std::cout << "- Registering metadata\n";
  register_mrs_router_instance();
}

void BootstrapConfigurator::create_mrs_users() {
  try {
    bool autogenerated_meta{false};
    bool autogenerated_data{false};

    bool is_change_password_ok_metadata = false;
    if (arguments_->mrs_metadata_account.user.empty()) {
      is_change_password_ok_metadata = true;
      arguments_->mrs_metadata_account.user =
          generate_username("mrs", get_config_router_id());
      autogenerated_meta = true;
    } else {
      arguments_->mrs_metadata_account.pass =
          prompt_password("Please enter MySQL password for MRS metadata-user:" +
                          arguments_->mrs_metadata_account.user);
    }

    bool is_change_password_ok_data = false;
    bool create_data_user = false;
    if (arguments_->mrs_data_account.user !=
        arguments_->mrs_metadata_account.user) {
      create_data_user = true;
      if (arguments_->mrs_data_account.user.empty()) {
        do {
          is_change_password_ok_data = true;
          arguments_->mrs_data_account.user =
              generate_username("mrs", get_config_router_id());
          autogenerated_data = true;
        } while (arguments_->mrs_metadata_account.user ==
                 arguments_->mrs_data_account.user);
      } else {
        arguments_->mrs_data_account.pass =
            prompt_password("Please enter MySQL password for MRS data-user:" +
                            arguments_->mrs_data_account.user);
      }
    }

    BootstrapMySQLAccount buser{session_.get()};

    arguments_->user_options.grant_role = {"mrs_provider_metadata"};

    if (!create_data_user) {
      arguments_->user_options.grant_role.push_back("mrs_provider_data_access");
    }

    arguments_->user_options.autogenerated = autogenerated_meta;
    buser.create_router_accounts(
        arguments_->user_options, get_account_host_args(),
        arguments_->mrs_metadata_account.user,
        arguments_->mrs_metadata_account.pass, is_change_password_ok_data);

    if (create_data_user) {
      arguments_->user_options.autogenerated = autogenerated_data;
      arguments_->user_options.grant_role = {"mrs_provider_data_access"};
      buser.create_router_accounts(
          arguments_->user_options, get_account_host_args(),
          arguments_->mrs_data_account.user, arguments_->mrs_data_account.pass,
          is_change_password_ok_metadata);
    }

  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error("Unable to connect to the server: "s + e.what());
  }
}

void BootstrapConfigurator::store_mrs_data_in_keyring() {
  static const char *kKeyringAttributePassword = "password";
  mysql_harness::Keyring *keyring = mysql_harness::get_keyring();

  auto secret = arguments_->mrs_secret;
  if (secret.empty()) {
    std::cout << "Please enter a secret string to be used as a JWT secret. If "
                 "this is the first MRS Router instance being deployed, you "
                 "may enter a new random string. Future deployments targeting "
                 "the same MySQL server or InnoDB Cluster must use the same "
                 "secret.\n";

    secret = prompt_password("JWT secret");
  }

  if (!secret.empty()) {
    keyring->store("rest-user", "jwt_secret", secret);
  }

  keyring->store(arguments_->mrs_metadata_account.user,
                 kKeyringAttributePassword,
                 arguments_->mrs_metadata_account.pass);

  if (arguments_->mrs_metadata_account.user !=
      arguments_->mrs_data_account.user)
    keyring->store(arguments_->mrs_data_account.user, kKeyringAttributePassword,
                   arguments_->mrs_data_account.pass);

  try {
    mysql_harness::flush_keyring();
  } catch (const std::exception &e) {
    throw std::runtime_error(
        std::string("Error storing encrypted password to disk: ") + e.what());
  }
}

bool BootstrapConfigurator::has_innodb_cluster_metadata() const {
  try {
    auto row = session_->query_one(
        "SELECT major, minor, patch FROM "
        "mysql_innodb_cluster_metadata.schema_version");
    return true;
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    if (e.code() == ER_BAD_DB_ERROR) {
      return false;
    } else {
      std::cout << "InnoDB Cluster metadata query returned error: " << e.code()
                << " " << e.what() << "\n";
      throw std::runtime_error("Invalid InnoDB Cluster metadata");
    }
  }
}

void BootstrapConfigurator::check_mrs_metadata() const {
  try {
    auto row = session_->query_one(
        "SELECT major, minor, patch FROM "
        "mysql_rest_service_metadata.schema_version");

    int major = std::stoi((*row)[0]);
    int minor = std::stoi((*row)[1]);
    int patch = std::stoi((*row)[2]);
    if (major != 1 || (minor == 0 && patch < 31)) {
      std::stringstream ss;
      ss << "Unsupported MRS metadata version (" << major << "." << minor << "."
         << patch << ")";
      throw std::runtime_error(ss.str());
    }
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    if (e.code() == ER_BAD_DB_ERROR) {
      throw std::runtime_error("MRS metadata not found");
    } else {
      std::cout << "MRS metadata query returned error: " << e.code() << " "
                << e.what() << "\n";
      throw std::runtime_error("Invalid MRS metadata");
    }
  }
}

bool BootstrapConfigurator::needs_configure_routing() const {
  bool exists;
  get_generated_configuration_file(&exists);

  if (exists) {
    std::cout << "Skipping Core MySQL Router configuration, the "
                 "'mysqlrouter.conf' file already exists.\n";
  }

  return !exists;
}

bool BootstrapConfigurator::can_configure_mrs() const {
  if (config_.has_any(kConfigMrsSection)) {
    mysql_harness::Path path = get_generated_configuration_file();

    std::cout << "Skipping MySQL REST Service configuration, the '"
              << path.basename().str()
              << "' file already has the 'rest_mrs' section.\n";
    return false;
  }
  return true;
}

void BootstrapConfigurator::store_mrs_configuration() {
  auto [rw_section, ro_section] = get_config_classic_sections();

  if (rw_section.key.empty()) {
    std::cout << "Disabling MySQL REST Service. There is no suitable 'routing' "
                 "section.\b";
    return;
  }

  std::map<std::string, std::string> kv;

  kv.insert_or_assign("mysql_user", arguments_->mrs_metadata_account.user);
  if (arguments_->mrs_metadata_account.user !=
      arguments_->mrs_data_account.user)
    kv.insert_or_assign("mysql_user_data_access",
                        arguments_->mrs_data_account.user);
  kv.insert_or_assign("mysql_read_write_route", rw_section.key);
  kv.insert_or_assign("mysql_read_only_route", ro_section.key);

  mysql_harness::ConfigBuilder builder;
  mysql_harness::Path path = get_generated_configuration_file();
  std::ofstream os;
  os.exceptions(std::ofstream::failbit | std::ofstream::badbit);
  os.open(path.c_str(), std::ofstream::out | std::ofstream::app);
  os << builder.build_section(kConfigMrsSection, kv);
  os.close();
}

static bool ends_with(const std::string &value, const std::string &&sst) {
  if (sst.empty()) return false;

  auto pos = value.rfind(sst);
  if (value.npos == pos) return false;
  return value.length() - pos == sst.length();
}

static bool starts_with(const std::string &value, const std::string &&sst) {
  if (sst.empty()) return false;

  auto pos = value.find(sst);
  if (value.npos == pos) return false;
  return pos == 0;
}

std::pair<BootstrapConfigurator::RoutingConfig,
          BootstrapConfigurator::RoutingConfig>
BootstrapConfigurator::get_config_classic_sections() {
  std::string kOptionProtocol = "protocol";
  std::string kOptionRouting = "routing";
  auto routing_sections = config_.get(kOptionRouting);
  mysql_harness::ConfigSection *rw{nullptr};
  mysql_harness::ConfigSection *ro{nullptr};

  for (auto &section : routing_sections) {
    if (!section->has(kOptionProtocol)) continue;

    if (section->get(kOptionProtocol) != "classic") continue;

    if (ends_with(section->get_section_name(), "_rw")) {
      rw = section;
    }
    if (ends_with(section->get_section_name(), "_ro")) {
      ro = section;
    }
  }

  if (rw) {
    bool is_metadata_cache = false;
    if (rw->has("destinations")) {
      is_metadata_cache =
          starts_with(rw->get("destinations"), "metadata-cache:");
    }
    return {{rw->get_section_name(), is_metadata_cache},
            {ro ? ro->get_section_name() : "", false}};
  }
  return {};
}

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

String BootstrapConfigurator::get_generated_configuration_file(
    bool *file_exists) const {
  auto default_paths = mysqlrouter::get_default_paths(
      arguments_->path_this_application_.dirname());
  auto directory = arguments_->bootstrap_directory;
  if (directory.empty()) {
    directory = default_paths.at("config_folder"s);
  }
  mysql_harness::Path config_file_path(directory);

  if (!config_file_path.exists()) {
    if (!file_exists)
      throw std::runtime_error("Deployment directory doesn't exist");
  } else {
    config_file_path = config_file_path.real_path();
  }
  config_file_path =
      config_file_path.join(mysql_harness::Path("mysqlrouter.conf"));

  if (!config_file_path.exists()) {
    if (file_exists)
      *file_exists = false;
    else
      throw std::runtime_error(
          "Configuration file is missing in deployment directory");
  } else {
    if (file_exists) *file_exists = true;
  }

  return config_file_path.str();
}

UniqueStrings BootstrapConfigurator::get_account_host_args() {
  auto result = arguments_->bootstrap_account_hosts;

  if (result.empty()) result.insert("%");

  return result;
}

void BootstrapConfigurator::register_mrs_router_instance() {
  auto socket_ops = mysql_harness::SocketOperations::instance();

  // TODO handle --report-host
  session_->execute(
      "INSERT INTO mysql_rest_service_metadata.router"
      " (id, router_name, address, product_name, version, attributes, options)"
      " VALUES ((SELECT coalesce(max(id),0)+1 from "
      "mysql_rest_service_metadata.router)," +
      session_->quote(arguments_->router_name) + ", " +
      session_->quote(socket_ops->get_local_hostname()) + ", " +
      session_->quote(MYSQL_ROUTER_PACKAGE_NAME) + ", " +
      session_->quote(MYSQL_ROUTER_VERSION) + ", '{}', '{}')");
}