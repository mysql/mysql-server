/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_MYSQL_ROUTER_INCLUDED
#define ROUTER_MYSQL_ROUTER_INCLUDED

#include "mysqlrouter/router_export.h"

/** @file
 * @brief Defining the main class MySQLRouter
 *
 * This file defines the main class `MySQLRouter`.
 *
 */

#include "mysql/harness/arg_handler.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/signal_handler.h"
#include "mysqlrouter/keyring_info.h"
#include "mysqlrouter/sys_user_operations.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

static const size_t kHelpScreenWidth = 72;
static const size_t kHelpScreenIndent = 8;

static const std::string kSystemRouterName = "system";
static const std::string kDefaultSystemUserName = "";

class ConfigFiles;

/** @class MySQLRouter
 *  @brief Manage the MySQL Router application.
 *
 *  The class MySQLRouter manages the MySQL Router application. It handles the
 *  command arguments, finds valid configuration files, and starts all plugins.
 *
 *  Since MySQL Router requires at least 1 configuration file to be available
 *  for reading, if no default configuration file location could be read and no
 *  explicit location was given, the application exits.
 *
 *  The class depends on MySQL Harness to, among other things, load the
 *  configuration and initialize all request plugins.
 *
 *  Example usage:
 *
 *     int main(int argc, char** argv) {
 *       MySQLRouter router(argc, argv);
 *       router.start();
 *     }
 *
 */
class MySQLRouter {
 public:
  /** @brief Default constructor
   *
   * Default constructor of MySQL Router which will not initialize.
   *
   * Usage:
   *
   *     MySQLRouter router;
   *     router.start();
   *
   */
  MySQLRouter()
      : can_start_(false),
        showing_info_(false),
        out_stream_(std::cout),
        err_stream_(std::cerr)
#ifndef _WIN32
        ,
        sys_user_operations_(mysqlrouter::SysUserOperations::instance())
#endif
  {
  }

  /** @brief Constructor with command line arguments as vector
   *
   * Constructor of MySQL Router which will start with the given command
   * line arguments as a vector of strings.
   *
   * Example usage:
   *
   *     MySQLRouter router(argv[0],
   *                        vector<string>({argv + 1, argv + argc}));
   *     router.start();
   *
   * @param program_name path of the started executable
   * @param arguments a vector of strings
   * @param out_stream output stream representing "stdout"
   * @param err_stream output stream representing "stderr"
   */
#ifndef _WIN32
  /// @param sys_user_operations system operations which provide chown, ...
#endif
  MySQLRouter(const std::string &program_name,
              const std::vector<std::string> &arguments,
              std::ostream &out_stream = std::cout,
              std::ostream &err_stream = std::cerr
#ifndef _WIN32
              ,
              mysqlrouter::SysUserOperationsBase *sys_user_operations =
                  mysqlrouter::SysUserOperations::instance()
#endif
  );

  /** @brief Constructor with command line arguments
   *
   * Constructor of MySQL Router which will start with the given command
   * line arguments given as the arguments argc and argv. Typically, argc
   * and argv are passed on from the global main function.
   *
   * Example usage:
   *
   *     int main(int argc, char** argv) {
   *       MySQLRouter router(argc, argv);
   *       router.start();
   *     }
   *
   * @param argc number of arguments
   * @param argv pointer to first command line argument
   * @param out_stream output stream representing "stdout"
   * @param err_stream output stream representing "stderr"
   */
#ifndef _WIN32
  /// @param sys_user_operations system operations which provide chown, ...
#endif
  MySQLRouter(const int argc, char **argv, std::ostream &out_stream,
              std::ostream &err_stream
#ifndef _WIN32
              ,
              mysqlrouter::SysUserOperationsBase *sys_user_operations =
                  mysqlrouter::SysUserOperations::instance()
#endif
  );

  virtual ~MySQLRouter() = default;

