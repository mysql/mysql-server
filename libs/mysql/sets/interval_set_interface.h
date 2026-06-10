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

#ifndef MYSQL_SETS_INTERVAL_SET_INTERFACE_H
#define MYSQL_SETS_INTERVAL_SET_INTERFACE_H

/// @file
/// Experimental API header

#include "mysql/ranges/disjoint_pairs.h"       // Disjoint_pairs_interface
#include "mysql/sets/boundary_set_meta.h"      // Is_boundary_set
#include "mysql/sets/interval_set_category.h"  // Interval_set_category_tag
#include "mysql/utils/return_status.h"         // Return_status

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Helper function object to construct intervals.
///
/// This is used as template argument for
/// mysql::ranges::Disjoint_pairs_interface.
template <Is_bounded_set_traits Set_traits_tp>
struct Make_interval {
  [[nodiscard]] static constexpr auto make_pair(
      const typename Set_traits_tp::Element_t &start,
      const typename Set_traits_tp::Element_t &exclusive_end) {
    auto interval = Interval<Set_traits_tp>();
    [[maybe_unused]] auto ret = interval.assign(start, exclusive_end);
    assert(ret == mysql::utils::Return_status::ok);
    return interval;
  }
};

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// Helper concept to identify if a class can be the implementation for
/// Interval_set_interface.
template <class Test>
concept Is_interval_set_implementation = requires(Test t, const Test ct) {
                                           {
                                             t.boundaries()
                                             } -> Is_boundary_set_ref;
                                           {
                                             ct.boundaries()
                                             } -> Is_boundary_set_ref;
                                         };

/// CRTP base class used to define an Interval set based on an implementation
/// having the member function `boundaries()`.
///
/// This is for objects that own the underlying boundary set. If you need
/// objects that do not own the underlying range - a *view* - use
/// Interval_set_view.
///
/// @tparam Self_tp Class that implements a boundaries() function that returns a
/// Boundary_set_tp.
///
/// @tparam Boundary_set_tp Type of boundary set.
template <class Self_tp, class Boundary_set_tp>
class Interval_set_interface
    : public mysql::ranges::Disjoint_pairs_interface<
          Self_tp,
          detail::Make_interval<typename Boundary_set_tp::Set_traits_t>> {
  using Self_t = Self_tp;

 public:
  using Boundary_set_t = Boundary_set_tp;
  using This_t = Interval_set_interface<Self_t, Boundary_set_t>;

  using Set_category_t = Interval_set_category_tag;
  using Set_traits_t = Boundary_set_t::Set_traits_t;
  using Element_t = typename Set_traits_t::Element_t;

  using Make_interval_t = detail::Make_interval<Set_traits_t>;
  using Boundary_iterator_t =
      mysql::ranges::Range_iterator_type<Boundary_set_tp>;
  using Boundary_const_iterator_t =
      mysql::ranges::Range_const_iterator_type<Boundary_set_tp>;
  using Iterator_t = mysql::ranges::Disjoint_pairs_iterator<Boundary_iterator_t,
                                                            Make_interval_t>;
  using Const_iterator_t =
      mysql::ranges::Disjoint_pairs_iterator<Boundary_const_iterator_t,
                                             Make_interval_t>;

  // Required by Disjoint_pairs_interface
  [[nodiscard]] const auto &disjoint_pairs_source() const {
    return self().boundaries();
  }
  [[nodiscard]] auto &disjoint_pairs_source() { return self().boundaries(); }

 private:
  [[nodiscard]] const auto &self() const {
    return static_cast<const Self_t &>(*this);
  }
  [[nodiscard]] auto &self() { return static_cast<Self_t &>(*this); }
};  // class Interval_set_interface

/// View that provides and Interval set from an underlying Boundary set.
///
/// This a view, which does not own the underlying Boundary set. If you need to
/// define a class that owns the underlying range, use Interval_set_interface.
///
/// @tparam Boundary_set_tp Underlying Boundary set.
template <Is_boundary_set Boundary_set_tp>
class Interval_set_view
    : public Interval_set_interface<Interval_set_view<Boundary_set_tp>,
                                    Boundary_set_tp>,
      public std::ranges::view_base {
  using Boundary_set_ref_t = mysql::ranges::View_source<Boundary_set_tp>;

 public:
  using Boundary_set_t = Boundary_set_tp;

  explicit Interval_set_view(const Boundary_set_t &boundaries)
      : m_boundaries(boundaries) {}

  [[nodiscard]] const auto &boundaries() const {
    return m_boundaries.reference();
  }

 private:
  const Boundary_set_ref_t m_boundaries;
};  // class Interval_set_view

template <Is_boundary_set Boundary_set_t>
[[nodiscard]] auto make_interval_set_view(const Boundary_set_t &boundary_set) {
  return Interval_set_view<Boundary_set_t>(boundary_set);
}

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_SET_INTERFACE_H
