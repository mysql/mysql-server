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

#include "bootstrap_configurator.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

#include "bootstrap_mysql_account.h"
#include "config_builder.h"
#include "dim.h"
#include "harness_assert.h"
#include "keyring_handler.h"
#include "mrs/database/query_version.h"
#include "my_macros.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysqld_error.h"
#include "mysqlrouter/config_files.h"
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/server_compatibility.h"
#include "mysqlrouter/sys_user_operations.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"
#include "print_version.h"
#include "random_generator.h"
#include "router_config.h"
#include "socket_operations.h"

IMPORT_LOG_FUNCTIONS()
using namespace mysqlrouter;
using namespace mysql_harness::utility;
using namespace std::string_literals;

using mysql_harness::DIM;

constexpr const char kConfigMrsSection[] = "mysql_rest_service";

constexpr const char kProgramName[] = "mysqlrouter_bootstrap";
constexpr const char kRouterProgramName[] = "mysqlrouter";

static const size_t kHelpScreenWidth = 72;
static const size_t kHelpScreenIndent = 8;

// throws std::runtime_error
static mysql_harness::LoaderConfig *make_config(
    const std::map<std::string, std::string> params,
    const std::vector<std::string> &config_files,
    const CmdArgHandler::ConfigOverwrites &config_overrides) {
  using namespace mysql_harness::utility;
  constexpr const char *err_msg = "Configuration error: %s.";

  try {
    // LoaderConfig ctor throws bad_option (std::runtime_error)
    std::unique_ptr<mysql_harness::LoaderConfig> config(
        new mysql_harness::LoaderConfig(params, std::vector<std::string>(),
                                        mysql_harness::Config::allow_keys,
                                        config_overrides));

    // throws std::invalid_argument, std::runtime_error, syntax_error, ...
    for (const auto &config_file : config_files) {
      config->read(config_file);
    }

    return config.release();
  } catch (const mysql_harness::syntax_error &err) {
    throw std::runtime_error(string_format(err_msg, err.what()));
  } catch (const std::runtime_error &err) {
    throw std::runtime_error(string_format(err_msg, err.what()));
  }
}

static std::string generate_username(
    const std::string &prefix, uint32_t router_id,
    const mysql_harness::RandomGeneratorInterface &rg =
        mysql_harness::RandomGenerator()) {
  constexpr const unsigned kNumRandomChars = 12;
  constexpr const unsigned kMaxUsernameChars = 32;

  using RandomGen = mysql_harness::RandomGeneratorInterface;
  std::string username_prefix = prefix + std::to_string(router_id) + "_";

  return username_prefix +
         rg.generate_identifier(
             std::min<unsigned>(kMaxUsernameChars - username_prefix.length(),
                                kNumRandomChars),
             RandomGen::AlphabetDigits | RandomGen::AlphabetLowercase);
}

static std::string string_after(std::string s, char c) {
  return s.substr(s.find(c) + 1);
}

static std::string get_plugin_folder_overwrite(
    const CmdArgHandler::ConfigOverwrites &overwrites) {
  const auto default_key = std::make_pair("DEFAULT"s, ""s);
  if (overwrites.count(default_key) != 0) {
    const auto &default_overwrites = overwrites.at(default_key);
    if (default_overwrites.count("plugin_folder") != 0) {
      return default_overwrites.at("plugin_folder");
    }
  }

  return "";
}

static void check_config_overwrites(
    const CmdArgHandler::ConfigOverwrites &overwrites) {
  for (const auto &overwrite : overwrites) {
    const std::string &section = overwrite.first.first;
    const std::string &key = overwrite.first.second;
    if (section == "DEFAULT" && !key.empty()) {
      throw std::runtime_error("Invalid argument '--" + section + ":" + key +
                               "'. Key not allowed on DEFAULT section");
    }

    // only --logger.level config overwrite is allowed currently for bootstrap
    for (const auto &option : overwrite.second) {
      const std::string name = section + "." + option.first;
      if (name != "logger.level" && name != "DEFAULT.plugin_folder") {
        throw std::runtime_error(
            "Invalid argument '--" + name +
            "'. Only '--logger.level' configuration option can be "
            "set with a command line parameter when bootstrapping.");
      }
    }
  }
}

