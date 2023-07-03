/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef _ROUTER_COMPONENT_SYSTEM_LAYOUT_H_
#define _ROUTER_COMPONENT_SYSTEM_LAYOUT_H_

#include <string>
#include "mysql/harness/filesystem.h"

/** @class RouterSystemLayout
 *
 * Helper class for preparing system layout for bootstrap tests
 *
 **/
class RouterSystemLayout {
 public:
  /** @brief Constructor
   */
  RouterSystemLayout();

  /** @brief Create temporary directory that represents system deployment
   * layout for mysqlrouter bootstrap. A mysqlrouter executable is copied to
   * tmp_dir_/stage/bin/ and then an execution permission is assigned to it.
   *
   * @param myslrouter_path path to the MySQLRouter binary
   * @param origin_path     path to the directory containing the calling binary
   *
   * After the test is completed init_system_layout_dir() should be called for
   * the proper cleanup.
   */
  void init_system_layout_dir(const mysql_harness::Path &myslrouter_path,
                              const mysql_harness::Path &origin_path);

  /*
   * Cleans up the directories and files created by the init_system_layout_dir()
   */
  void cleanup_system_layout();

 protected:
  std::string tmp_dir_;
  std::string exec_file_;
  std::string config_file_;

#ifdef __APPLE__
  std::string library_link_file_;
#endif
};

#endif  // _ROUTER_COMPONENT_SYSTEM_LAYOUT_H_
