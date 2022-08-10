/*
Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/stdx/filesystem.h"

#ifndef _WIN32
#include <unistd.h>  // unlink
#else
#include <Windows.h>  // GetLastError
#include <direct.h>   // _getcwd, _rmdir
#include <io.h>       // _unlink
#endif

#include <array>
#include <climits>
#include <system_error>

#include "mysql/harness/stdx/expected.h"

#if !defined(PATH_MAX) && defined(MAX_PATH)
#define PATH_MAX MAX_PATH
#endif

namespace stdx {
namespace filesystem {

static std::error_code last_posix_error_code() {
  return {errno, std::generic_category()};
}

static std::error_code last_error_code() {
#ifdef _WIN32
  return {static_cast<int>(GetLastError()), std::system_category()};
#else
  return last_posix_error_code();
#endif
}

namespace impl {
stdx::expected<void, std::error_code> unlink(const char *path_name) {
#ifdef _WIN32
  if (-1 == ::_unlink(path_name)) {
    return stdx::make_unexpected(last_posix_error_code());
  }
#else
  if (-1 == ::unlink(path_name)) {
    return stdx::make_unexpected(last_error_code());
  }
#endif
  return {};
}

stdx::expected<void, std::error_code> rmdir(const char *path_name) {
#ifdef _WIN32
  if (-1 == ::_rmdir(path_name)) {
    return stdx::make_unexpected(last_posix_error_code());
  }
#else
  if (-1 == ::rmdir(path_name)) {
    return stdx::make_unexpected(last_error_code());
  }
#endif
  return {};
}

stdx::expected<std::string, std::error_code> getcwd() {
#ifdef _WIN32
  std::array<char, MAX_PATH> cwd;
  if (nullptr == ::_getcwd(cwd.data(), cwd.size())) {
    // _getcwd() sets errno
    return stdx::make_unexpected(last_posix_error_code());
  }
#else
  std::array<char, PATH_MAX> cwd{};
  if (nullptr == ::getcwd(cwd.data(), cwd.size())) {
    return stdx::make_unexpected(last_error_code());
  }
#endif

  return {cwd.data()};
}

}  // namespace impl

path temp_directory_path(std::error_code &ec) {
  ec.clear();
#ifdef _WIN32
  std::array<char, MAX_PATH + 1> d;
  auto sz = GetTempPath(d.size(), d.data());
  if (sz == 0) {
    ec = last_error_code();
    return {};
  }

  return {std::string(d.begin(), std::next(d.begin(), sz))};
#else
  for (auto const *envvar : {"TMPDIR", "TMP", "TEMP", "TEMPDIR"}) {
    auto *path = getenv(envvar);
    if (path) return {path};
  }

  return {"/tmp"};
#endif
}

path current_path(std::error_code &ec) noexcept {
  ec.clear();

  auto res = impl::getcwd();
  if (res) return res.value();

  ec = res.error();

  return {};
}

path current_path() {
  std::error_code ec;

  auto p = current_path(ec);
  if (ec) throw std::system_error(ec);

  return p;
}

bool remove(const path &p, std::error_code &ec) noexcept {
  ec.clear();

  auto res = impl::unlink(p.c_str());

  if (!res && res.error() == make_error_condition(std::errc::is_a_directory)) {
    res = impl::rmdir(p.c_str());
  }

  if (res) return true;

  ec = res.error();

  return false;
}

}  // namespace filesystem
}  // namespace stdx