void BootstrapConfigurator::MySQLRouterAndMrsConf::check_and_add_conf(
    std::vector<std::string> &configs, const std::string &value) {
  mysql_harness::Path cfg_file_path;
  try {
    cfg_file_path = mysql_harness::Path(value);
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error(
        string_format("Failed reading configuration file: %s", exc.what()));
  }

  if (cfg_file_path.is_regular()) {
    configs.push_back(cfg_file_path.real_path().str());
  } else if (!cfg_file_path.exists()) {
    throw std::runtime_error(string_format(
        "The configuration file '%s' does not exist.", value.c_str()));
  } else {
    throw std::runtime_error(string_format(
        "The configuration file '%s' is expected to be a readable file, but it "
        "is %s.",
        value.c_str(), mysqlrouter::to_string(cfg_file_path.type()).c_str()));
  }
}

BootstrapConfigurator::BootstrapConfigurator(std::ostream &out_stream,
                                             std::ostream &err_stream)
    : bootstrapper_(is_legacy_, keyring_.get_ki(), out_stream, err_stream,
                    config_files_) {}

void BootstrapConfigurator::init_main_logger(
    mysql_harness::LoaderConfig &config, bool raw_mode /*= false*/) {
  if (!config.has_default("logging_folder"))
    config.set_default("logging_folder", "");

  const std::string logging_folder = config.get_default("logging_folder");

  // setup logging
  {
    // REMINDER: If something threw beyond this point, but before we managed to
    //           re-initialize the logger (registry), we would be in a world of
    //           pain: throwing with a non-functioning logger may cascade to a
    //           place where the error is logged and... BOOM!) So we deal with
    //           the above problem by working on a new logger registry object,
    //           and only if nothing throws, we replace the current registry
    //           with the new one at the very end.

    // our new logger registry, it will replace the current one if all goes well
    std::unique_ptr<mysql_harness::logging::Registry> registry(
        new mysql_harness::logging::Registry());

    const auto level = mysql_harness::logging::get_default_log_level(
        config, raw_mode);  // throws std::invalid_argument

    // register loggers for all modules + main exec (throws std::logic_error,
    // std::invalid_argument)
    mysql_harness::logging::create_module_loggers(
        *registry, level, {MYSQL_ROUTER_LOG_DOMAIN}, MYSQL_ROUTER_LOG_DOMAIN);

    // register logger for sql domain
    mysql_harness::logging::create_logger(*registry, level, "sql");

    // attach all loggers to main handler (throws std::runtime_error)
    mysql_harness::logging::create_main_log_handler(
        *registry, kProgramName, logging_folder, !raw_mode, false);

    // nothing threw - we're good. Now let's replace the new registry with the
    // old one
    DIM::instance().set_LoggingRegistry(
        [&registry]() { return registry.release(); },
        std::default_delete<mysql_harness::logging::Registry>());
    DIM::instance().reset_LoggingRegistry();

    // flag that the new loggers are ready for use
    DIM::instance().get_LoggingRegistry().set_ready();
  }

  // and give it a first spin
  if (config.logging_to_file())
    log_debug("Main logger initialized, logging to '%s'",
              config.get_log_file().c_str());
  else
    log_debug("Main logger initialized, logging to STDERR");
}

