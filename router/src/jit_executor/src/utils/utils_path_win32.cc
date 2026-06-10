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

// #include "mysqlshdk/libs/utils/utils_path.h"
#include "utils/utils_path.h"
#include "utils/utils_string.h"

#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <tuple>

#include <ShlObj.h>

// #include "mysqlshdk/libs/utils/utils_general.h"
// #include "mysqlshdk/libs/utils/utils_string.h"

namespace shcore {
namespace path {

const char *k_valid_path_separators = "/\\";

namespace detail {
class Known_folder_path {
 public:
  Known_folder_path() = default;
  Known_folder_path(const Known_folder_path &other) = delete;
  Known_folder_path(Known_folder_path &&other) = delete;
  Known_folder_path &operator=(const Known_folder_path &other) = delete;
  Known_folder_path &operator=(Known_folder_path &&other) = delete;
  ~Known_folder_path() { CoTaskMemFree(path_); }

  std::string operator()(const KNOWNFOLDERID folder_id) {
    HRESULT hr = SHGetKnownFolderPath(folder_id, 0, nullptr, &path_);
    if (SUCCEEDED(hr)) {
      std::mbstate_t s = std::mbstate_t();
      const wchar_t *p = path_;
      size_t len = std::wcsrtombs(nullptr, &p, 0, &s);
      if (len != static_cast<std::size_t>(-1)) {
        std::string path(len, '\0');
        std::wcsrtombs(&path[0], &p, path.size(), &s);
        return path;
      }
    }

    return std::string{};
  }

