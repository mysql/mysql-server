/*
 * Copyright (c) 2015, 2026, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_NGS_MEMORY_H_
#define PLUGIN_X_SRC_NGS_MEMORY_H_

#include <mysql/plugin.h>

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "my_compiler.h"  // NOLINT(build/include_subdir)
#include "plugin/x/src/config/config.h"
#include "plugin/x/src/xpl_performance_schema.h"

namespace ngs {

namespace detail {
// PSI instrumented allocator class that can be used with STL objects
template <class T>
class PFS_allocator {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using is_always_equal = std::true_type;

  constexpr PFS_allocator() noexcept = default;

  template <class U>
  constexpr PFS_allocator(PFS_allocator<U> const &) noexcept {}  // NOLINT

  template <class U>
  struct rebind {
    using other = PFS_allocator<U>;
  };

  [[nodiscard]] T *allocate(size_type n) {
    if (n > std::numeric_limits<size_type>::max() / sizeof(T)) [[unlikely]] {
      throw std::bad_array_new_length();
    }

    auto *ptr = static_cast<T *>(my_malloc(
        IS_PSI_AVAILABLE(KEY_memory_x_objects, 0), sizeof(T) * n, MYF(MY_WME)));
    if (ptr == nullptr) [[unlikely]] {
      throw std::bad_alloc();
    }

    return ptr;
  }

  void deallocate(T *ptr, size_type) noexcept { my_free(ptr); }
};

template <class T, class U>
constexpr bool operator==(const PFS_allocator<T> & /*lhs*/,
                          const PFS_allocator<U> & /*rhs*/) noexcept {
  return true;
}
}  // namespace detail

// instrumented deallocator
template <class T>
void free_object(T *ptr) {
  if (ptr != nullptr) {
    ptr->~T();
    my_free(ptr);
  }
}

// set of instrumented object allocators for different parameters number
template <typename T, typename... Args>
T *allocate_object(Args &&...args) {
  return new (my_malloc(IS_PSI_AVAILABLE(KEY_memory_x_objects, 0), sizeof(T),
                        MYF(MY_WME))) T(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
std::shared_ptr<T> allocate_shared(Args &&...args) {
  return std::allocate_shared<T>(detail::PFS_allocator<T>(),
                                 std::forward<Args>(args)...);
}

// allocates array of selected type using mysql server instrumentation
template <typename ArrayType>
void allocate_array(ArrayType *&array_ptr, std::size_t size,
                    unsigned int psf_key) {
  array_ptr = reinterpret_cast<ArrayType *>(
      my_malloc(psf_key, sizeof(ArrayType) * size, 0));
}

// reallocates array of selected type using mysql server instrumentation
// does simple allocate if null pointer passed
template <typename ArrayType>
void reallocate_array(ArrayType *&array_ptr, std::size_t size,
                      unsigned int psf_key) {
  if (NULL == array_ptr) {
    allocate_array(array_ptr, size, psf_key);
    return;
  }

  array_ptr = reinterpret_cast<ArrayType *>(
      my_realloc(psf_key, array_ptr, sizeof(ArrayType) * size, 0));
}

// frees array of selected type using mysql server instrumentation
template <typename ArrayType>
void free_array(ArrayType *array_ptr) {
  my_free(array_ptr);
}

// wrapper for ngs unique ptr with instrumented default deallocator
template <typename Type>
struct Memory_instrumented {
  struct Unary_delete {
    void operator()(Type *ptr) { free_object(ptr); }
  };

  typedef std::unique_ptr<Type, Unary_delete> Unique_ptr;
};

// PSF instrumented string
typedef std::basic_string<char, std::char_traits<char>,
                          detail::PFS_allocator<char>>
    PFS_string;

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_MEMORY_H_
