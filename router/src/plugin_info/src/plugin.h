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

#ifndef MYSQLROUTER_PLUGIN_INFO_PLUGIN_INCLUDED
#define MYSQLROUTER_PLUGIN_INFO_PLUGIN_INCLUDED

#include <iostream>
#include <list>

// we duplicate the Plugin struct here to have the history of the potential
// changes but we still include this one for VERSION_ macros
#include "mysql/harness/plugin.h"

/** @struct Plugin_abi
 *
 * @brief The assumed and expected beginning of each version of Plugin struct
 *
 **/
struct Plugin_abi {
  uint32_t abi_version;
};

/** @struct Plugin_v1
 *
 * @brief Data fields of the first version of the Plugin struct.
 *        Whenever this changes, add a new struct (callded vX) here,
 *        respective constructor to Plugin_info and its handling
 *
 **/
struct Plugin_v1 {
  uint32_t abi_version;

  const char *arch_descriptor;
  const char *brief;
  uint32_t plugin_version;

  size_t requires_length;
  const char **requires_plugins;

  size_t conflicts_length;
  const char **conflicts;

  /* some function pointers follow; we are not really interested in those and we
    don't want to be dependent on their types/arguments so we skip them here */
};

/** @class Plugin_info
 *
 * @brief Version independent plugin data storage, defines conversion from
 *        existing versions and enables writing the data as a JSON text.
 *
 **/
class Plugin_info {
 public:
  /** @brief Constructor
   *
   * @param plugin constructor from v1 of Plugin struct
   **/
  explicit Plugin_info(const Plugin_v1 &plugin);

  /** @brief prints the JSON representation of the Plugin_info object to the
   *         selected output stream
   *
   **/
  friend std::ostream &operator<<(std::ostream &stream,
                                  const Plugin_info &plugin_info);

  /** @brief converts ABI version integer to string representation
   *
   * @param ver integer representation to convert
   **/
  static std::string get_abi_version_str(uint32_t ver);

  /** @brief converts plugin version integer to string representation
   *
   * @param ver integer representation to convert
   **/
  static std::string get_plugin_version_str(uint32_t ver);

 private:
  static void copy_to_list(std::list<std::string> &out_list,
                           const char **in_list, size_t in_list_size);

  void print_as_json(std::ostream &out_stream) const;

  uint32_t abi_version;

  std::string arch_descriptor;
  std::string brief;
  uint32_t plugin_version;

  std::list<std::string> requires_plugins;
  std::list<std::string> conflicts;
};

#endif