void BootstrapConfigurator::init(int argc, char **argv) {
  origin_ = mysql_harness::Path(mysqlrouter::find_full_executable_path(argv[0]))
                .dirname();

  router_program_name_ = origin_.join(kRouterProgramName).str();

  parse_command_options(std::vector<std::string>({argv + 1, argv + argc}));

  if (bootstrap_mrs_ && bootstrapper_.bootstrap_options().count("disable-rest"))
    throw std::runtime_error(
        "invalid configuration, --mrs cannot be used with --disable-rest");

  // default configuration for bootstrap is not supported
  // extra configuration for bootstrap is not supported
  auto config_files_res =
      ConfigFilePathValidator({}, config_files_, {}).validate();
  std::vector<std::string> config_files;
  if (config_files_res && !config_files_res.value().empty()) {
    config_files = std::move(config_files_res.value());
  }

  DIM::instance().reset_Config();  // simplifies unit tests
  const auto config_overwrites = arg_handler_.get_config_overwrites();
  DIM::instance().set_Config(
      [&config_files, &config_overwrites]() {
        return make_config({}, config_files, config_overwrites);
      },
      std::default_delete<mysql_harness::LoaderConfig>());
  auto &config = DIM::instance().get_Config();
  try {
    init_main_logger(config,
                     true);  // true = raw logging mode
  } catch (const std::runtime_error &) {
    // If log init fails, there's not much we can do here (no way to log the
    // error) except to catch this exception to prevent it from bubbling up
    // to std::terminate()
  }
}

void BootstrapConfigurator::run() {
  if (showing_info_) return;

  //  bootstrapper_.connect();

  // TODO add check for target here

  const auto config_overwrites = arg_handler_.get_config_overwrites();
  check_config_overwrites(config_overwrites);

  std::string config_path =
      bootstrapper_.bootstrap(router_program_name_, origin_, bootstrap_mrs_,
                              get_plugin_folder_overwrite(config_overwrites)
#ifndef _WIN32
                                  ,
                              mysqlrouter::SysUserOperations::instance()
#endif
      );

  load_configuration(config_path);
  if (bootstrapper_.skipped()) keyring_.init(config_, false);

  if (bootstrap_mrs_) {
    configure_mrs(bootstrapper_.session(), config_path);
  }
}

void BootstrapConfigurator::configure_mrs(mysqlrouter::MySQLSession *session,
                                          const std::string &config_path) {
  if (can_configure_mrs(config_path)) {
    // XXX move this check to before regular bootstap is done
    check_mrs_metadata(session);

    bool accounts_if_not_exists = mrs_metadata_account_.user.empty();

    const auto &opts = bootstrapper_.bootstrap_options();
    if (auto account_create = opts.find("account-create");
        account_create != opts.end()) {
      accounts_if_not_exists = account_create->second == "if-not-exists";
    }

    std::cout << Vt100::foreground(Vt100::Color::Yellow)
              << "# Configuring `MRS` plugin..."
              << Vt100::render(Vt100::Render::ForegroundDefault) << std::endl
              << std::endl;

    std::cout << "- Registering metadata\n";
    auto mrs_router_id = register_mrs_router_instance(session);

    std::cout << "- Creating account(s) "
              << (accounts_if_not_exists
                      ? "(only those that are needed, if any)"
                      : "")
              << "\n";
    create_mrs_users(session, mrs_router_id);

    std::cout << "- Storing account in keyring\n";
    store_mrs_data_in_keyring();

    std::cout << "- Adjusting configuration file " << config_path << "\n";
    store_mrs_configuration(config_path, mrs_router_id);

    std::cout << "\n"
              << "Once the MySQL Router is started, the MySQL REST Service can "
                 "be reached at\n    "
              << get_configured_rest_endpoint() << "\n";
  }
}

void BootstrapConfigurator::parse_command_options(
    std::vector<std::string> arguments) {
  if (arguments.empty()) {
    throw std::runtime_error(
        "Bootstrap requires at least one parameter with URI that points "
        "to the target MySQL Server.");
  }
  if (!arguments[0].empty() && arguments[0][0] != '-') {
    prepare_command_options(arguments[0]);
    arguments.erase(arguments.begin());
  } else {
    prepare_command_options("");
  }

  try {
    arg_handler_.process(arguments);
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error(exc.what());
  }
}

