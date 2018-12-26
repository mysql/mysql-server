/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLROUTER_PLUGIN_INFO_APP_INCLUDED
#define MYSQLROUTER_PLUGIN_INFO_APP_INCLUDED

#include <ostream>

// name displayed with --version request
const std::string kPluginInfoAppName = "MySQL Router - Plugin Info App";

/** @class Plugin_info
 *
 * @brief Application class, enables testing of the application through the
 *        selecting input parameters and regular and error outputs.
 *
 **/
class Plugin_info_app final {
 public:
  /** Constructor.
   *
   * @brief Normal application operation exepcts 3 parameters:
   *        {app_name} {path_to_plugin_file} {plugin_name}
   *        This retrieves the data read from the plugin file to the output
   *stream.
   *
   *        Other supported options are:
   *        {app_name} --help     outputs application usage to the error stream
   *        {app_name} --version  outputs application version to the error
   *stream
   *
   * @param argc number of parameters passed to the application
   * @param argv array of the parameters passed to the applications
   *        NOTE: the first parameter is expected to be application file name as
   *in c and c++ main() convention
   * @param out_stream      Output stream for the data printed by the
   *application
   * @param out_stream_err  Output stream for the error data printed by the
   *application
   *
   **/
  Plugin_info_app(int argc, const char **argv, std::ostream &out_stream,
                  std::ostream &out_stream_err);

  /**
   * @brief Executes the action requested from the application with the
   *parameters passed to the contructor. Redirects the output to the streams
   *provided to the constructor.
   *
   * @returns 0 on success, -1 if an error occured.
   **/
  int run();

 private:
  void print_usage(const char *exec_name);
  void print_version();

  const int argc_;
  const char **const argv_;

  std::ostream &out_stream_;
  std::ostream &out_stream_err_;
};

#endif
