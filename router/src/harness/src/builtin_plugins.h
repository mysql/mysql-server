/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_BUILTIN_PLUGINS_INCLUDED
#define MYSQL_HARNESS_BUILTIN_PLUGINS_INCLUDED

#include "mysql/harness/plugin.h"

#include "harness_export.h"

#include <map>
#include <string>

namespace mysql_harness {

/**
 * @brief Singleton class implementing registry of the built-in MySQLRouter
 * plugins.
 *
 * Built-in plugin is statically linked to the harness library (does not have
 * dedicated .so or .dll file) but implements the same API as the external
 * plugin (init(), start(), etc.) that is called by the Loader.
 */
class HARNESS_EXPORT BuiltinPlugins {
 public:
  /**
   * @brief Gets the singleton instance.
   */
  static BuiltinPlugins &instance();

  /**
   * It's a singleton so we disable copying.
   */
  BuiltinPlugins(const BuiltinPlugins &) = delete;
  BuiltinPlugins &operator=(const BuiltinPlugins &) = delete;

  /**
   * Stores the information about a single built-in plugin.
   */
  struct PluginInfo {
    /**
     * pointer to the plugin struct (used by the Loader to run init(),
     * start(), etc on the plugin)
     */
    Plugin *plugin;

    /** if true the plugin should ALWAYS be loaded even if it does not have
     * its section in the configuration
     */
    bool always_load;
  };

  using PluginsMap = std::map<std::string, PluginInfo>;

  /** @brief Checks if there is a built-in plugin with a specified name
   *
   * @param plugin_name plugin name to check
   * @return true if there is a built-in plugin with a given name in the
   * registry, false otherwise
   */
  bool has(const std::string &plugin_name) noexcept;

  /** @brief Returns the map containing information about all built-in plugins
   * in the registry
   *
   * @return map containing information about all built-in plugins in the
   * registry
   */
  const PluginsMap *get() noexcept { return &plugins_; }

  /** @brief Returns pointer to the Plugin struct for the plugin with
   * selected name
   *
   * @throw  std::out_of_range if there is no info about the plugin with a given
   * name.
   *
   * @param plugin_name name of the plugin queried for the Plugin struct
   * @return Plugin struct for the plugin with selected name
   */
  Plugin *get_plugin(const std::string &plugin_name) {
    return plugins_.at(plugin_name).plugin;
  }

  /**
   * add plugin to the built in plugins.
   */
  void add(std::string name, PluginInfo plugin_info);

 private:
  BuiltinPlugins();
  PluginsMap plugins_;
};

}  // namespace mysql_harness

#endif
