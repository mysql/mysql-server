/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_DEFAULT_PATHS_INCLUDED
#define ROUTER_DEFAULT_PATHS_INCLUDED

#include <map>
#include <string>

#include "mysql/harness/filesystem.h"  // Path
#include "mysqlrouter/router_export.h"

namespace mysqlrouter {

/**
 * Returns predefined (computed) default paths.
 *
 * Returns a map of predefined default paths, which are computed based on
 * `origin` argument. This argument serves as base directory for any
 * predefined relative paths. The returned map consists of absolue paths.
 *
 * @param origin Base directory which will be prepended to any relative
 *        predefined directories
 *
 * @throws std::invalid_argument (std::logic_error) if `origin` is empty
 */
std::map<std::string, std::string> ROUTER_LIB_EXPORT
get_default_paths(const mysql_harness::Path &origin);

/**
 * Returns absolute path to mysqlrouter.exe currently running.
 *
 * @param argv0 1th element of `argv` array passed to `main()` (i.e.
 * `argv[0]`)
 *
 * @throws std::runtime_error, ...?
 *
 * @note argv0 is currently ignored on Windows platforms
 */
std::string ROUTER_LIB_EXPORT
find_full_executable_path(const std::string &argv0);

}  // namespace mysqlrouter

#endif
