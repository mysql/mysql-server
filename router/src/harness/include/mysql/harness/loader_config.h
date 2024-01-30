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

#ifndef MYSQL_HARNESS_LOADER_CONFIG_INCLUDED
#define MYSQL_HARNESS_LOADER_CONFIG_INCLUDED

#include <stdexcept>

#include "config_parser.h"
#include "harness_export.h"

namespace mysql_harness {

class Path;

/**
 * Configuration file handler for the loader.
 *
 * @ingroup ConfigParser
 *
 * Specialized version of the config file read that does some extra
 * checks after reading the configuration file.
 */
class HARNESS_EXPORT LoaderConfig : public Config {
 public:
  using Config::Config;

  /**
   * Fill and check the configuration.
   *
   * This function will fill in default values for any options that
   * should have default values and check all sections to make sure
   * that they have valid values.
   *
   * @exception bad_section Thrown if the configuration is not correct.
   */
  void fill_and_check();

  /**
   * Read a configuration entry.
   *
   * This will read one configuration entry and incorporate it into
   * the configuration. The entry can be either a directory or a file.
   *
   * This function allows reading multiple configuration entries and
   * can be used to load paths of configurations. An example of how it
   * could be used is:
   *
   * @code
   * LoaderConfig config;
   * for (auto&& entry: my_path)
   *    config.read(entry);
   * @endcode
   *
   * @param path Path to configuration entry to read.
   * @throws std::invalid_argument, std::runtime_error, syntax_error, ...
   */
  void read(const Path &path);

  /**
   * Read a configuration entry.
   *
   * This will read one configuration entry and incorporate it into
   * the configuration. The entry is the input stream object.
   *
   *
   * @param input Input stream with the configuration to read from.
   * @throws std::invalid_argument, std::runtime_error, syntax_error, ...
   */
  void read(std::istream &input);

  /**
   * Return true if we are logging to a file, false if we are logging
   * to console instead.
   */
  bool logging_to_file() const;

  /**
   * Return log filename.
   *
   * @throws std::invalid_argument if not logging to file
   */
  Path get_log_file() const;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_LOADER_CONFIG_INCLUDED
