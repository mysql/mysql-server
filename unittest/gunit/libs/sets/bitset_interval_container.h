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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef UNITTEST_LIBS_SETS_BITSET_INTERVAL_CONTAINER_H
#define UNITTEST_LIBS_SETS_BITSET_INTERVAL_CONTAINER_H

#include <gtest/gtest.h>                // TEST
#include <bit>                          // countr_zero
#include <cassert>                      // assert
#include <iterator>                     // forward_iterator
#include "bitset_boundary_container.h"  // Bitset_boundary_container
#include "mysql/meta/not_decayed.h"     // Not_decayed
#include "mysql/sets/sets.h"            // Is_interval_container

namespace unittest::libs::sets {

/// Interval container wrapped around a Bitset_boundary_container.
template <Bitset_value max_exclusive_t>
class Bitset_interval_container_impl
    : public mysql::sets::Interval_container<
          mysql::sets::Bitset_boundary_container<max_exclusive_t>> {
 public:
  static constexpr auto max_exclusive = max_exclusive_t;

 private:
  using Base_t = mysql::sets::Interval_container<
      mysql::sets::Bitset_boundary_container<max_exclusive>>;

 public:
  using typename Base_t::Set_traits_t;

  /// Use defaults for default constructor and rule-of-5 members.
  Bitset_interval_container_impl() = default;
  Bitset_interval_container_impl(const Bitset_interval_container_impl &other) =
      default;
  Bitset_interval_container_impl(Bitset_interval_container_impl &&other) =
      default;
  Bitset_interval_container_impl &operator=(
      const Bitset_interval_container_impl &other) = default;
  Bitset_interval_container_impl &operator=(
      Bitset_interval_container_impl &&other) = default;
  ~Bitset_interval_container_impl() = default;

  /// Construct a new container from the given bitmap.
  explicit Bitset_interval_container_impl(Bitset_storage bits) {
    set_bits(bits);
  }

  /// Construct a new container by copying source.
  explicit Bitset_interval_container_impl(
      const mysql::sets::Is_interval_set_over_traits<Set_traits_t> auto &source)
      : Base_t(source) {}

  /// Replace this container by the given bits.
  void set_bits(Bitset_storage bits) { this->boundaries().set_bits(bits); }

  /// Return true if value is in the set.
  [[nodiscard]] bool contains_element(const Bitset_value &value) const {
    return this->boundaries().contains_element(value);
  }

  /// Return the total length of all intervals.
  [[nodiscard]] auto volume() const { return this->boundaries().volume(); }

  /// Replace this set by its complement.
  void inplace_complement() { this->boundaries().inplace_complement(); }
};  // class Bitset_interval_container_impl

/// Verify that the types satisfy the intended requirements.
static_assert(std::same_as<mysql::ranges::Range_iterator_type<
                               mysql::sets::Bitset_boundary_container<61>>,
                           Bitset_boundary_iterator<61>>);
static_assert(std::same_as<
              mysql::ranges::Iterator_value_type<Bitset_boundary_iterator<37>>,
              Bitset_value>);
static_assert(std::same_as<mysql::ranges::Range_value_type<
                               mysql::sets::Bitset_boundary_container<53>>,
                           Bitset_value>);
static_assert(
    mysql::sets::Is_boundary_set<mysql::sets::Bitset_boundary_container<59>>);

}  // namespace unittest::libs::sets

// ==== Import Bitset_interval_container into mysql::sets ====
//
// The class is for testing, so we define all the behavior in
// unittest::libs::sets. However, all set types need to exist in mysql::sets, in
// order to be accessible to argument-dependent lookup in free functions such as
// mysql::sets::is_superset. Therefore, we tag the implementation class with the
// _impl suffix (to not confuse ourselves), and import it into mysql::sets with
// this declaration, through which we access it in all the tests.
namespace mysql::sets {

template <unittest::libs::sets::Bitset_value max_exclusive_tp>
class Bitset_interval_container
    : public unittest::libs::sets::Bitset_interval_container_impl<
          max_exclusive_tp> {
  using This_t = Bitset_interval_container<max_exclusive_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Bitset_interval_container(Args_t &&...args)
      : unittest::libs::sets::Bitset_interval_container_impl<max_exclusive_tp>(
            std::forward<Args_t>(args)...) {}
};

}  // namespace mysql::sets

#endif  // ifndef UNITTEST_LIBS_SETS_BITSET_INTERVAL_CONTAINER_H