void BootstrapConfigurator::prepare_command_options(
    const std::string &bootstrap_uri) {
  using OptionNames = CmdOption::OptionNames;

  // General guidelines for naming command line options:
  //
  // Option names that start with --conf are meant to affect
  // configuration only and used during bootstrap.
  // If an option affects the bootstrap process itself, it should
  // omit the --conf prefix, even if it affects both the bootstrap
  // and the configuration.

  arg_handler_.clear_options();
  bootstrapper_.prepare_command_options(arg_handler_, bootstrap_uri);

  arg_handler_.add_option(
      OptionNames({"--mrs"}),
      "Enable MRS plugin. Write configurations and setup MySQL accounts for "
      "MRS.",
      CmdOptionValueReq::none, "",
      [this](const std::string &) { bootstrap_mrs_ = true; });
  arg_handler_.add_option(
      OptionNames({"--mrs-mysql-metadata-account"}),
      "MySQL account (username) to be used by MRS to access MRS metadata.",
      CmdOptionValueReq::required, "username",
      [this](const std::string &account) {
        mrs_metadata_account_.user = account;
      });
  arg_handler_.add_option(
      OptionNames({"--mrs-mysql-data-account"}),
      "MySQL account (username) to be used by MRS to access data to be served.",
      CmdOptionValueReq::required, "username",
      [this](const std::string &account) { mrs_data_account_.user = account; });
  arg_handler_.add_option(
      OptionNames({"--mrs-global-secret"}),
      "Common secret string to be used for JSON Web Token encryption. Must be "
      "common to all MRS Router instances.",
      CmdOptionValueReq::required, "global-secret",
      [this](const std::string &secret) { mrs_secret_ = secret; });

  arg_handler_.add_option(
      OptionNames({"-V", "--version"}), "Display version information and exit.",
      CmdOptionValueReq::none, "", [this](const std::string &) {
        std::cout << this->get_version_line() << std::endl;
        this->showing_info_ = true;
      });
  arg_handler_.add_option(
      OptionNames({"-?", "--help"}), "Display this help and exit.",
      CmdOptionValueReq::none, "", [this](const std::string &) {
        this->showing_info_ = true;
        this->show_help();
      });
}

std::string BootstrapConfigurator::get_version_line() noexcept {
  std::string version_string;
  build_version(kProgramName, &version_string);

  return version_string;
}

/**
 * filter CmdOption by section.
 *
 * makes options "required" if needed for the usage output
 */
static std::pair<bool, CmdOption> cmd_option_acceptor(
    const std::string &section, const std::set<std::string> &accepted_opts,
    const CmdOption &opt) {
  for (const auto &name : opt.names) {
    if (accepted_opts.find(name) != accepted_opts.end()) {
      if ((section == "help" && name == "--help") ||
          (section == "version" && name == "--version")) {
        CmdOption req_opt(opt);
        req_opt.required = true;
        return {true, req_opt};
      } else {
        return {true, opt};
      }
    }
  }

  return {false, opt};
}

