/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include <array>
#include <cstring>  // strlen
#include <map>
#include <string>

#include "mysql/harness/filesystem.h"  // Path
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/utils.h"  // substitute_envvar
#include "router_config.h"

#ifndef _WIN32
#include <unistd.h>
const char dir_sep = '/';
const std::string path_sep = ":";
#else
const char dir_sep = '\\';
const std::string path_sep = ";";
#endif

static const char kProgramName[] = "mysqlrouter";

/** @brief Returns `<path>` if it is absolute[*], `<basedir>/<path>` otherwise
 *
 * [*] `<path>` is considered absolute if it starts with one of:
 *   Unix:    '/'
 *   Windows: '/' or '\' or '.:' (where . is any character)
 *   both:    '{origin}' or 'ENV{'
 * else:
 *   it's considered relative (empty `<path>` is also relative in such respect)
 *
 * @param path Absolute or relative path; absolute path may start with
 *        '{origin}' or 'ENV{'
 * @param basedir Path to grandparent directory of mysqlrouter.exe, i.e.
 *        for '/path/to/bin/mysqlrouter.exe/' it will be '/path/to'
 */
static std::string ensure_absolute_path(const std::string &path,
                                        const std::string &basedir) {
  if (path.empty()) return basedir;
  if (path.compare(0, strlen("{origin}"), "{origin}") == 0) return path;
  if (path.find("ENV{") != std::string::npos) return path;

  // if the path is not absolute, it must be relative to the origin
  return (mysql_harness::Path(path).is_absolute() ? path
                                                  : basedir + dir_sep + path);
}

namespace mysqlrouter {

/*static*/
std::map<std::string, std::string> get_default_paths(
    const mysql_harness::Path &origin) {
  std::string basedir = mysql_harness::Path(origin)
                            .dirname()
                            .str();  // throws std::invalid_argument

  std::map<std::string, std::string> params = {
      {"program", kProgramName},
      {"origin", origin.str()},
#ifdef _WIN32
      {"event_source_name", MYSQL_ROUTER_PACKAGE_NAME},
#endif
      {"logging_folder",
       ensure_absolute_path(MYSQL_ROUTER_LOGGING_FOLDER, basedir)},
      {"plugin_folder",
       ensure_absolute_path(MYSQL_ROUTER_PLUGIN_FOLDER, basedir)},
      {"runtime_folder",
       ensure_absolute_path(MYSQL_ROUTER_RUNTIME_FOLDER, basedir)},
      {"config_folder",
       ensure_absolute_path(MYSQL_ROUTER_CONFIG_FOLDER, basedir)},
      {"data_folder", ensure_absolute_path(MYSQL_ROUTER_DATA_FOLDER, basedir)}};

  // foreach param, s/{origin}/<basedir>/
  for (auto it : params) {
    std::string &param = params.at(it.first);
    param.assign(
        mysqlrouter::substitute_variable(param, "{origin}", origin.str()));
  }
  return params;
}

// throws std::runtime_error, ...?
std::string find_full_executable_path(const std::string &argv0) {
#ifdef _WIN32
  UNREFERENCED_PARAMETER(argv0);

  // the bin folder is not usually in the path, just the lib folder
  std::array<char, MAX_PATH> szPath;
  if (GetModuleFileName(NULL, szPath.data(), szPath.size()) != 0) {
    return szPath.data();
  }
#else
  mysql_harness::Path p_argv0(argv0);

  // Path normalizes '\' to '/'
  if (p_argv0.str().find('/') != std::string::npos) {
    // Path is either absolute or relative to the current working dir, so
    // we can use realpath() to find the full absolute path
    return p_argv0.real_path().str();
  } else {
    // Program was found via PATH lookup by the shell, so we
    // try to find the program in one of the PATH dirs
    const char *env_path = getenv("PATH");
    std::string path(env_path ? env_path : "");

    size_t begin{0};
    do {
      size_t found = path.find(path_sep, begin);

      std::string path_name;

      if (found == begin) {
        // just ":"
      } else if (found == std::string::npos) {
        // not found ...
        path_name = path.substr(begin);
      } else {
        path_name = path.substr(begin, found - begin);
      }

      if (!path_name.empty()) {  // if not only a separator
        auto abs_file_path = mysql_harness::Path(path_name).join(argv0);

        if (mysqlrouter::my_check_access(abs_file_path.str())) {
          return abs_file_path.real_path().str();
        }
      }

      if (found == std::string::npos) break;

      begin = found + 1;  // skip the sep.
    } while (true);
  }
#endif
  throw std::logic_error("Could not find own installation directory");
}

}  // namespace mysqlrouter
