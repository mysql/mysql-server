/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "router_component_system_layout.h"

#ifndef _WIN32
#include <sys/stat.h>  // chmod
#include <unistd.h>
#endif
#include <cstring>
#include <stdexcept>

#include "mysql/harness/filesystem.h"
#include "mysqlrouter/utils.h"  // copy_file

RouterSystemLayout::RouterSystemLayout() = default;

void RouterSystemLayout::init_system_layout_dir(
    const mysql_harness::Path &myslrouter_path,
    const mysql_harness::Path &origin_path) {
  tmp_dir_ = mysql_harness::get_tmp_dir();
  mysql_harness::mkdir(tmp_dir_ + "/stage/bin", 0700, true);
  exec_file_ = tmp_dir_ + "/stage/bin/mysqlrouter";
  mysql_harness::mkdir(tmp_dir_ + "/stage/var/lib", 0700, true);
  mysqlrouter::copy_file(myslrouter_path.str(), exec_file_);
#ifndef _WIN32
  chmod(exec_file_.c_str(), 0700);
#endif

  // on MacOS we need to create symlink to library_output_directory
  // inside our temp dir as mysqlrouter has @loader_path/../lib
  // hardcoded by MYSQL_ADD_EXECUTABLE
#ifdef __APPLE__
  std::string cur_dir_name = origin_path.real_path().dirname().str();
  const std::string library_output_dir =
      cur_dir_name + "/library_output_directory";

  library_link_file_ = std::string(
      mysql_harness::Path(tmp_dir_).real_path().str() + "/stage/lib");

  if (symlink(library_output_dir.c_str(), library_link_file_.c_str())) {
    throw std::runtime_error(
        "Could not create symbolic link to library_output_directory: " +
        std::to_string(errno));
  }
#else
  (void)origin_path;
#endif
  config_file_ = tmp_dir_ + "/stage/mysqlrouter.conf";
}

void RouterSystemLayout::cleanup_system_layout() {
#ifdef __APPLE__
  unlink(library_link_file_.c_str());
#endif
  mysql_harness::delete_dir_recursive(tmp_dir_);
}