void BootstrapConfigurator::show_usage() noexcept {
  std::cout << Vt100::render(Vt100::Render::Bold) << "# Usage"
            << Vt100::render(Vt100::Render::Normal) << "\n\n";

  std::vector<std::pair<std::string, std::set<std::string>>> usage_sections{
      {"help", {"--help"}},
      {"version", {"--version"}},
      {"bootstrap",
       {"--account-host",
        "--bootstrap",
        "--bootstrap-socket",
        "--conf-use-sockets",
        "--conf-set-option",
        "--conf-skip-tcp",
        "--conf-base-port",
        "--conf-use-gr-notifications",
        "--connect-timeout",
        "--client-ssl-cert",
        "--client-ssl-cipher",
        "--client-ssl-curves",
        "--client-ssl-key",
        "--client-ssl-mode",
        "--core-file",
        "--directory",
        "--force",
        "--force-password-validation",
        "--name",
        "--master-key-reader",
        "--master-key-writer",
        "--password-retries",
        "--read-timeout",
        "--report-host",
        "--server-ssl-ca",
        "--server-ssl-capath",
        "--server-ssl-cipher",
        "--server-ssl-crl",
        "--server-ssl-crlpath",
        "--server-ssl-curves",
        "--server-ssl-mode",
        "--server-ssl-verify",
        "--ssl-ca",
        "--ssl-cert",
        "--ssl-cipher",
        "--ssl-crl",
        "--ssl-crlpath",
        "--ssl-key",
        "--ssl-mode",
        "--tls-version",
        "--user"}},
      {"mrs",
       {"--mrs", "--mrs-mysql-metadata-account", "--mrs-mysql-data-account",
        "--mrs-global-secret"}}};

  for (const auto &section : usage_sections) {
    for (auto line : arg_handler_.usage_lines_if(
             kProgramName, "", kHelpScreenWidth,
             [&section](const CmdOption &opt) {
               return cmd_option_acceptor(section.first, section.second, opt);
             })) {
      std::cout << line << "\n";
    }
    std::cout << "\n";
  }

  std::cout << Vt100::render(Vt100::Render::Bold) << "# Options"
            << Vt100::render(Vt100::Render::Normal) << "\n\n";
  for (auto line :
       arg_handler_.option_descriptions(kHelpScreenWidth, kHelpScreenIndent)) {
    std::cout << line << std::endl;
  }
}

void BootstrapConfigurator::show_help() {
  std::cout << Vt100::render(Vt100::Render::Bold) << "# Usage"
            << Vt100::render(Vt100::Render::Normal) << "\n\n";

  std::cout << kProgramName << " <server URI> [options]\n";

  show_usage();

  std::cout << std::endl
            << Vt100::render(Vt100::Render::Bold) << "# Examples"
            << Vt100::render(Vt100::Render::Normal) << std::endl
            << std::endl;

  constexpr const char kStartWithSudo[]{IF_WIN("", "sudo ")};
  constexpr const char kStartWithUser[]{IF_WIN("", " --user=mysqlrouter")};

  std::cout << "Bootstrap for use with InnoDB cluster into system-wide "
               "installation\n\n"
            << "    " << kStartWithSudo << "mysqlrouter_bootstrap "
            << "root@clusterinstance01 " << kStartWithUser << "\n\n"
            << "Bootstrap for use with InnoDb cluster in a self-contained "
               "directory\n\n"
            << "    "
            << "mysqlrouter_bootstrap root@clusterinstance01 -d myrouter"
            << "\n\n";
}

void BootstrapConfigurator::load_configuration(
    const std::string &cnf_file_name) {
  config_.read(cnf_file_name);
}

