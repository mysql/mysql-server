/*
  Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLROUTER_PLUGIN_CONFIG_INCLUDED
#define MYSQLROUTER_PLUGIN_CONFIG_INCLUDED

#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/utils.h"
#include "tcp_address.h"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>

#ifdef _WIN32
#pragma push_macro("max")
#undef max
#endif

namespace mysqlrouter {

/** Exception that gets thrown when the configuarion option is missing
 */
class option_not_present : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

/** Exception that gets thrown when the configuarion option is present
 *  but it is empty value
 */
class option_empty : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

/** @class BasePluginConfig
 * @brief Retrieve and manage plugin configuration
 *
 * BasePluginConfig is an abstract class which can be used to by plugins
 * to derive their own class retrieving configuration from, for example,
 * Harness `ConfigSection instances`.
 */
class BasePluginConfig {
 public:
  using defaults_map = std::map<std::string, std::string>;

  /** @brief Constructor
   */
  BasePluginConfig() {}

  /**
   * destructor
   */
  virtual ~BasePluginConfig() = default;

  /** @brief Gets value of given option as string
   *
   * @throws option_not_present if the required option is missing
   * @throws option_empty if the required option is present but empty
   *
   * @param section Instance of ConfigSection
   * @param option name of the option
   * @return Option value as std::string
   *
   */
  std::string get_option_string(const mysql_harness::ConfigSection *section,
                                const std::string &option) const;

  /** @brief Gets a number of milliseconds from a string value
   *
   * The expected option value is a string with floating point number in seconds
   * (with '.' as a decimal separator) in standard or scientific notation
   * Example:
   *  for value = "1.0" expected result is std::chrono:milliseconds(1000)
   *  for value = "0.01" expected result is std::chrono:milliseconds(10)
   *  for value = "1.6E-2" expected result is std::chrono:milliseconds(16)
   *
   * @param value Instance of ConfigSection
   * @param min_value Minimum value
   * @param max_value Maximum value
   * @param log_prefix prefix to be used when creating a message for the
   *        exception
   * @return value converted to milliseconds
   * @throws std::invalid_argument on errors
   */
  static std::chrono::milliseconds get_option_milliseconds(
      const std::string &value, double min_value = 0.0,
      double max_value = std::numeric_limits<double>::max(),
      const std::string &log_prefix = "");

  /** @brief Name of the section */
  std::string section_name;

 protected:
  /** @brief Constructor for derived classes */
  BasePluginConfig(const mysql_harness::ConfigSection *section)
      : section_name(get_section_name(section)) {}

  /** @brief Generate the name for this configuration
   *
   * @param section Instance of ConfigSection
   * @return the name for this configuration
   */
  virtual std::string get_section_name(
      const mysql_harness::ConfigSection *section) const noexcept;

  /** @brief Gets the default for the given option
   *
   * Gets the default value of the given option. If no default option
   * is available, an empty string is returned.
   *
   * @param option name of the option
   * @return default value for given option as std::string
   */
  virtual std::string get_default(const std::string &option) const = 0;

  /** @brief Returns whether the given option is required
   *
   * @return bool
   */
  virtual bool is_required(const std::string &option) const = 0;

  /**
   * @brief Returns message prefix for option and section
   *
   * Gets the message prefix of option and section. The option
   * name will be mentioned as well as the section from the configuration.
   *
   * For example, option wait_timeout in section [routing:homepage] will
   * return a prefix (without quotes):
   *   "option wait_timeout in [routing:homepage]"
   *
   * This is useful when reporting errors.
   *
   * @param option Name of the option
   * @param section Pointer to Instance of ConfigSection, nullptr by default
   * @return Prefix as std::string
   */
  virtual std::string get_log_prefix(
      const std::string &option,
      const mysql_harness::ConfigSection *section = nullptr) const noexcept;

