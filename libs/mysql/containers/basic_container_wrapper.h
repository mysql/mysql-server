// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_CONTAINERS_BASIC_CONTAINER_WRAPPER_H
#define MYSQL_CONTAINERS_BASIC_CONTAINER_WRAPPER_H

/// @file
/// Experimental API header

#include <concepts>                             // derived_from
#include <type_traits>                          // is_nothrow_move_assignable
#include <utility>                              // move
#include "mysql/meta/not_decayed.h"             // Not_decayed
#include "mysql/ranges/collection_interface.h"  // Collection_interface
#include "mysql/utils/call_and_catch.h"         // call_and_catch
#include "mysql/utils/is_same_object.h"         // is_same_object
#include "mysql/utils/return_status.h"          // Return_status

/// @addtogroup GroupLibsMysqlContainers
/// @{

namespace mysql::containers {

/// CRTP base class (mixin) to define a wrapper around a container.
///
/// This defines the `clear` and `assign` member modifiers based on the wrapped
/// container, as well as `get_memory_resource` and all the
/// `Collection_interface` members.
///
/// @tparam Self_tp The wrapper class that inherits from this class.
///
/// @tparam Wrapped_tp The wrapped class.
///
/// @tparam shall_catch If `yes`, `assign` will use
/// `mysql::utils::call_and_catch` to convert exceptions to return values.
template <class Self_tp, class Wrapped_tp,
          mysql::utils::Shall_catch shall_catch = mysql::utils::Shall_catch::no>
class Basic_container_wrapper
    : public mysql::ranges::Collection_interface<Self_tp> {
  using Self_t = Self_tp;
  using This_t = Basic_container_wrapper<Self_t, Wrapped_tp, shall_catch>;

 public:
  using Wrapped_t = Wrapped_tp;

  /// Constructor that delegates all parameters to the constructor of the
  /// wrapped class.
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Basic_container_wrapper(Args_t &&...args) noexcept(
      noexcept(Wrapped_t(std::forward<Args_t>(args)...)))
      : m_wrapped(std::forward<Args_t>(args)...) {}

  /// Assign a range defined by the two iterators to the wrapped object.
  ///
  /// This is enabled provided that the wrapped class defines an `assign` member
  /// taking two iterators.
  template <std::input_iterator First_iterator_t,
            std::sentinel_for<First_iterator_t> Sentinel_t>
    requires requires(Wrapped_t w, First_iterator_t f, Sentinel_t s) {
               w.assign(f, s);
             }
  [[nodiscard]] auto assign(const First_iterator_t &first,
                            const Sentinel_t &last)
      // this comment helps clang-format
      noexcept(shall_catch == mysql::utils::Shall_catch::yes ||
               noexcept(std::declval<Wrapped_t>().assign(first, last))) {
    return mysql::utils::conditional_call_and_catch<
        shall_catch>([&]() DEDUCED_NOEXCEPT_FUNCTION(
        m_wrapped.assign(first, last)));
  }

  /// Copy-assign the other object to the wrapped object.
  ///
  /// This is enabled provided that the subclass defines an `assign` member
  /// taking two iterators.
  template <class Other_t>
  [[nodiscard]] auto assign(const Other_t &other) noexcept(
      shall_catch == mysql::utils::Shall_catch::yes ||
      noexcept(std::declval<Self_t>().assign(other.begin(), other.end()))) {
    if (mysql::utils::is_same_object(other, *this)) {
      // Must return the same type as the call to `assign` below.
      if constexpr (std::same_as<decltype(self().assign(other.begin(),
                                                        other.end())),
                                 void>) {
        return;
      } else {
        return mysql::utils::Return_status::ok;
      }
    }
    return self().assign(other.begin(), other.end());
  }

  /// Move-assign the other object to the wrapped object.
  ///
  /// This is enabled provided that the wrapped class is
  /// nothrow-move-assignable.
  void assign(Self_t &&other) noexcept
    requires std::is_nothrow_move_assignable_v<Wrapped_t>
  {
    if (&other == &self()) return;
    m_wrapped = std::move(other).m_wrapped;
  }

  /// Clear the wrapped object.
  ///
  /// This invokes the `clear` member of the wrapped object.
  void clear() noexcept { m_wrapped.clear(); }

  /// Return the memory resource used by the wrapped object.
  ///
  /// This invokes the `get_memory_resource` member of the wrapped object, if it
  /// exists; otherwise invokes the `get_memory_resource` member of the object
  /// returned by the `get_allocator` member of the wrapped object, if both
  /// those exists. If none of these two is possible, this member cannot be
  /// used.
  [[nodiscard]] auto get_memory_resource() const noexcept {
    constexpr bool has_memory_resource =
        requires { m_wrapped.get_memory_resource(); };
    constexpr bool has_allocator =
        requires { m_wrapped.get_allocator().get_memory_resource(); };
    if constexpr (has_memory_resource) {
      return m_wrapped.get_memory_resource();
    } else if constexpr (has_allocator) {
      return m_wrapped.get_allocator().get_memory_resource();
    } else {
      static_assert(
          has_memory_resource || has_allocator,
          "Wrapped class has neither a get_allocator() member function that "
          "returns an object having a get_memory_resource() function, nor a "
          "get_memory_resource() member function of its own.");
    }
  }

  /// Return the allocator used by the wrapped object.
  ///
  /// This invokes the `get_allocator` member of the wrapped object, if it
  /// exists. Otherwise, this member cannot be used.
  [[nodiscard]] auto get_allocator() const noexcept {
    return m_wrapped.get_allocator();
  }

  /// @return iterator to the first element
  [[nodiscard]] auto begin() noexcept { return m_wrapped.begin(); }

  /// @return iterator to one-after-last element
  [[nodiscard]] auto end() noexcept { return m_wrapped.end(); }

  /// @return const iterator to the first element
  [[nodiscard]] auto begin() const noexcept { return m_wrapped.begin(); }

  /// @return const iterator to one-after-last element
  [[nodiscard]] auto end() const noexcept { return m_wrapped.end(); }

  /// @return const iterator to the first element
  [[nodiscard]] auto empty() const noexcept { return m_wrapped.empty(); }

  /// @return const iterator to one-after-last element
  [[nodiscard]] auto size() const noexcept { return m_wrapped.size(); }

 protected:
  /// @return non-const lvalue reference to the wrapped object.
  [[nodiscard]] auto &wrapped() &noexcept { return m_wrapped; }

  /// @return const lvalue reference to the wrapped object.
  [[nodiscard]] const auto &wrapped() const &noexcept { return m_wrapped; }

  /// @return rvalue reference to the wrapped object.
  [[nodiscard]] auto &&wrapped() &&noexcept { return std::move(m_wrapped); }

 private:
  /// Return a non-const reference to the subclass.
  [[nodiscard]] auto &self() noexcept { return static_cast<Self_t &>(*this); }

  /// Return a const reference to the subclass.
  [[nodiscard]] const auto &self() const noexcept {
    return static_cast<const Self_t &>(*this);
  }

  /// Wrapped object.
  Wrapped_t m_wrapped;
};  // class Basic_container_wrapper

}  // namespace mysql::containers

// addtogroup GroupLibsMysqlContainers
/// @}

#endif  // ifndef MYSQL_CONTAINERS_BASIC_CONTAINER_WRAPPER_H