void BootstrapConfigurator::create_mrs_users(mysqlrouter::MySQLSession *session,
                                             uint64_t mrs_router_id) {
  try {
    bool autogenerated_meta{false};

    bool is_change_password_ok_metadata = false;
    if (mrs_metadata_account_.user.empty()) {
      is_change_password_ok_metadata = true;
      mrs_metadata_account_.user =
          generate_username("mysql_router_mrs", mrs_router_id);
      autogenerated_meta = true;
    } else {
      mrs_metadata_account_.pass =
          prompt_password("Please enter MySQL password for MRS metadata-user:" +
                          mrs_metadata_account_.user);
    }

    bool create_data_user = false;
    if (mrs_data_account_.user != mrs_metadata_account_.user) {
      if (mrs_data_account_.user.empty()) {
      } else {
        create_data_user = true;
        mrs_data_account_.pass =
            prompt_password("Please enter MySQL password for MRS data-user:" +
                            mrs_data_account_.user);
      }
    }

    BootstrapMySQLAccount buser{session};

    UserOptions user_options;

    user_options.account_create = "if-not-exists";
    user_options.grant_role = {"mysql_rest_service_meta_provider"};

    if (!create_data_user) {
      user_options.grant_role.push_back("mysql_rest_service_data_provider");
    }

    std::string account_metadata;

    user_options.autogenerated = autogenerated_meta;
    buser.create_router_accounts(
        user_options, {"%"}, mrs_metadata_account_.user,
        mrs_metadata_account_.pass, is_change_password_ok_metadata);
    store_mrs_account_metadata(session, mrs_router_id, "mrs_metadata_accounts",
                               mrs_metadata_account_.user, {"%"});

    if (create_data_user) {
      user_options.grant_role = {"mysql_rest_service_data_provider"};
      buser.create_router_accounts(user_options, {"%"}, mrs_data_account_.user,
                                   mrs_data_account_.pass, false);

      store_mrs_account_metadata(session, mrs_router_id, "mrs_data_accounts",
                                 mrs_data_account_.user, {"%"});
    }

  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error("Unable to connect to the server: "s + e.what());
  }
}

void BootstrapConfigurator::store_mrs_data_in_keyring() {
  static const char *kKeyringAttributePassword = "password";
  mysql_harness::Keyring *keyring = mysql_harness::get_keyring();

  auto secret = mrs_secret_;
  int prompt_for_jwt = 0;
  while (secret.empty()) {
    if (prompt_for_jwt++) {
      std::cout << "\nJSON Web Token, can't be set to an empty string.\n";
    }
    std::cout << "\n";
    std::cout << "Please enter a secret string to be used as a JSON Web Token "
                 "(JWT) secret.\n"
                 "If this is the first MRS Router instance being deployed, you "
                 "may enter a new random string.\nFuture deployments targeting "
                 "the same MySQL server or InnoDB Cluster must use the same "
                 "secret.\n";

    secret = prompt_password("JWT secret");
  }

  keyring->store("rest-user", "jwt_secret", secret);

  keyring->store(mrs_metadata_account_.user, kKeyringAttributePassword,
                 mrs_metadata_account_.pass);

  if (mrs_metadata_account_.user != mrs_data_account_.user)
    keyring->store(mrs_data_account_.user, kKeyringAttributePassword,
                   mrs_data_account_.pass);

  try {
    mysql_harness::flush_keyring();
  } catch (const std::exception &e) {
    throw std::runtime_error(
        std::string("Error storing encrypted password to disk: ") + e.what());
  }
}

void BootstrapConfigurator::check_mrs_metadata(
    mysqlrouter::MySQLSession *session) const {
  try {
    mrs::database::MrsSchemaVersionChecker k_version_combatible_2{2, 2};
    mrs::database::MrsSchemaVersionChecker k_version_combatible_3{3};
    mrs::database::QueryVersion q;

    auto version = q.query_version(session);

    if (!k_version_combatible_3.is_compatible(version) &&
        !k_version_combatible_2.is_compatible(version)) {
      std::stringstream ss;
      ss << "Unsupported MRS metadata version (" << version.major << "."
         << version.minor << "." << version.patch << ")";
      throw std::runtime_error(ss.str());
    }
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    if (e.code() == ER_BAD_DB_ERROR) {
      std::cout << "MySQL REST Service metadata was not found at the target "
                   "MySQL server. Please deploy it before bootstrapping MRS.\n";
      throw std::runtime_error("MRS metadata not found");
    } else {
      std::cout << "MRS metadata query returned error: " << e.code() << " "
                << e.what() << "\n";
      throw std::runtime_error("Invalid MRS metadata");
    }
  }

  check_version_compatibility(session);
}