  /** @brief Gets an unsigned integer using the given option
   *
   * Gets an unsigned integer using the given option. The type can be
   * any unsigned integer type such as uint16_t.
   *
   * The min_value argument can be used to set a minimum value for
   * the option. For example, when 0 (zero) is not allowed, min_value
   * can be set to 0. The maximum value is whatever the maximum of the
   * use type is.
   *
   * Throws std::invalid_argument on errors.
   *
   * @param section Instance of ConfigSection
   * @param option Option name in section
   * @param min_value Minimum value
   * @param max_value Maximum value
   * @return value read from the configuration
   */
  template <typename T>
  T get_uint_option(const mysql_harness::ConfigSection *section,
                    const std::string &option, T min_value = 0,
                    T max_value = std::numeric_limits<T>::max()) {
    std::string value = get_option_string(section, option);

    static_assert(
        std::numeric_limits<T>::max() <= std::numeric_limits<long long>::max(),
        "");

    char *rest;
    errno = 0;
    long long tol = std::strtoll(value.c_str(), &rest, 0);
    T result = static_cast<T>(tol);

    if (tol < 0 || errno > 0 || *rest != '\0' || result > max_value ||
        result < min_value ||
        result != tol ||  // if casting lost high-order bytes
        (max_value > 0 && result > max_value)) {
      std::ostringstream os;
      os << get_log_prefix(option, section) << " needs value between "
         << min_value << " and " << to_string(max_value) << " inclusive";
      if (!value.empty()) {
        os << ", was '" << value << "'";
      }
      throw std::invalid_argument(os.str());
    }
    return result;
  }

  /** @brief Gets a number of milliseconds using the given option
   *
   * The expected option value is a string with floating point number in seconds
   * (with '.' as a decimal separator) in standard or scientific notation
   * Example:
   *  for value = "1.0" expected result is std::chrono:milliseconds(1000)
   *  for value = "0.01" expected result is std::chrono:milliseconds(10)
   *  for value = "1.6E-2" expected result is std::chrono:milliseconds(16)
   *
   * @param section Instance of ConfigSection
   * @param option Option name in section
   * @param min_value Minimum value
   * @param max_value Maximum value
   * @return value read from the configuration converted to milliseconds
   * @throws std::invalid_argument on errors
   */
  std::chrono::milliseconds get_option_milliseconds(
      const mysql_harness::ConfigSection *section, const std::string &option,
      double min_value = 0.0,
      double max_value = std::numeric_limits<double>::max()) const;

  /** @brief Gets a TCP address using the given option
   *
   * Gets a TCP address using the given option. The option value is
   * split in 2 giving the IP (or address) and the TCP Port. When
   * require_port is true, a valid port number will be required.
   *
   * Throws std::invalid_argument on errors.
   *
   * @param section Instance of ConfigSection
   * @param option Option name in section
   * @param require_port Whether a TCP port is required
   * @param default_port default port
   * @return mysql_harness::TCPAddress
   */
  mysql_harness::TCPAddress get_option_tcp_address(
      const mysql_harness::ConfigSection *section, const std::string &option,
      bool require_port = false, int default_port = -1);

  int get_option_tcp_port(const mysql_harness::ConfigSection *section,
                          const std::string &option);

  /** @brief Gets location of a named socket
   *
   * Gets location of a named socket. The option value is checked first
   * for its validity. For example, on UNIX system the path can be
   * at most (sizeof(sockaddr_un().sun_path)-1) characters.
   *
   * Throws std::invalid_argument on errors.
   *
   * @param section Instance of ConfigSection
   * @param option Option name in section
   * @return Path object
   */
  mysql_harness::Path get_option_named_socket(
      const mysql_harness::ConfigSection *section, const std::string &option);
};

}  // namespace mysqlrouter

#ifdef _WIN32
#pragma pop_macro("max")
#endif

#endif  // MYSQLROUTER_PLUGIN_CONFIG_INCLUDED
