/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysqlrouter/plugin_config.h"

#ifndef _WIN32
#include <sys/un.h>
#include <unistd.h>
#endif

using std::invalid_argument;
using std::string;

namespace mysqlrouter {

string BasePluginConfig::get_section_name(
    const mysql_harness::ConfigSection *section) const noexcept {
  auto name = section->name;
  if (!section->key.empty()) {
    name += ":" + section->key;
  }
  return name;
}

string BasePluginConfig::get_option_string(
    const mysql_harness::ConfigSection *section, const string &option) const {
  bool required = is_required(option);
  string value;

  try {
    value = section->get(option);
  } catch (const mysql_harness::bad_option &exc) {
    if (required) {
      throw option_not_present(get_log_prefix(option) + " is required");
    }
  }

  if (value.empty()) {
    if (required) {
      throw option_empty(get_log_prefix(option) + " needs a value");
    }
    value = get_default(option);
  }

  return value;
}

string BasePluginConfig::get_log_prefix(
    const std::string &option,
    const mysql_harness::ConfigSection *section) const noexcept {
  return "option " + option + " in [" +
         (section ? section->get_section_name(option) : section_name) + "]";
}

/*static*/
std::chrono::milliseconds BasePluginConfig::get_option_milliseconds(
    const std::string &value, double min_value, double max_value,
    const std::string &log_prefix) {
  std::istringstream ss(value);
  // we want to make sure the decinal separator is always '.' regardless of the
  // user locale settings so we force classic locale
  ss.imbue(std::locale("C"));
  double result = 0.0;
  if (!(ss >> result) || !ss.eof() || (result < min_value - 0.0001) ||
      (result > max_value + 0.0001)) {
    std::stringstream os;
    os << log_prefix << " needs value between " << min_value << " and "
       << to_string(max_value) << " inclusive";
    if (!value.empty()) {
      os << ", was '" << value << "'";
    }
    throw std::invalid_argument(os.str());
  }

  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::duration<double>(result));
}

std::chrono::milliseconds BasePluginConfig::get_option_milliseconds(
    const mysql_harness::ConfigSection *section, const std::string &option,
    double min_value, double max_value) const {
  std::string value = get_option_string(section, option);

  return get_option_milliseconds(value, min_value, max_value,
                                 get_log_prefix(option, section));
}

mysql_harness::TCPAddress BasePluginConfig::get_option_tcp_address(
    const mysql_harness::ConfigSection *section, const string &option,
    bool require_port, int default_port) {
  std::string value = get_option_string(section, option);

  if (value.empty()) {
    return mysql_harness::TCPAddress{};
  }

  try {
    std::pair<string, uint16_t> bind_info = mysqlrouter::split_addr_port(value);

    uint16_t port = bind_info.second;

    if (port <= 0) {
      if (default_port > 0) {
        port = static_cast<uint16_t>(default_port);
      } else if (require_port) {
        throw std::runtime_error("TCP port missing");
      }
    }

    return mysql_harness::TCPAddress(bind_info.first, port);

  } catch (const std::runtime_error &exc) {
    throw invalid_argument(get_log_prefix(option) + " is incorrect (" +
                           exc.what() + ")");
  }
}

int BasePluginConfig::get_option_tcp_port(
    const mysql_harness::ConfigSection *section, const string &option) {
  auto value = get_option_string(section, option);

  if (!value.empty()) {
    char *rest;
    errno = 0;
    auto result = std::strtol(value.c_str(), &rest, 0);

    if (errno > 0 || *rest != '\0' || result > UINT16_MAX || result < 1) {
      std::ostringstream os;
      os << get_log_prefix(option)
         << " needs value between 1 and 65535 inclusive";
      if (!value.empty()) {
        os << ", was '" << value << "'";
      }
      throw std::invalid_argument(os.str());
    }

    return static_cast<int>(result);
  }

  return -1;
}

mysql_harness::Path BasePluginConfig::get_option_named_socket(
    const mysql_harness::ConfigSection *section, const string &option) {
  std::string value = get_option_string(section, option);

  std::string error;
  if (!is_valid_socket_name(value, error)) {
    throw invalid_argument(error);
  }

  if (value.empty()) {
    return mysql_harness::Path();
  }
  return mysql_harness::Path(value);
}

}  // namespace mysqlrouter