bool BootstrapConfigurator::can_configure_mrs(
    const std::string &config_path) const {
  if (config_.has_any(kConfigMrsSection)) {
    mysql_harness::Path path = config_path;
    std::cout << Vt100::foreground(Vt100::Color::Yellow)
              << "# Skipping MySQL REST Service bootstrap"
              << Vt100::render(Vt100::Render::ForegroundDefault)
              << "\n\nSkipping MySQL REST Service configuration, the '"
              << path.basename().str() << "' file already has the '"
              << kConfigMrsSection << "' section.\n\n";
    return false;
  }
  return true;
}

std::string BootstrapConfigurator::get_configured_router_name() const {
  auto section = config_.get_default_section();
  if (section.has("name")) {
    return section.get("name");
  }
  return "";
}

std::string BootstrapConfigurator::get_configured_rest_endpoint() const {
  auto sections = config_.get("http_server");
  for (auto s : sections) {
    if (s->has("port")) {
      std::string url;
      if (s->has("ssl") && s->get("ssl") == "1")
        url = "https://";
      else
        url = "http://";
      url += "localhost:" + s->get("port") + "/<service-name>";
      return url;
    }
  }
  return "";
}

void BootstrapConfigurator::store_mrs_configuration(
    const std::string &config_path, uint64_t mrs_router_id) {
  auto [rw_section, ro_section] = get_config_classic_sections();

  if (rw_section.key.empty()) {
    std::cout << "Disabling MySQL REST Service. There is no suitable 'routing' "
                 "section.\b";
    return;
  }

  std::map<std::string, std::string> kv;

  kv.insert_or_assign("router_id", std::to_string(mrs_router_id));

  kv.insert_or_assign("mysql_user", mrs_metadata_account_.user);
  if (mrs_metadata_account_.user != mrs_data_account_.user)
    kv.insert_or_assign("mysql_user_data_access", mrs_data_account_.user);
  kv.insert_or_assign("mysql_read_write_route",
                      string_after(rw_section.key, ':'));
  kv.insert_or_assign("mysql_read_only_route",
                      string_after(ro_section.key, ':'));

  mysql_harness::ConfigBuilder builder;
  mysql_harness::Path path = config_path;
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

uint64_t BootstrapConfigurator::register_mrs_router_instance(
    mysqlrouter::MySQLSession *session) {
  auto socket_ops = mysql_harness::SocketOperations::instance();

  std::string router_name = get_configured_router_name();
  std::string report_host = socket_ops->get_local_hostname();

  if (auto rh = bootstrapper_.bootstrap_options().find("report-host");
      rh != bootstrapper_.bootstrap_options().end())
    report_host = rh->second;

  session->execute(
      "INSERT INTO mysql_rest_service_metadata.router"
      " (router_name, address, product_name, version, attributes, options)"
      " VALUES (" +
      session->quote(router_name) + ", " + session->quote(report_host) + ", " +
      session->quote(MYSQL_ROUTER_PACKAGE_NAME) + ", " +
      session->quote(MYSQL_ROUTER_VERSION) +
      ", '{}', '{}') ON DUPLICATE KEY UPDATE version=" +
      session->quote(MYSQL_ROUTER_VERSION) + ", last_check_in=NOW()");

  return session->last_insert_id();
}

void BootstrapConfigurator::store_mrs_account_metadata(
    mysqlrouter::MySQLSession *session, uint64_t mrs_router_id,
    const std::string &key, const std::string &user,
    const std::vector<std::string> &hosts) {
  std::string sql = "UPDATE mysql_rest_service_metadata.router";
  sql += " SET attributes = JSON_MERGE_PRESERVE(attributes, JSON_OBJECT('" +
         key + "', JSON_ARRAY(";
  bool first = true;
  for (const auto &h : hosts) {
    sql += session->quote(user + "@" + h);
    if (!first) {
      sql += ", ";
      first = false;
    }
  }
  sql += ")))";
  sql += " WHERE id = " + std::to_string(mrs_router_id);

  session->execute(sql);
}
