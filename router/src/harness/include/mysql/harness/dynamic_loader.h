/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_DYNAMIC_LOADER_INCLUDED
#define MYSQL_HARNESS_DYNAMIC_LOADER_INCLUDED

#include <string>
#include <system_error>

#ifdef _WIN32
#include <Windows.h>  // HMODULE
#endif

#include "harness_export.h"
#include "mysql/harness/stdx/expected.h"

namespace mysql_harness {
/**
 * error-codes of the DynamicLoader and DynamicLibrary.
 *
 * when set, the error-msg needs be retrieved from DynamicLoader().error_msg()
 * or DynamicLibrary().error_msg()
 */
enum class DynamicLoaderErrc {
  kDlError = 1,
};
}  // namespace mysql_harness

namespace std {
template <>
struct is_error_code_enum<mysql_harness::DynamicLoaderErrc> : std::true_type {};
}  // namespace std

namespace mysql_harness {

/**
 * make error_code from a DynamicLoaderErrc.
 *
 * ~~~{.cpp}
 * throw std::system_error(make_error_code(DynamicLoaderErrc::kDlError))
 * ~~~
 */
std::error_code make_error_code(DynamicLoaderErrc ec);

// forward decl for the friend relationship
class DynamicLoader;

/**
 * A DynamicLibrary.
 */
class HARNESS_EXPORT DynamicLibrary {
 public:
#ifdef _WIN32
  using native_handle_type = HMODULE;
#else
  using native_handle_type = void *;
#endif
  // construct a DynamicLibrary that refers to the main executable.
  DynamicLibrary() = default;

  // disable copy constructor.
  DynamicLibrary(const DynamicLibrary &) = delete;

  // move constructor.
  DynamicLibrary(DynamicLibrary &&rhs)
      : filename_{std::move(rhs.filename_)},
        handle_{std::exchange(rhs.handle_, nullptr)},
        error_msg_{std::move(rhs.error_msg_)} {}

  // disable copy assignment.
  DynamicLibrary &operator=(const DynamicLibrary &) = delete;

  // move assignment.
  DynamicLibrary &operator=(DynamicLibrary &&rhs) {
    filename_ = std::move(rhs.filename_);
    handle_ = std::exchange(rhs.handle_, nullptr);
    error_msg_ = std::move(rhs.error_msg_);
    return *this;
  }

  /**
   * destruct a DynamicLibrary.
   *
   * unloads dynamic library if it is loaded
   */
  ~DynamicLibrary() { unload(); }

  /**
   * unload a DynamicLibary if it is loaded.
   */
  void unload();

  /**
   * get the native handle to the shared object.
   */
  native_handle_type native_handle() const { return handle_; }

  /**
   * get a symbol from the dynamic library.
   */
  stdx::expected<void *, std::error_code> symbol(const std::string &name) const;

  /**
   * get error message if symbol() failed with DynamicLoaderErrc::DlError.
   */
  std::string error_msg() const { return error_msg_; }

  /**
   * get filename of the loaded module.
   */
  std::string filename() const { return filename_; }

  friend class DynamicLoader;

 private:
  /**
   * construct DynamicLibrary from native_handle.
   *
   * @param filename filename on the loaded library
   * @param handle handle to the loaded library
   */
  DynamicLibrary(std::string filename, native_handle_type handle)
      : filename_{std::move(filename)}, handle_{handle} {}

  std::string filename_;

  native_handle_type handle_{};

  mutable std::string error_msg_;
};

/**
 * Loader for DynamicLibrary.
 */
class HARNESS_EXPORT DynamicLoader {
 public:
  DynamicLoader() : search_path_{} {}

  /**
   * construct DynamicLoader with search_path.
   */
  explicit DynamicLoader(std::string search_path)
      : search_path_{std::move(search_path)} {}

  /**
   * load a shared library from path.
   *
   * @param name library name without suffix
   * @return DynamicLibrary on success, std::error_code on failure
   */
  stdx::expected<DynamicLibrary, std::error_code> load(
      const std::string &name) const;

  /**
   * get error message if load() failed with DynamicLoaderErrc::DlError.
   */
  std::string error_msg() const { return error_msg_; }

  /**
   * get current search path.
   */
  std::string search_path() const { return search_path_; }

 private:
  std::string search_path_;

  mutable std::string error_msg_;
};

}  // namespace mysql_harness

#endif
