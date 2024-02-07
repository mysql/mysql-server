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

#include "plugin_info_app.h"

#include <sstream>
#include <stdexcept>
#include <vector>

#include "library_file.h"
#include "mysql/harness/vt100.h"
#include "print_version.h"
#include "router_config.h"  // MYSQL_ROUTER_PACKAGE_NAME
#include "welcome_copyright_notice.h"

PluginInfoFrontend::PluginInfoFrontend(
    const std::string &exe_name, const std::vector<std::string> &arguments,
    std::ostream &out)
    : program_name_(exe_name), cout_(out) {
  prepare_command_options();
  try {
    arg_handler_.process(arguments);
  } catch (const std::invalid_argument &e) {
    throw FrontendError(e.what());
  }
}

constexpr const char kNewline[] = "\n";

std::string PluginInfoFrontend::get_version() const noexcept {
  std::stringstream os;

  std::string version_string;
  build_version(std::string(MYSQL_ROUTER_PACKAGE_NAME), &version_string);

  os << version_string << kNewline;
  os << ORACLE_WELCOME_COPYRIGHT_NOTICE("2015") << kNewline;

  return os.str();
}

std::string PluginInfoFrontend::get_help(const size_t screen_width) const {
  std::stringstream os;

  os << Vt100::render(Vt100::Render::Bold)
     << "Usage: " << Vt100::render(Vt100::Render::Normal) << program_name_
     << " <mysqlrouter_plugin_file> <plugin_name>" << kNewline << kNewline;

  os << Vt100::render(Vt100::Render::Bold) << "# Examples"
     << Vt100::render(Vt100::Render::Normal) << kNewline << kNewline;

  os << "Print plugin information:" << kNewline << kNewline;
  os << "    " << program_name_
#ifndef _WIN32
     << " /usr/lib/mysqlrouter/routing.so routing"
#else
     << " \"c:\\Program Files\\MySQL\\MySQL Router "
        "8.0\\lib\\routing.dll\" routing"
#endif
     << kNewline << kNewline;
  os << Vt100::render(Vt100::Render::Bold) << "# Options"
     << Vt100::render(Vt100::Render::Normal) << kNewline << kNewline;

  for (const auto &line : arg_handler_.option_descriptions(screen_width, 6)) {
    os << line << kNewline;
  }

  return os.str();
}

void PluginInfoFrontend::prepare_command_options() {
  arg_handler_.add_option(
      CmdOption::OptionNames({"-V", "--version"}),
      "Display version information and exit.", CmdOptionValueReq::none, "",
      [this](const std::string &) { this->config_.cmd = Cmd::SHOW_VERSION; });

  arg_handler_.add_option(
      CmdOption::OptionNames({"-?", "--help"}), "Display this help and exit.",
      CmdOptionValueReq::none, "",
      [this](const std::string &) { this->config_.cmd = Cmd::SHOW_HELP; });
}

int PluginInfoFrontend::run() {
  switch (config_.cmd) {
    case Cmd::SHOW_HELP:
      cout_ << get_help() << std::flush;
      return EXIT_SUCCESS;
    case Cmd::SHOW_VERSION:
      cout_ << get_version() << std::flush;
      return EXIT_SUCCESS;
    default:
      break;
  }

  auto &rest_args = arg_handler_.get_rest_arguments();
  auto rest_args_count = rest_args.size();

  if (rest_args_count != 2) {
    throw UsageError("<file> and <plugin_name> are required");
  }

  const std::string file_name = rest_args[0];
  const std::string plugin_name = rest_args[1];

  try {
    Library_file plugin_file(file_name, plugin_name);
    uint32_t abi_version = plugin_file.get_abi_version();

    if (abi_version > mysql_harness::PLUGIN_ABI_VERSION) {
      throw FrontendError("Unsupported plugin ABI version: " +
                          Plugin_info::get_abi_version_str(abi_version));
    }

    // all the other versions so far have the same structure from our
    // perspective
    Plugin_v1 *plugin = plugin_file.get_plugin_struct<Plugin_v1>(plugin_name);
    Plugin_info plugin_info(*plugin);

    cout_ << plugin_info << std::endl;

    return EXIT_SUCCESS;
  } catch (const std::runtime_error &e) {
    throw FrontendError(e.what());
  }
}
