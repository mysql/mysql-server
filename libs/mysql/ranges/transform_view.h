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

#ifndef MYSQL_RANGES_TRANSFORM_VIEW_H
#define MYSQL_RANGES_TRANSFORM_VIEW_H

/// @file
/// Experimental API header

#include <algorithm>                             // move
#include <iterator>                              // input_iterator
#include <ranges>                                // range
#include "mysql/iterators/iterator_interface.h"  // Iterator_interface
#include "mysql/ranges/collection_interface.h"   // Collection_interface
#include "mysql/ranges/meta.h"                   // Range_iterator_type
#include "mysql/ranges/view_sources.h"           // View_source

/// @addtogroup GroupLibsMysqlRanges
/// @{

namespace mysql::ranges {

/// Iterator adaptor that applies a transformation on each value before
/// returning it.
///
/// @tparam Transform_tp The transform.
///
/// @tparam Source_iterator_tp The source iterator type.
template <class Transform_tp, std::input_iterator Source_iterator_tp>
class Transform_iterator
    : public mysql::iterators::Iterator_interface<
          Transform_iterator<Transform_tp, Source_iterator_tp>> {
 public:
  using Transform_t = Transform_tp;
  using Source_iterator_t = Source_iterator_tp;

  explicit Transform_iterator() = default;
  explicit Transform_iterator(const Source_iterator_t &source_iterator)
      : m_source_iterator(source_iterator) {}
  explicit Transform_iterator(Source_iterator_t &&source_iterator)
      : m_source_iterator(std::move(source_iterator)) {}

  [[nodiscard]] decltype(auto) get() const {
    return Transform_t::transform(*m_source_iterator);
  }

  void next() { ++m_source_iterator; }

  void prev()
    requires std::bidirectional_iterator<Source_iterator_t>
  {
    --m_source_iterator;
  }

  void advance(std::ptrdiff_t delta)
    requires std::random_access_iterator<Source_iterator_t>
  {
    m_source_iterator += delta;
  }

  [[nodiscard]] bool is_equal(const Transform_iterator &other) const {
    return m_source_iterator == other.m_source_iterator;
  }

  [[nodiscard]] std::ptrdiff_t distance_from(
      const Transform_iterator &other) const
    requires std::random_access_iterator<Source_iterator_t>
  {
    return m_source_iterator - other.m_source_iterator;
  }

 private:
  Source_iterator_t m_source_iterator{};
};  // class Transform_iterator

/// Factory function to create a Transform_iterator.
///
/// @tparam Transform_t The transform.
///
/// @tparam Source_iterator_t The source iterator type.
template <class Transform_t, std::input_iterator Source_iterator_t>
[[nodiscard]] auto make_transform_iterator(const Source_iterator_t &iterator) {
  return Transform_iterator<Transform_t, Source_iterator_t>(iterator);
}

/// CRTP base class / mixin used to define ranges that provide
/// Transform_iterators.
///
/// This is for objects that own the source. If you need objects that do not own
/// the source - a *view* - use Transform_view.
///
/// @tparam Self_tp Derived class. This must implement the member function
/// `transform_source()` which returns a source object whose iterators return
/// tuple-like objects.
///
/// @tparam Transform_tp The transform.
///
/// @tparam Source_tp The source type.
template <class Self_tp, class Transform_tp, std::ranges::range Source_tp>
class Transform_interface
    : public Collection_interface<
          Transform_interface<Self_tp, Transform_tp, Source_tp>> {
 public:
  using Source_t = Source_tp;
  using Transform_t = Transform_tp;
  using Iterator_t =
      Transform_iterator<Transform_tp,
                         mysql::ranges::Range_iterator_type<Source_t>>;

  [[nodiscard]] auto begin() {
    return make_transform_iterator<Transform_t>(source().begin());
  }
  [[nodiscard]] auto end() {
    return make_transform_iterator<Transform_t>(source().end());
  }
  [[nodiscard]] auto begin() const {
    return make_transform_iterator<Transform_t>(source().begin());
  }
  [[nodiscard]] auto end() const {
    return make_transform_iterator<Transform_t>(source().end());
  }
  [[nodiscard]] auto size() const
    requires requires {
               std::declval<const Self_tp>().transform_source().size();
             }
  {
    return source().size();
  }
  [[nodiscard]] auto empty() const
    requires requires {
               std::declval<const Self_tp>().transform_source().empty();
             }
  {
    return source().empty();
  }

 private:
  [[nodiscard]] const auto &source() const {
    return static_cast<const Self_tp *>(this)->transform_source();
  }
  [[nodiscard]] auto &source() {
    return static_cast<Self_tp *>(this)->transform_source();
  }
};  // class Transform_interface

/// View whose iterators provide transformed values.
///
/// @tparam Transform_tp The transform.
///
/// @tparam Source_tp The source type.
template <class Transform_tp, std::ranges::range Source_tp>
class Transform_view
    : public Transform_interface<Transform_view<Transform_tp, Source_tp>,
                                 Transform_tp, Source_tp>,
      public std::ranges::view_base {
  using Source_ref_t = View_source<Source_tp>;

 public:
  using Source_t = Source_tp;

  Transform_view() = default;
  explicit Transform_view(const Source_t &source) : m_source(source) {}

  [[nodiscard]] auto &transform_source() { return m_source.reference(); }
  [[nodiscard]] const auto &transform_source() const {
    return m_source.reference();
  }

 private:
  Source_ref_t m_source{};
};  // class Transform_view

/// Factory function to create a Transform_view.
///
/// @tparam Transform_t The transform.
///
/// @tparam Source_t The source type.
template <class Transform_t, std::ranges::range Source_t>
[[nodiscard]] auto make_transform_view(const Source_t &source) {
  return Transform_view<Transform_t, Source_t>(source);
}

}  // namespace mysql::ranges

// addtogroup GroupLibsMysqlRanges
/// @}

#endif  // ifndef MYSQL_RANGES_TRANSFORM_VIEW_H
