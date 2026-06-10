/*
 * Copyright (c) 2017, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MYSQLSHDK_LIBS_UTILS_UTILS_PATH_H_
#define MYSQLSHDK_LIBS_UTILS_UTILS_PATH_H_

#include <string>
#include <tuple>
#include <utility>
#include <vector>
// #include "scripting/common.h"

namespace shcore {
namespace path {
namespace detail {
std::string expand_user(const std::string &path, const std::string &sep);
std::tuple<std::string, std::string> split_extension(const std::string &path,
                                                     const std::string &sep);
size_t span_dirname(const std::string &path);
}  // namespace detail

std::string join_path(const std::vector<std::string> &components);

inline std::string join_path(const std::string &a, const std::string &b) {
  return join_path({a, b});
}

template <typename... Args>
std::string join_path(const std::string &a, const std::string &b,
                      Args... args) {
  return join_path(a, join_path(b, args...));
}

std::pair<std::string, std::string> splitdrive(const std::string &path);

std::string dirname(const std::string &path);
std::string basename(const std::string &path);

#ifdef WIN32
const char path_separator = '\\';
const char pathlist_separator = ';';
#else
const char path_separator = '/';
const char pathlist_separator = ':';
#endif
const char pathlist_separator_s[] = {pathlist_separator, '\0'};

extern const char *k_valid_path_separators;

/**
 * Get home directory path of the user executing the shell.
 *
 * On Unix: `HOME` environment variable is returned if is set, otherwise current
 *          user home directory is looked up in password directory.
 *
 * On Windows: `%HOME%` or `%USERPROFILE%` or `%HOMEDRIVE%` + `%HOMEPATH%`
 *             environment variable is returned if is set, otherwise user home
 *             directory is obtained from system KnownFolder ID
 *             `FOLDERID_Profile`.
 *             If all of above fail, empty string is returned.
 *
 * @return User home directory path.
 */
std::string home();

/**
 * Get home directory path of user associated with the specified login name.
 *
 * On Unix: User home directory path associated with `loginname` is looked up
 *          directly in the password directory.
 *
 * On Windows: Retrieving home directory of another user on Windows platform
 *             is *NOT SUPPORTED*.
 *
 * @param loginname Login name of existing user in system.
 *
 * @return User home directory associated with `loginname` or empty string
 *         if such user does not exist.
 */
std::string home(const std::string &loginname);

/**
 * `expand_user` expand paths beginning with `~` or `~user` (also known as
 * "tilde expansion").
 *
 * Home directory for `~` prefix is obtained by `home()` function.
 * Home directory for `~user` prefix is obtained by `home(user)` function.
 *
 * @param path Path string.
 *
 * @return Return `path` with prefix `~` or `~user` replaced by that user's home
 *         directory. @see home() and @see home(user).
 *         If the expansion fails or if the path does not begin with `~`, the
 *         path is returned unchanged.
 */
std::string expand_user(const std::string &path);

/**
 * Unix:
 *   Normalize a path collapsing redundant separators and relative references.
 *   This string path manipulation, might affect paths containing
 *   symbolic links.
 *
 * Windows:
 *   Retrieves the full path and file name of the specified file. If error
 *   occur path isn't altered.
 *
 * @param path Input path string to normalize.
 * @return Normalized string path.
 */
std::string normalize(const std::string &path);

/**
 * Split path to (root, extension) tuple such that [root + extenstion == path].
 *
 * @param path Original file name
 * @return Tuple (root, extension). Leading periods on basename are ignored,
 *         i.e. split_extension(".dotfile") returns (".dotfile", "").
 */
std::tuple<std::string, std::string> split_extension(const std::string &path);

/**
 * Returns true if the path exists.
 */
bool exists(const std::string &path);

/**
 * Returns path to the given executable name searched in PATH.
 */
std::string search_stdpath(const std::string &name);

/**
 * Returns path to the given executable name searched in the given path list
 * string, separated by the given separator.
 *
 * @param name name of the executable. .exe is automatically appended in Win32
 * @param pathlist string with list of paths to search
 * @param separator separator character used in pathlist. If 0, the default
 *        PATH separator for the platform is used.
 */
std::string search_path_list(const std::string &name,
                             const std::string &pathlist,
                             const char separator = 0);

/**
 * Checks if character is a path separator.
 *
 * @param c - character to be checked.
 *
 * @returns true if the given character is a path separator.
 */
bool is_path_separator(char c);

/**
 * Checks if path is absolute.
 *
 * @param path - path to be checked.
 *
 * @returns true if the given path is absolute.
 */
bool is_absolute(const std::string &path);

/**
 * Provides path to the current working directory.
 */
std::string getcwd();

}  // namespace path
}  // namespace shcore

#endif  // MYSQLSHDK_LIBS_UTILS_UTILS_PATH_H_
