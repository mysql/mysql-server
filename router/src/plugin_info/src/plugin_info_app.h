/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_PLUGIN_INFO_APP_INCLUDED
#define MYSQLROUTER_PLUGIN_INFO_APP_INCLUDED

#include <ostream>

#include "mysql/harness/arg_handler.h"

// name displayed with --version request
const std::string kPluginInfoAppName = "MySQL Router - Plugin Info App";

/**
 * exception thrown by the frontend.
 *
 * Should be presented to the user.
 */
class FrontendError : public std::runtime_error {
 public:
  FrontendError(const std::string &what) : std::runtime_error(what) {}
};

/**
 * frontend error that involved the command-line options.
 *
 * should bet handled by showing the user the help-text or a high how to get the
 * help
 */
class UsageError : public FrontendError {
 public:
  UsageError(const std::string &what) : FrontendError(what) {}
};

/** @class Plugin_info
 *
 * @brief Application class, enables testing of the application through the
 *        selecting input parameters and regular and error outputs.
 *
 **/
class PluginInfoFrontend final {
 public:
  enum class Cmd { INFO, SHOW_HELP, SHOW_VERSION };
  struct Config {
    Cmd cmd{Cmd::INFO};
    std::string filename;
    std::string username;
  };

  /** Constructor.
   *
   * @brief Normal application operation exepcts 3 parameters:
   *        {exe_name} {path_to_plugin_file} {plugin_name}
   *        This retrieves the data read from the plugin file to the output
   *stream.
   *
   *        Other supported options are:
   *        {exe_name} --help     outputs application usage to the error stream
   *        {exe_name} --version  outputs application version to the error
   *stream
   *
   * @param exe_name  name of the started executable
   * @param arguments command line arguments (without exe_name)
   * @param out       Output stream for the data printed by the application
   *
   **/
  PluginInfoFrontend(const std::string &exe_name,
                     const std::vector<std::string> &arguments,
                     std::ostream &out);

  std::string get_version() const noexcept;

  // should be const, but arg_handler's
  std::string get_help(const size_t screen_width = 80) const;

  /**
   * Executes the action requested from the application with the
   * parameters passed to the constructor.
   *
   * Note: Redirects the output to the streams provided to the constructor.
   *
   * @returns exit-code
   * @retval EXIT_SUCESS on success
   * @retval EXIT_FAILURE if an error occurred.
   */
  int run();

 private:
  void prepare_command_options();

  std::string program_name_;
  CmdArgHandler arg_handler_{true};
  std::ostream &cout_;

  Config config_;  // must be last as config-handling may depend on cin, ...
                   // and arg_handler_
};

#endif
