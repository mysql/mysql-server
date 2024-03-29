/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_PLUGIN_INFO_PLUGIN_FILE_INCLUDED
#define MYSQLROUTER_PLUGIN_INFO_PLUGIN_FILE_INCLUDED

#include <memory>
#include <string>

#include "plugin.h"

/** @class Library_file
 *
 * @brief Abstraction over the plugin library file, hides system specific
 *        dynamic library handling.
 *
 **/
class Library_file {
 public:
  /** @brief Constructor
   *
   * @param file_name   path to the plugin file on the filesystem
   * @param plugin_name name of the plugin (has to match name of the exported
   *Plugin struct)
   **/
  explicit Library_file(const std::string &file_name,
                        const std::string &plugin_name);

  /** @brief Returns ABI version of the plugin represented by the object.
   **/
  uint32_t get_abi_version() const;

  /** @brief Returns version specific Plugin struct of the plugin.
   *         Specified by the caller through the template parameter.
   *
   * @param symbol name of the struct symbol
   *
   **/
  template <class T>
  T *get_plugin_struct(const std::string &symbol) const;

  /** @brief Destructor
   **/
  ~Library_file();

 private:
  struct Library_file_impl;
  std::unique_ptr<Library_file_impl> impl_;

  template <class T>
  T *get_plugin_struct_internal(const std::string &symbol) const;

  const std::string plugin_name_;
  const std::string file_name_;
};

#endif
