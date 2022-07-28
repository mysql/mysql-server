/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include "library_file.h"

#include "my_config.h"

#include <stdexcept>

#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#else
#include <Windows.h>
#include "mysql/harness/filesystem.h"
#endif

#define USE_DLCLOSE 1

// disable dlclose() when built with lsan
//
// clang has __has_feature(address_sanitizer)
// gcc has __SANITIZE_ADDRESS__
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#undef USE_DLCLOSE
#define USE_DLCLOSE 0
#endif
#endif

#if defined(__SANITIZE_ADDRESS__) && __SANITIZE_ADDRESS__ == 1
#undef USE_DLCLOSE
#define USE_DLCLOSE 0
#endif

// dlopen/dlclose work differently on Alpine
#if defined(LINUX_ALPINE)
#undef USE_DLCLOSE
#define USE_DLCLOSE 0
#endif

struct Library_file::Library_file_impl {
#ifndef _WIN32
  void *handle;
#else
  HMODULE handle;
#endif
};

#ifdef _WIN32

namespace {
inline std::error_code last_win32_error_code() {
  return {static_cast<int>(GetLastError()), std::system_category()};
}
}  // namespace
#endif

Library_file::Library_file(const std::string &file_name,
                           const std::string &plugin_name)
    : impl_(new Library_file_impl()),
      plugin_name_(plugin_name),
      file_name_(file_name) {
#ifndef _WIN32
  impl_->handle = dlopen(file_name.c_str(), RTLD_LOCAL | RTLD_LAZY);
  if (impl_->handle == nullptr) {
    throw std::runtime_error("Could not load plugin file '" + file_name +
                             "': " + dlerror());
  }
#else
  mysql_harness::Path lib_file(file_name);
  // we need to do this so all the dlls that plugin library needs could be found
  auto res = SetCurrentDirectory(lib_file.dirname().c_str());
  if (!res) {
    const auto ec = last_win32_error_code();
    throw std::system_error(
        ec, "Could not switch directory to " + lib_file.dirname().str());
  }
  impl_->handle = LoadLibrary(lib_file.real_path().c_str());
  if (impl_->handle == nullptr) {
    const auto ec = last_win32_error_code();
    throw std::system_error(ec,
                            "Could not load plugin file '" + file_name + "'");
  }
#endif
}

uint32_t Library_file::get_abi_version() const {
  Plugin_abi *plugin = get_plugin_struct<Plugin_abi>(plugin_name_);

  return plugin->abi_version;
}

template <class T>
T *Library_file::get_plugin_struct(const std::string &symbol) const {
  // In the older MySQLRouter releases some plugins did not use harness_plugin_
  // prefix for the plugin structure name. So we check harness_plugin_xxx and
  // then xxx if the first check failed.

  T *result{nullptr};
  try {
    result = get_plugin_struct_internal<T>("harness_plugin_" + symbol);
  } catch (const std::runtime_error &) {
    result = get_plugin_struct_internal<T>(plugin_name_);
  }

  return result;
}

template <class T>
T *Library_file::get_plugin_struct_internal(const std::string &symbol) const {
  T *result{nullptr};

#ifndef _WIN32
  result = reinterpret_cast<T *>(dlsym(impl_->handle, symbol.c_str()));
  const char *error = dlerror();
  if (error) {
    throw std::runtime_error("Loading plugin information for '" + file_name_ +
                             "' failed: " + error);
  }
#else
  SetLastError(0);
  result = reinterpret_cast<T *>(GetProcAddress(impl_->handle, symbol.c_str()));
  const auto ec = last_win32_error_code();
  if (ec) {
    throw std::system_error(
        ec, "Loading plugin information for '" + file_name_ + "' failed");
  }
#endif

  return result;
}

template Plugin_abi *Library_file::get_plugin_struct<Plugin_abi>(
    const std::string &) const;
template Plugin_v1 *Library_file::get_plugin_struct<Plugin_v1>(
    const std::string &) const;
template Plugin_abi *Library_file::get_plugin_struct_internal<Plugin_abi>(
    const std::string &) const;
template Plugin_v1 *Library_file::get_plugin_struct_internal<Plugin_v1>(
    const std::string &) const;

Library_file::~Library_file() {
// disable dlclose() if run with address sanitizer to get good memleak reports
#if USE_DLCLOSE
#ifndef _WIN32
  if (impl_->handle) dlclose(impl_->handle);
#else
  if (impl_->handle) {
    FreeLibrary(impl_->handle);
  }
#endif
#endif
}
