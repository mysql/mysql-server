/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "mysql/harness/dynamic_loader.h"

#ifdef _WIN32
#include <Windows.h>  // HMODULE
#else
#include <dlfcn.h>  // dlopen
#endif

#include "mysql/harness/filesystem.h"  // Path::make_path
#include "mysql/harness/stdx/expected.h"

using namespace std::string_literals;

#ifdef _WIN32
static std::error_code last_error_code() {
  // static_cast<> for the "conversion from 'DWORD' to 'int' requires a
  // narrowing conversion"
  return {static_cast<int>(GetLastError()), std::system_category()};
#if 0
  // currently unused on unix
  return {errno, std::generic_category()};
#endif
}
#endif

const char default_library_extension[] =
#ifdef _WIN32
    "dll"
#else
    "so"
#endif
    ;

namespace mysql_harness {

const std::error_category &dynamic_loader_category() noexcept {
  class category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "dl"; }
    std::string message(int ev) const override {
      switch (static_cast<DynamicLoaderErrc>(ev)) {
        case DynamicLoaderErrc::kDlError:
          return "dlerror";
      }

      return "(unrecognized error)";
    }
  };

  static category_impl instance;
  return instance;
}

std::error_code make_error_code(DynamicLoaderErrc e) {
  return {static_cast<int>(e), dynamic_loader_category()};
}

#ifdef _WIN32
/**
 * get filename of a module.
 *
 * @note windows only.
 *
 * @param handle handle to a module
 * @returns filename of success, std::error_code on error.
 */
static stdx::expected<std::string, std::error_code> module_filename(
    DynamicLibrary::native_handle_type handle) {
  std::string fn;

  fn.resize(MAX_PATH);

  const auto sz = GetModuleFileName(handle, &fn.front(), fn.size());
  // fn.size() is buffer with \0
  // on success, returns size without trailing \0
  if (sz == 0) {
    return stdx::make_unexpected(last_error_code());
  } else if (sz == fn.size()) {
    // truncation
    return stdx::make_unexpected(
        std::error_code(ERROR_INSUFFICIENT_BUFFER, std::system_category()));
  } else {
    fn.resize(sz);
    return fn;
  }
}
#endif

stdx::expected<DynamicLibrary, std::error_code> DynamicLoader::load(
    const std::string &libname) const {
#ifdef _WIN32
  if (0 == SetDllDirectory(search_path_.c_str())) {
    return stdx::make_unexpected(last_error_code());
  }

  std::string filename = libname + "." + default_library_extension;

  const DynamicLibrary::native_handle_type handle =
      LoadLibrary(filename.c_str());
  if (handle == nullptr) {
    return stdx::make_unexpected(last_error_code());
  }

  if (auto res = module_filename(handle)) {
    // if the filename can be resolved, use it.
    filename = std::move(res.value());
  }
#else
  // reset older error in the dl-lib
  dlerror();

  const std::string filename =
      mysql_harness::Path::make_path(search_path_, libname,
                                     default_library_extension)
          .str();

  const DynamicLibrary::native_handle_type handle =
      dlopen(filename.c_str(), RTLD_LOCAL | RTLD_NOW);
  if (handle == nullptr) {
    error_msg_ = dlerror();
    return stdx::make_unexpected(make_error_code(DynamicLoaderErrc::kDlError));
  }
#endif

  return DynamicLibrary{filename, handle};
}

stdx::expected<void *, std::error_code> DynamicLibrary::symbol(
    const std::string &name) const {
#ifdef _WIN32
  auto *sym = reinterpret_cast<void *>(GetProcAddress(handle_, name.c_str()));
  if (sym == nullptr) {
    return stdx::make_unexpected(last_error_code());
  }
#else
  // as the return-value of dlsym() can be NULL even on success, the dlerror()
  // must be checked if it is non-null
  auto *sym = dlsym(handle_, name.c_str());

  auto *error = dlerror();
  if (error != nullptr) {
    error_msg_ = error;
    return stdx::make_unexpected(make_error_code(DynamicLoaderErrc::kDlError));
  }
#endif

  return {sym};
}

void DynamicLibrary::unload() {
#ifdef _WIN32
  if (handle_ != nullptr) FreeLibrary(handle_);
#else
  if (handle_ != nullptr) dlclose(handle_);
#endif
  handle_ = nullptr;
}

}  // namespace mysql_harness
