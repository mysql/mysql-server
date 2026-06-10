// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_SETS_ALIASES_H
#define MYSQL_SETS_ALIASES_H

/// @file
/// Experimental API header

#include "mysql/meta/not_decayed.h"         // Not_decayed
#include "mysql/sets/interval_container.h"  // Interval_container
#include "mysql/sets/map_nested_storage.h"  // Map_nested_storage
#include "mysql/sets/nested_container.h"    // Nested_container
#include "mysql/sets/nested_set_traits.h"   // Nested_set_traits
#include "mysql/sets/nonthrowing_boundary_container_adaptor.h"  // Nonthrowing_boundary_container_adaptor
#include "mysql/sets/set_categories_and_traits.h"      // Is_set
#include "mysql/sets/set_traits.h"                     // Is_bounded_set_traits
#include "mysql/sets/throwing/boundary_container.h"    // Boundary_container
#include "mysql/sets/throwing/map_boundary_storage.h"  // Map_boundary_storage
#include "mysql/sets/throwing/vector_boundary_storage.h"  // Vector_boundary_storage

/// @addtogroup GroupLibsMysqlSets
/// @{

// ==== Helpers ====

namespace mysql::sets {

/// Given a class template of the same shape as std::map (four class arguments
/// for key, mapped, compare, and allocator), and an Ordered set traits class,
/// provides the specialization with parameters derived from the set traits.
template <template <class, class, class, class> class Map_template_t,
          Is_ordered_set_traits Set_traits_t,
          class Mapped_t = typename Set_traits_t::Element_t>
using Map_for_set_traits = Map_template_t<
    typename Set_traits_t::Element_t, Mapped_t, typename Set_traits_t::Less_t,
    mysql::allocators::Allocator<
        std::pair<const typename Set_traits_t::Element_t, Mapped_t>>>;

}  // namespace mysql::sets

// ==== Type aliases ====
//
// The following type aliases (using declarations) define how we compose class
// templates into commonly used set containers with backing storage based on
// standard library containers.
//
// We recommned to use the wrapper classes below instead: the type signatures of
// the aliases here contain all class templates and template arguments that they
// are composed of, which makes compilation errors and stack traces hard to
// read. The wrapper classes hide those details.

namespace mysql::sets::throwing::detail {

/// Throwing Boundary container using std::map as backing storage.
template <Is_bounded_set_traits Set_traits_t>
using Map_boundary_container_alias = Boundary_container<Map_boundary_storage<
    Set_traits_t, Map_for_set_traits<std::map, Set_traits_t>>>;

/// Throwing Boundary container using std::vector as backing storage.
template <Is_bounded_set_traits Set_traits_t>
using Vector_boundary_container_alias =
    Boundary_container<Vector_boundary_storage<Set_traits_t>>;

/// Throwing Interval container using std::map as backing storage.
template <Is_bounded_set_traits Set_traits_t>
using Map_interval_container_alias =
    Interval_container<Map_boundary_container_alias<Set_traits_t>>;

/// Throwing Interval container using std::vector as backing storage.
template <Is_bounded_set_traits Set_traits_t>
using Vector_interval_container_alias =
    Interval_container<Vector_boundary_container_alias<Set_traits_t>>;

}  // namespace mysql::sets::throwing::detail

namespace mysql::sets::detail {

/// Boundary container using std::map as backing storage.
template <Is_bounded_set_traits Set_traits_t>
using Map_boundary_container_alias = Nonthrowing_boundary_container_adaptor<
    throwing::detail::Map_boundary_container_alias<Set_traits_t>>;

/// Boundary container using std::vector as backing storage.
template <Is_bounded_set_traits Set_traits_t>
using Vector_boundary_container_alias = Nonthrowing_boundary_container_adaptor<
    throwing::detail::Vector_boundary_container_alias<Set_traits_t>>;

/// Interval container using std::map as backing storage.
template <Is_bounded_set_traits Set_traits_t>
using Map_interval_container_alias =
    Interval_container<Map_boundary_container_alias<Set_traits_t>>;

/// Interval container using std::vector as backing storage.
template <Is_bounded_set_traits Set_traits_t>
using Vector_interval_container_alias =
    Interval_container<Vector_boundary_container_alias<Set_traits_t>>;

/// Nested set container using std::map as backing storage.
template <Is_ordered_set_traits Key_traits_t, Is_set Mapped_t>
using Map_nested_container_alias = Nested_container<Map_nested_storage<
    Nested_set_traits<Key_traits_t, typename Mapped_t::Set_traits_t,
                      typename Mapped_t::Set_category_t>,
    Map_for_set_traits<std::map, Key_traits_t, Mapped_t>>>;

}  // namespace mysql::sets::detail

// ==== Wrapper classes ====
//
// The following classes define set containers with standard library containers
// as backing storage.
//
// They are defined based on the type aliases above. We recommend using these
// wrapper classes instead of the type aliases, because the type signatures of
// the aliases contain all the class templates and template arguments they are
// composed of, whereas these wrapper classes hide those details.

