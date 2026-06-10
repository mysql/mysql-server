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

#ifndef MYSQL_SETS_BASIC_SET_CONTAINER_WRAPPER_H
#define MYSQL_SETS_BASIC_SET_CONTAINER_WRAPPER_H

/// @file
/// Experimental API header

#include <type_traits>                                 // remove_cvref_t
#include <utility>                                     // move
#include "mysql/containers/basic_container_wrapper.h"  // Basic_container_wrapper
#include "mysql/sets/meta.h"                           // Can_donate_set
#include "mysql/sets/set_categories_and_traits.h"      // Is_compatible_set
#include "mysql/utils/is_same_object.h"                // is_same_object

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

template <class Self_tp, class Wrapped_tp,
          mysql::utils::Shall_catch shall_catch_tp =
              mysql::utils::Shall_catch::no>
class Basic_set_container_wrapper
    : public mysql::containers::Basic_container_wrapper<Self_tp, Wrapped_tp,
                                                        shall_catch_tp> {
  using Base_t = mysql::containers::Basic_container_wrapper<Self_tp, Wrapped_tp,
                                                            shall_catch_tp>;
  using This_t =
      Basic_set_container_wrapper<Self_tp, Wrapped_tp, shall_catch_tp>;

 public:
  template <class... Args_t>
  explicit Basic_set_container_wrapper(Args_t &&...args)
      : Base_t(std::forward<Args_t>(args)...) {}

  using Base_t::assign;

  /// Enable move-assign from any Basic_set_container_wrapper for a compatible
  /// set type (not necessarily for a derived class).
  template <class Source_t>
    requires Can_donate_set<decltype(std::declval<Source_t &&>().wrapped()),
                            Wrapped_tp>
  // The requires clause ensures that Source_t is an rvalue reference.
  // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
  void assign(Source_t &&source) {
    if (mysql::utils::is_same_object(source, *this)) return;
    // The requires clause ensures that Source_t is an rvalue reference.
    // NOLINTNEXTLINE(bugprone-move-forwarding-reference)
    this->wrapped() = std::move(source).wrapped();
  }
};

namespace detail {

template <class Source_t, class Target_t>
concept Shall_enable_donate_set_for_wrapper =
    Is_compatible_set<Source_t, Target_t> &&
    ((
         // If Source wraps a compatible set, recurse into Source
         Is_compatible_set<typename Source_t::Wrapped_t, Target_t> &&
         Can_donate_set_unqualified<typename Source_t::Wrapped_t, Target_t>) ||
     (
         // If Target wraps a compatible set, recurse into Target
         Is_compatible_set<Source_t, typename Target_t::Wrapped_t> &&
         Can_donate_set_unqualified<Source_t, typename Target_t::Wrapped_t>) ||
     // Recurse into both, regardless if they are compatible with the wrappers.
     Can_donate_set_unqualified<typename Source_t::Wrapped_t,
                                typename Target_t::Wrapped_t>);

}  // namespace detail

/// Declare that move-semantics is supported for full-set-copy operations on Set
/// Container Wrapper types, whenever the wrapped types can be nothrow-moved.
template <class Source_t, class Target_t>
  requires detail::Shall_enable_donate_set_for_wrapper<Source_t, Target_t>
struct Enable_donate_set<Source_t, Target_t> : public std::true_type {};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BASIC_SET_CONTAINER_WRAPPER_H