 private:
  PWSTR path_ = nullptr;
};
}  // namespace detail

/*
 * Join two or more pathname components, inserting '\\' as needed.
 * If any component is an absolute path, all previous path components
 * will be discarded.  An empty last part will result in a path that
 * ends with a separator.
 * @param components vector of strings with the paths to join
 *
 * @return the concatenation of all paths with exactly one directory separator
 *         following each non-empty part except the last, meaning that the
 *         result will only end in a separator if the last part is empty.
 */
std::string join_path(const std::vector<std::string> &components) {
  std::string result, result_drive, result_path;
  std::string p_drive, p_path, lp_drive, lresult_drive;
  if (!components.empty())
    std::tie(result_drive, result_path) = splitdrive(components.at(0));
  else
    return "";

  for (size_t i = 1; i < components.size(); ++i) {
    std::tie(p_drive, p_path) = splitdrive(components.at(i));
    if (!p_path.empty() && (p_path.front() == '\\' || p_path.front() == '/')) {
      // second path is absolute, so forget about first
      if (!p_drive.empty() || result_drive.empty()) result_drive = p_drive;
      result_path = p_path;
      continue;
    } else if (!p_drive.empty() && p_drive != result_drive) {
      // Convert drives to lower case.
      // TODO(nelson): This will fail for non unicode characters
      lp_drive.assign(p_drive);
      lresult_drive.assign(result_drive);
      std::transform(p_drive.begin(), p_drive.end(), lp_drive.begin(),
                     ::tolower);
      std::transform(result_drive.begin(), result_drive.end(),
                     lresult_drive.begin(), ::tolower);

      if (lp_drive != lresult_drive) {
        // Different drives, ignore the first path entirely
        result_drive = p_drive;
        result_path = p_path;
        continue;
      }
      // same drive but in different case
      result_drive = p_drive;
    }
    // Second path is relative to the first
    if (!result_path.empty() && result_path.back() != '\\' &&
        result_path.back() != '/')
      result_path += '\\';
    result_path += p_path;
  }
  // Add separator between UNC and non-absolute path
  if (!result_path.empty() && result_path.front() != '\\' &&
      result_path.front() != '/' && !result_drive.empty() &&
      result_drive.back() != ':')
    return result_drive + "\\" + result_path;
  return result_drive + result_path;
}

/*
 * Split a pathname into drive/UNC sharepoint and relative path specifiers.
 *  Returns an std::pair of std::strings (drive_or_unc, path); either part may
 *  be empty.
 *
 *  If you assign
 *      result = splitdrive(p)
 *  It is always true that:
 *      result.first + result.second == p
 *
 *  If the path contained a drive letter, drive_or_unc will contain everything
 *  up to and including the colon.  e.g. splitdrive("c:/dir") returns
 *  ("c:", "/dir")
 *
 *  If the path contained a UNC path, the drive_or_unc will contain the host
 *  name and share up to but not including the fourth directory separator
 *  character. e.g. splitdrive("//host/computer/dir") returns
 *  ("//host/computer", "/dir")
 *
 *  Paths cannot contain both a drive letter and a UNC path.
 * @param string with the path name to be split
 *
 * @return a pair (drive, tail) where the drive is either a drive specification
 *         or empty string. On Unix systems drive will always be empty string.
 *
 */
std::pair<std::string, std::string> splitdrive(const std::string &path) {
  if (path.size() > 1) {
    std::string norm_path = path;
    // replace all '/' with '\' character
    std::replace(norm_path.begin(), norm_path.end(), '/', '\\');
    std::string first_two_chars, third_char;
    try {
      first_two_chars = norm_path.substr(0, 2);
    } catch (const std::out_of_range &) {
      first_two_chars = "";
    }
    try {
      third_char = norm_path.substr(2, 1);
    } catch (const std::out_of_range &) {
      third_char = "";
    }
    if (first_two_chars == std::string(2, '\\') && third_char != "\\") {
      // it is a UNC path:
      auto index = norm_path.find('\\', 2);
      if (index == std::string::npos) {
        return std::make_pair("", path);
      }
      auto index2 = norm_path.find('\\', index + 1);
      // a UNC path can't have two slashes in a row
      // (after the initial two)
      if (index2 == index + 1) return std::make_pair("", path);
      if (index2 == std::string::npos) index2 = path.size();
      return std::make_pair(path.substr(0, index2), path.substr(index2));
    }
    if (norm_path.at(1) == ':')
      return std::make_pair(path.substr(0, 2), path.substr(2));
  }
  return std::make_pair("", path);
}

std::string home() {
  const char *env_home = getenv("HOME");
  if (env_home != nullptr) {
    return std::string{env_home};
  }

  const char *env_userprofile = getenv("USERPROFILE");
  if (env_userprofile != nullptr) {
    return std::string{env_userprofile};
  }

  const char *env_homedrive = getenv("HOMEDRIVE");
  const char *env_homepath = getenv("HOMEPATH");
  if (env_homedrive != nullptr && env_homepath != nullptr) {
    return std::string{env_homedrive} + std::string{env_homepath};
  }

  auto profile_path = detail::Known_folder_path()(FOLDERID_Profile);
  return profile_path;
}

std::string home(const std::string & /* username */) {
  // retrieving home directory of another user on Windows is NOT SUPPORTED.
  return std::string{};
}

std::string expand_user(const std::string &path) {
  const char sep[] = "\\/";
  return detail::expand_user(path, sep);
}

std::tuple<std::string, std::string> split_extension(const std::string &path) {
  const char sep[] = "\\/";
  return detail::split_extension(path, sep);
}

std::string normalize(const std::string &path) {
  const auto norm = str_replace(path, "/", "\\");
  const auto wide_norm = utf8_to_wide(norm);
  // get length (including the terminating null character)
  const auto length = GetFullPathNameW(wide_norm.c_str(), 0, nullptr, nullptr);
  // include space for a terminating null character
  std::wstring result(length, 'x');
  // get the data
  GetFullPathNameW(wide_norm.c_str(), result.length(), &result[0], nullptr);

  // remove the terminating null characters
  while (!result.empty() && L'\0' == result.back()) {
    result.pop_back();
  }

  return wide_to_utf8(result);
}

std::string dirname(const std::string &path) {
  std::string drive, dir;
  std::tie(drive, dir) = splitdrive(path);
  size_t xx = detail::span_dirname(dir);
  if (xx == std::string::npos) return drive.empty() ? "." : drive;
  return drive + dir.substr(0, xx);
}

std::string basename(const std::string &path) {
  std::string drive, dir;
  std::tie(drive, dir) = splitdrive(path);

  size_t end = dir.find_last_not_of(k_valid_path_separators);
  if (end == std::string::npos)  // only separators
    return dir.substr(0, 1);
  end++;  // go to after the last char
  size_t p = detail::span_dirname(dir);
  if (p == std::string::npos || p == dir.size() || p == 0 || p == end)
    return dir.substr(0, end);
  size_t pp = dir.find_first_not_of(k_valid_path_separators, p);
  if (pp != std::string::npos) p = pp;
  return dir.substr(p, end - p);
}

bool exists(const std::string &filename) {
  const auto wide_filename = utf8_to_wide(filename);
  DWORD attributes = GetFileAttributesW(wide_filename.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES;
}

bool is_absolute(const std::string &path) {
  if (path.length() > 0) {
    const auto expanded = expand_user(path);
    const auto first = expanded[0];

    return is_path_separator(first) ||
           (expanded.length() > 2 &&
            ((first >= 'A' && first <= 'Z') ||
             (first >= 'a' && first <= 'z')) &&
            ':' == expanded[1] && is_path_separator(expanded[2]));
  }

  return false;
}

std::string getcwd() {
  // get length (including the terminating null character)
  const auto length = GetCurrentDirectoryW(0, nullptr);
  // include space for a terminating null character
  std::wstring result(length, 'x');
  // get the data
  GetCurrentDirectoryW(result.length(), &result[0]);

  // remove the terminating null characters
  while (!result.empty() && L'\0' == result.back()) {
    result.pop_back();
  }

  return wide_to_utf8(result);
}

}  // namespace path
}  // namespace shcore