namespace mysql::sets::throwing {

/// Throwing Boundary container using std::map as backing storage.
template <Is_bounded_set_traits Set_traits_tp>
class Map_boundary_container
    : public detail::Map_boundary_container_alias<Set_traits_tp> {
  using Base_t = detail::Map_boundary_container_alias<Set_traits_tp>;
  using This_t = Map_boundary_container<Set_traits_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Map_boundary_container(Args_t &&...args) noexcept(
      noexcept(Base_t(std::forward<Args_t>(args)...)))
      : Base_t(std::forward<Args_t>(args)...) {}

  template <Is_boundary_set_over_traits_unqualified<Set_traits_tp> Other_t>
  auto &operator=(Other_t &&other) noexcept(
      noexcept(this->assign(std::forward<Other_t>(other)))) {
    this->assign(std::forward<Other_t>(other));
    return *this;
  }
};

/// Throwing Boundary container using std::vector as backing storage.
template <Is_bounded_set_traits Set_traits_tp>
class Vector_boundary_container
    : public detail::Vector_boundary_container_alias<Set_traits_tp> {
  using Base_t = detail::Vector_boundary_container_alias<Set_traits_tp>;
  using This_t = Vector_boundary_container<Set_traits_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Vector_boundary_container(Args_t &&...args) noexcept(
      noexcept(Base_t(std::forward<Args_t>(args)...)))
      : Base_t(std::forward<Args_t>(args)...) {}

  template <Is_boundary_set_over_traits_unqualified<Set_traits_tp> Other_t>
  auto &operator=(Other_t &&other) noexcept(
      noexcept(this->assign(std::forward<Other_t>(other)))) {
    this->assign(std::forward<Other_t>(other));
    return *this;
  }
};

/// Throwing Interval container using std::map as backing storage.
template <Is_bounded_set_traits Set_traits_tp>
class Map_interval_container
    : public detail::Map_interval_container_alias<Set_traits_tp> {
  using Base_t = detail::Map_interval_container_alias<Set_traits_tp>;
  using This_t = Map_interval_container<Set_traits_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Map_interval_container(Args_t &&...args) noexcept(
      noexcept(Base_t(std::forward<Args_t>(args)...)))
      : Base_t(std::forward<Args_t>(args)...) {}

  template <Is_interval_set_over_traits_unqualified<Set_traits_tp> Other_t>
  auto &operator=(Other_t &&other) noexcept(
      noexcept(this->assign(std::forward<Other_t>(other)))) {
    this->assign(std::forward<Other_t>(other));
    return *this;
  }
};

/// Throwing Interval container using std::vector as backing storage.
template <Is_bounded_set_traits Set_traits_tp>
class Vector_interval_container
    : public detail::Vector_interval_container_alias<Set_traits_tp> {
  using Base_t = detail::Vector_interval_container_alias<Set_traits_tp>;
  using This_t = Vector_interval_container<Set_traits_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Vector_interval_container(Args_t &&...args) noexcept(
      noexcept(Base_t(std::forward<Args_t>(args)...)))
      : Base_t(std::forward<Args_t>(args)...) {}

  template <Is_interval_set_over_traits_unqualified<Set_traits_tp> Other_t>
  auto &operator=(Other_t &&other) noexcept(
      noexcept(this->assign(std::forward<Other_t>(other)))) {
    this->assign(std::forward<Other_t>(other));
    return *this;
  }
};

}  // namespace mysql::sets::throwing

namespace mysql::sets {

/// Boundary container using std::map as backing storage.
template <Is_bounded_set_traits Set_traits_tp>
class Map_boundary_container
    : public detail::Map_boundary_container_alias<Set_traits_tp> {
  using Base_t = detail::Map_boundary_container_alias<Set_traits_tp>;
  using This_t = Map_boundary_container<Set_traits_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Map_boundary_container(Args_t &&...args) noexcept
      : Base_t(std::forward<Args_t>(args)...) {}
};

/// Boundary container using std::vector as backing storage.
template <Is_bounded_set_traits Set_traits_tp>
class Vector_boundary_container
    : public detail::Vector_boundary_container_alias<Set_traits_tp> {
  using Base_t = detail::Vector_boundary_container_alias<Set_traits_tp>;
  using This_t = Vector_boundary_container<Set_traits_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Vector_boundary_container(Args_t &&...args) noexcept
      : Base_t(std::forward<Args_t>(args)...) {}
};

/// Interval container using std::map as backing storage.
template <Is_bounded_set_traits Set_traits_tp>
class Map_interval_container
    : public detail::Map_interval_container_alias<Set_traits_tp> {
  using Base_t = detail::Map_interval_container_alias<Set_traits_tp>;
  using This_t = Map_interval_container<Set_traits_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Map_interval_container(Args_t &&...args) noexcept
      : Base_t(std::forward<Args_t>(args)...) {}
};

/// Interval container using std::vector as backing storage.
template <Is_bounded_set_traits Set_traits_tp>
class Vector_interval_container
    : public detail::Vector_interval_container_alias<Set_traits_tp> {
  using Base_t = detail::Vector_interval_container_alias<Set_traits_tp>;
  using This_t = Vector_interval_container<Set_traits_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Vector_interval_container(Args_t &&...args) noexcept
      : Base_t(std::forward<Args_t>(args)...) {}
};

/// Nested set container using std::map as backing storage.
template <Is_ordered_set_traits Key_traits_tp, Is_set Mapped_tp>
class Map_nested_container
    : public detail::Map_nested_container_alias<Key_traits_tp, Mapped_tp> {
  using Base_t = detail::Map_nested_container_alias<Key_traits_tp, Mapped_tp>;
  using This_t = Map_nested_container<Key_traits_tp, Mapped_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Map_nested_container(Args_t &&...args) noexcept
      : Base_t(std::forward<Args_t>(args)...) {}
};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_ALIASES_H