  /** @brief Initialize main logger
   *
   * Initializes main logger, according to options in the configuration.
   *
   * @param config Configuration to be used to initialize logger
   * @param raw_mode If true, all messages are logged raw; if false, messages
   *        are subject formatting
   * @param use_os_log If true, Windows EventLog will be used instead of STDERR;
   *        Currently this option is only meaningful on Windows
   *
   * @throws std::runtime_error on:
   * - failure to initialize file or OS logger
   * - bad configuration
   *
   * @note This function is static and public, because unlike
   * create_plugin_loggers(), it's also meant to be called very early during
   * startup, close to main().
   */
  static void init_main_logger(mysql_harness::LoaderConfig &config,
                               bool raw_mode = false, bool use_os_log = false);

  // Information member function
  std::string get_package_name() noexcept;

  /** @brief Returns the MySQL Router version as string
   *
   * Returns the MySQL Router as a string. The string is a concatenation
   * of version's major, minor and patch parts, for example `1.0.2`.
   *
   * @return string containing the version
   */
  std::string get_version() noexcept;

  /** @brief Returns string version details.
   *
   * Returns string with name and version details, including:
   *
   * * name of the application,
   * * version,
   * * platform and architecture,
   * * edition,
   * * and a special part-of-clause.
   *
   * The architecture is either 32-bit or 64-bit. Edition is usually used to
   * denote whether release is either GPLv2 license or commercial.
   *
   * The part-of clause is used to show which product family MySQL Router
   * belongs to.
   *
   * @internal
   * Most information can be defined while configuring using CMake and will
   * become available through the router_config.h file (generated from
   * router_config.h.in).
   * @endinternal
   *
   * @return a string containing version details
   */
  std::string get_version_line() noexcept;

  /** @brief Starts the MySQL Router application
   *
   * Starts the MySQL Router application, reading the configuration file(s) and
   * loading and starting all plugins.
   *
   * Example:
   *
   *     MySQLRouter router;
   *     router.start();
   *
   * Throws std::runtime_error on configuration or plugin errors.
   *
   * @internal
   * We are using MySQL Harness to load and start the plugins. We give Harness
   * a configuration files and it will parse it. Not that at this moment,
   * Harness only accept one configuration file.
   * @endinternal
   */
  void start();

  /** @brief Stop and cleanup the MySQL Router application
   *
   * Cleanup to perform when the MySQL Router application shuts down.
   *
   * Example:
   *
   *     MySQLRouter router;
   *     router.start();
   *     ...
   *     router.stop();
   *
   * Throws std::runtime_error on errors during cleanup.
   *
   * @internal
   * We ensure that the Harness pidfile is removed if present.
   * @endinternal
   */
  void stop();

  /** @brief Gets list of default configuration files
   *
   * Returns a list of configuration files which will be read (if available)
   * by default.
   *
   * @return std::vector<string>
   */
  const std::vector<std::string> &get_default_config_files() const noexcept {
    return default_config_files_;
  }

  /** @brief Gets list of configuration files passed using command line
   *
   * Returns a list of configuration files which were passed through command
   * line options.
   *
   * @return std::vector<string>
   */
  const std::vector<std::string> &get_config_files() const noexcept {
    return config_files_;
  }

  /** @brief Gets list of extra configuration files passed using command line
   *
   * Returns a list of extra configuration files which were passed through
   * command line options.
   *
   * @return std::vector<string>
   */
  const std::vector<std::string> &get_extra_config_files() const noexcept {
    return extra_config_files_;
  }

#if !defined(_MSC_VER) && !defined(UNIT_TESTS)
  // MSVC produces different symbols for private vs public methods, which mean
  // the #define private public trick for unit-testing private methods doesn't
  // work. Thus, we turn private methods public in Windows.
 private:
#endif

  /** @brief Initializes the MySQL Router application
   *
   * Initialized the MySQL Router application by
   *
   * * setting the default configuration files,
   * * loading the command line options,
   * * processing the given command line arguments,
   * * and finding all the usable configuration files.
   *
   * The command line options are passed using the `arguments`
   * argument. It should be a vector of strings. For example, to start
   * MySQL Router using the `main()` functions `argc` and `argv`
   * arguments:
   *
   *     MySQLRouter router(vector<string>({argv + 1, argv + argc}));
   *     router.start();
   *
   * @internal
   * We do not need the first command line argument, argv[0] since we do not
   * use it.
   * @endinternal
   *
   * @param program_name path to the executable.
   * @param arguments command line arguments as vector of strings
   */
  virtual void init(const std::string &program_name,
                    const std::vector<std::string> &arguments);

