/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_PLUGIN_CONFIG_INCLUDED
#define MYSQL_HARNESS_PLUGIN_CONFIG_INCLUDED

#include <chrono>
#include <limits>
#include <optional>
#include <string>

#include "harness_export.h"
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"

namespace mysql_harness {

/** Exception that gets thrown when the configuration option is missing
 */
class option_not_present : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

/** Exception that gets thrown when the configuration option is present
 *  but it is empty value
 */
class option_empty : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

/**
 * Retrieve and manage plugin configuration.
 *
 * BasePluginConfig is an abstract class which can be used to by plugins
 * to derive their own class retrieving configuration from, for example,
 * Harness `ConfigSection instances`.
 */
class HARNESS_EXPORT BasePluginConfig {
 public:
  BasePluginConfig() = default;

  BasePluginConfig(const BasePluginConfig &) = default;
  BasePluginConfig(BasePluginConfig &&) = default;

  BasePluginConfig &operator=(const BasePluginConfig &) = default;
  BasePluginConfig &operator=(BasePluginConfig &&) = default;

  /**
   * destructor
   */
  virtual ~BasePluginConfig() = default;

  /**
   * get description of the option.
   *
   * For example, option wait_timeout in section [routing:homepage] will
   * return a prefix (without quotes):
   *
   *     option wait_timeout in [routing:homepage]
   *
   * @param section configuration section
   * @param option Name of the option
   *
   * @return Prefix as std::string
   */
  std::string get_option_description(
      const mysql_harness::ConfigSection *section,
      const std::string &option) const;

  /**
   * Gets the default for the given option.
   *
   * Gets the default value of the given option. If no default option
   * is available, an empty string is returned.
   *
   * @param option name of the option
   * @return default value for given option as std::string
   */
  virtual std::string get_default(const std::string &option) const = 0;

  /**
   * Returns whether the given option is required.
   *
   * @return bool
   */
  virtual bool is_required(const std::string &option) const = 0;

  /**
   * get option value.
   *
   * gets the option from a config-section (or its default value if it doesn't
   * exist) and converts it with a transformation function.
   *
   * @param section configuration section
   * @param option name of the option
   * @param transformer transformation function. The signature of the
   * transformation function should be equivalent to:
   *   @c (const std::string &value, const std::string &option_description)
   *   and returns the transformed value.
   * @returns return value of the the transformation function.
   */
  template <class Func>
  decltype(auto) get_option(const mysql_harness::ConfigSection *section,
                            const std::string &option,
                            Func &&transformer) const {
    const auto value = get_option_string_or_default_(section, option);

    return transformer(value, get_option_description(section, option));
  }

  /**
   * get option value.
   *
   * gets the option from a config-section and converts it with a transformation
   * function.
   *
   * does not call get_default().
   *
   * @param section configuration section
   * @param option name of the option
   * @param transformer transformation function. The signature of the
   * transformation function should be equivalent to:
   *   @c (const std::string &value, const std::string &option_description)
   *   and returns the transformed value.
   * @returns transformed value.
   */
  template <class Func>
  decltype(auto) get_option_no_default(
      const mysql_harness::ConfigSection *section, const std::string &option,
      Func &&transformer) const {
    const auto value = get_option_string_(section, option);

    return transformer(value, get_option_description(section, option));
  }

  /**
   * Gets value of given option as string.
   *
   * @throws option_not_present if the required option is missing
   * @throws option_empty if the required option is present but empty
   *
   * @param section Instance of ConfigSection
   * @param option name of the option
   * @return Option value as std::string
   *
   */
  [[deprecated("use get_option(..., StringOption{}) instead")]]
  // line-break
  std::string
  get_option_string(const mysql_harness::ConfigSection *section,
                    const std::string &option) const {
    return get_option(section, option,
                      [](auto const &value, auto const &) { return value; });
  }

  /**
   * Gets an unsigned integer using the given option.
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
  template <class T>
  [[deprecated("used get_option(..., IntOption<T>{}) instead")]]
  // line-break
  T get_uint_option(const mysql_harness::ConfigSection *section,
                  const std::string &option, T min_value = 0,
                  T max_value = std::numeric_limits<T>::max()) const {
    return get_option(section, option, IntOption<T>{min_value, max_value});
  }

  /**
   * Gets a number of milliseconds using the given option.
   *
   * The expected option value is a string with floating point number in seconds
   * (with '.' as a decimal separator) in standard or scientific notation
   *
   * - for value = "1.0" expected result is std::chrono:milliseconds(1000)
   * - for value = "0.01" expected result is std::chrono:milliseconds(10)
   * - for value = "1.6E-2" expected result is std::chrono:milliseconds(16)
   *
   * @param section Instance of ConfigSection
   * @param option Option name in section
   * @param min_value Minimum value
   * @param max_value Maximum value
   *
   * @return value converted to milliseconds
   * @throws std::invalid_argument on errors
   */
  [[deprecated("used get_option(..., MilliSecondsOption{}) instead")]]
  // line-break
  std::chrono::milliseconds
  get_option_milliseconds(
      const mysql_harness::ConfigSection *section, const std::string &option,
      double min_value = 0.0,
      double max_value = std::numeric_limits<double>::max()) const {
    return get_option(section, option,
                      MilliSecondsOption{min_value, max_value});
  }

 protected:
  /**
   * Constructor for derived classes.
   */
  BasePluginConfig(const mysql_harness::ConfigSection *section)
      : section_name_{get_section_name(section)} {}

  /**
   * Generate the name for this configuration.
   *
   * @param section Instance of ConfigSection
   * @return the name for this configuration
   */
  static std::string get_section_name(
      const mysql_harness::ConfigSection *section);

 private:
  /**
   * Name of the section
   */
  std::string section_name_;

  /**
   * get value of an option from a config-section.
   *
   * does not call get_default()
   *
   * @return a value of the option if it exists.
   */
  std::optional<std::string> get_option_string_(
      const mysql_harness::ConfigSection *section,
      const std::string &option) const;

  /**
   * get value of an option from a config-section.
   *
   * gets value from get_default() if the option-value
   * is not present or empty.
   *
   * @return a value of the option if it exists.
   */
  std::string get_option_string_or_default_(
      const mysql_harness::ConfigSection *section,
      const std::string &option) const;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_PLUGIN_CONFIG_INCLUDED