  /** @brief Prepares a command line option
   *
   * Prepares command line options for the MySQL Router `mysqlrouter`
   * application.
   *
   * @internal
   * Currently, to add options to the command line, you need to add it to the
   * `prepare_command_options`-method using `CmdArgHandler::add_option()`.
   * @endinternal
   */
  void prepare_command_options() noexcept;

  /** @brief Process command line options
   *
   * Processes command line options for the MySQL Router `mysqlrouter`
   * application.
   */
  void parse_command_options(
      const std::vector<std::string> &arguments);  // throws std::runtime_error

  /** @brief Finds all valid configuration files
   *
   * Finds all valid configuration files from the list of default
   * configuration file locations.
   *
   * An exception of type `std::runtime_error` is thrown when no valid
   * configuration file was found.
   *
   * @return returns a list of valid configuration file locations
   *
   */
  std::vector<std::string> check_config_files();

  /** @brief Shows the help screen on the console
   *
   * Shows the help screen on the console including
   *
   * * copyright and  trademark notice,
   * * command line usage,
   * * default configuration file locations,
   * * and options with their descriptions.
   *
   * Users would use the command line option `--help`:
   *
   *     shell> mysqlrouter --help
   */
  void show_help();

  /** @brief Saves the selected command line option in the internal options
   * array after verifying it's value not empty and the router is doing
   * bootstrap.
   *
   *  Throws: std::runtime_error
   */
  void save_bootstrap_option_not_empty(const std::string &option_name,
                                       const std::string &save_name,
                                       const std::string &option_value);

  /**
   * @brief verify that bootstrap option (--bootstrap or -B) was given by user.
   *
   * @throw std::runtime_error if called in non-bootstrap mode.
   */
  void assert_bootstrap_mode(const std::string &option_name) const;

  /**
   * @brief verify that option given by user is not used with bootstrap option
   * (--bootstrap or -B).
   *
   * @throw std::runtime_error if called in bootstrap mode.
   */
  void assert_not_bootstrap_mode(const std::string &option_name) const;

  /**
   * @brief verify that option given by user is an integer value in the given
   * range.
   *
   * @throw std::out_of_range - option not in [min, max] range
   * @throw std::invalid_argument - not a valid integer
   */
  void assert_option_value_in_range(const std::string &option_value,
                                    const int min, const int max) const;

  /** @brief Shows command line usage and option description
   *
   * Shows command line usage and all available options together with their
   * description. It is possible to prevent the option listing by setting the
   * argument `include_options` to `false`.
   *
   * @internal
   * This method is used by the `MySQLRouter::show_help()`. We keep a separate
   * method so we could potentionally show the usage in other places or using
   * different option combinations, for example after an error.
   * @endinternal
   *
   * @param include_options bool whether we show the options and descriptions
   */
  void show_usage(bool include_options) noexcept;

  /* @overload */
  void show_usage() noexcept;

  /** @brief Sets default configuration file locations
   *
   *  Sets the default configuration file locations based on information
   *  found in the locations argument. The locations should be provided
   *  as a semicolon separated list.
   *
   *  The previous loaded locations are first removed. If not new locations
   *  were provider (if locations argument is empty), then no configuration
   *  files will be available.
   *
   *  Locations can include environment variable placeholders. These
   * placeholders are replaced using the provided name. For example, user Jane
   * executing MySQL Router:
   *
   *      /opt/ENV{USER}/etc    becomes   /opt/jane/etc
   *
   *  If the environment variable is not available, for example if
   * MYSQL_ROUTER_HOME was not set before starting MySQL Router, every location
   * using this environment variable will be ignored.
   *
   *  @param locations a char* with semicolon separated file locations
   */
  void set_default_config_files(const char *locations) noexcept;

  void bootstrap(const std::string &program_name,
                 const std::string &metadata_server_uri,
                 const std::string &plugin_folder = "");

  /*
   * @brief returns id of the router.
   *
   * @throw bad_section
   */
  uint32_t get_router_id(mysql_harness::Config &config);

  void init_keyring(mysql_harness::Config &config);

  void init_dynamic_state(mysql_harness::Config &config);

  /**
   * @brief Initializes keyring using master-key-reader and master-key-writer.
   *
   * @throw MasterKeyReadError
   */
  void init_keyring_using_external_facility(mysql_harness::Config &config);

  /**
   * @brief Initializes keyring using master key file.
   *
   * @throw std::runtime_error
   */
  void init_keyring_using_master_key_file();

  /**
   * @brief Initializes keyring using password read from STDIN.
   *
   * @throw std::runtime_error
   */
  void init_keyring_using_prompted_password();

  // throws std::runtime_error
  void init_loader(mysql_harness::LoaderConfig &config);

  // throws std::runtime_error
  mysql_harness::LoaderConfig *make_config(
      const std::map<std::string, std::string> params,
      const std::vector<std::string> &config_files);

  std::map<std::string, std::string> get_default_paths() const;

  /** @brief Tuple describing the MySQL Router version, with major, minor and
   * patch level **/
  std::tuple<const uint8_t, const uint8_t, const uint8_t> version_;

  /** @brief Vector with default configuration file locations as strings **/
  std::vector<std::string> default_config_files_;
  /** @brief Vector with extra configuration file locations as strings **/
  std::vector<std::string> extra_config_files_;
  /** @brief Vector with configuration files passed through command line
   * arguments **/
  std::vector<std::string> config_files_;
  /** @brief PID file location **/
  std::string pid_file_path_;
  bool pid_file_created_{false};

  /** @brief CmdArgHandler object handling command line arguments **/
  CmdArgHandler arg_handler_;
  /** @brief Harness loader **/
  std::unique_ptr<mysql_harness::Loader> loader_;
  /** @brief Whether the MySQLRouter can start or not **/
  bool can_start_;
  /** @brief Whether we are showing information on command line, for example,
   * using --help or --version **/
  bool showing_info_;
  /**
   * @brief Value of the argument passed to the -B or --bootstrap
   *        command line option for bootstrapping.
   */
  std::string bootstrap_uri_;
  /**
   * @brief Valueof the argument passed to the --directory command line option
   */
  std::string bootstrap_directory_;
  /**
   * @brief key/value map of additional configuration options for bootstrap
   */
  std::map<std::string, std::string> bootstrap_options_;

  /**
   * @brief key/list-of-values map of additional configuration options for
   * bootstrap
   */
  std::map<std::string, std::vector<std::string>> bootstrap_multivalue_options_;

  /**
   * Path to origin of executable.
   *
   * This variable contain the directory that the executable is
   * running from.
   */
  mysql_harness::Path origin_;

  KeyringInfo keyring_info_;

  std::ostream &out_stream_;
  std::ostream &err_stream_;

  bool core_file_{false};

#ifndef _WIN32
  /** @brief Value of the --user parameter given on the command line if router
   * is launched in bootstrap mode **/
  std::string user_cmd_line_;

  /** @brief Value of the --user parameter given on the command line. It is used
   *to buffer the value of --user parameter till all command line parameters are
   *parsed. If router is launched in bootstrap mode, then username_ is copied to
   *user_cmd_line_, otherwise it is copied to bootstrap_options_["user"]
   **/
  std::string username_;

  /** @brief Pointer to the object to be used to perform system specific
   * user-related operations **/
  mysqlrouter::SysUserOperationsBase *sys_user_operations_;
#endif

  mysql_harness::SignalHandler signal_handler_;

#ifdef FRIEND_TEST
  FRIEND_TEST(Bug24909259, PasswordPrompt_plain);
  FRIEND_TEST(Bug24909259, PasswordPrompt_keyed);
#endif
};

class silent_exception : public std::exception {
 public:
  silent_exception() : std::exception() {}
};

/** @brief Expose the configured application-level options (the ones
 * from [DEFAULT] section that are not specific to any particular plugin and are
 * relevant from the Cluster metadata perspective) **/
void expose_router_configuration(const bool initial,
                                 const mysql_harness::ConfigSection &section);

#endif  // ROUTER_MYSQL_ROUTER_INCLUDED
