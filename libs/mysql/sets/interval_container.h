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

#ifndef MYSQL_SETS_INTERVAL_CONTAINER_H
#define MYSQL_SETS_INTERVAL_CONTAINER_H

/// @file
/// Experimental API header

#include "mysql/allocators/memory_resource.h"   // Memory_resource
#include "mysql/sets/boundary_set_meta.h"       // Is_boundary_set
#include "mysql/sets/interval.h"                // Interval
#include "mysql/sets/interval_set_interface.h"  // Interval_set_interface
#include "mysql/sets/interval_set_meta.h"       // Is_interval_set_over_traits
#include "mysql/utils/is_same_object.h"         // is_same_object

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Container for intervals.
///
/// @tparam Boundary_container_tp Boundary container, i.e., a Boundary
/// set capable of storing the data, and computing inplace operations.
template <Is_boundary_container Boundary_container_tp>
class Interval_container
    : public Interval_set_interface<Interval_container<Boundary_container_tp>,
                                    Boundary_container_tp> {
 public:
  using Boundary_container_t = Boundary_container_tp;
  using This_t = Interval_container<Boundary_container_t>;

 private:
  using Base_t = Interval_set_interface<This_t, Boundary_container_t>;

 public:
  using Memory_resource_t = mysql::allocators::Memory_resource;
  using Allocator_t = typename Boundary_container_t::Allocator_t;
  using typename Base_t::Const_iterator_t;
  using typename Base_t::Element_t;
  using typename Base_t::Iterator_t;
  using typename Base_t::Set_traits_t;
  using Boundary_iterator_t = typename Boundary_container_t::Iterator_t;
  using Interval_t = Interval<Set_traits_t>;
  using Storage_t = Storage_or_void<Boundary_container_t>;

 private:
  // True if 'insert' does not throw. We assume that this implies that all
  // modifications are non-throwing.
  static constexpr bool noexcept_insertions = noexcept(
      std::declval<Boundary_container_t>().insert(std::declval<Element_t>()));

 public:
  static constexpr bool has_fast_insertion =
      Boundary_container_t::has_fast_insertion;

  /// Default constructor.
  Interval_container() noexcept = default;

  /// Copy constructor.
  ///
  /// This copy constructor is only enabled when the underlying boundary
  /// container is throwing. For non-throwing containers, use the default
  /// constructor followed by the assign member function instead.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed,
  /// which may leave the container as a subset of @c source.
  Interval_container(const Interval_container &source) = default;

  /// Move constructor, defaulted.
  Interval_container(Interval_container &&source) noexcept = default;

  /// Copy assignment operator.
  ///
  /// This copy assignment operator is only enabled when the underlying boundary
  /// container is throwing. For non-throwing containers, use assign instead.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed,
  /// which may leave the container as a subset of @c source.
  Interval_container &operator=(const Interval_container &source) = default;

  /// Move assignment operator, defaulted.
  Interval_container &operator=(Interval_container &&source) noexcept = default;

  /// Destructor.
  ///
  /// This is defaulted, so it will just invoke the destructor for m_boundaries.
  ~Interval_container() noexcept = default;

  /// Construct an empty container using the specified Memory_resource.
  explicit Interval_container(const Memory_resource_t &memory_resource) noexcept
      : m_boundaries(memory_resource) {}

  /// Construct by copying any other Interval_set over the same boundary traits.
  ///
  /// This copy constructor is only enabled when the underlying boundary
  /// container is copy-constructible, i.e., when it is throwing. For
  /// non-throwing containers, use the default constructor followed by the
  /// assign member function instead.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed,
  /// which may leave the container as a subset of @c source.
  explicit Interval_container(
      const Is_interval_set_over_traits<Set_traits_t> auto &source)
      : m_boundaries(
            source.boundaries(),
            mysql::allocators::get_memory_resource_or_default(source)) {}

  /// Construct by copying any other Interval_set over the same boundary traits.
  ///
  /// This copy constructor is only enabled when the underlying boundary
  /// container is copy-constructible, i.e., when it is throwing. For
  /// non-throwing containers, use the default constructor followed by the
  /// assign member function instead.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed,
  /// which may leave the container as a subset of @c source.
  explicit Interval_container(
      const Is_interval_set_over_traits<Set_traits_t> auto &source,
      const Memory_resource_t &memory_resource)
      : m_boundaries(source.boundaries(), memory_resource) {}

  /// Assign from any other Boundary_set over the same Set traits.
  ///
  /// This copy assignment operator is only enabled when the underlying boundary
  /// container is copy-constructible, i.e., when it is throwing. For
  /// non-throwing containers, use assign instead.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed,
  /// which may leave the container as a subset of @c source.
  Interval_container &operator=(
      const Is_interval_set_over_traits<Set_traits_t> auto &source) {
    boundaries() = source.boundaries();
    return *this;
  }

  /// Overwrite this object with @c source (copy-assigned).
  ///
  /// This is equivalent to the copy assignment operator, and provided only in
  /// order for uniformity of this API and the non-throwing API.
  ///
  /// If the boundary underlying set cannot fail (e.g. the argument is an rvalue
  /// reference to a type satisfying Can_donate_set), this returns void and
  /// does not throw. Otherwise, out-of-memory conditions may be possible. Then,
  /// if an out-of-memory condition occurred while inserting the interval, this
  /// returns error or throws bad_alloc according to the underlying boundary
  /// container. If the out-of-memory condition occurs when the operation is
  /// half-completed, it may leave the container as a subset of @c source.
  template <Is_interval_set_over_traits_unqualified<Set_traits_t> Source_t>
  [[nodiscard]] auto assign(Source_t &&source)
#ifdef IN_DOXYGEN
      ;  // doxygen can't parse the macro below
#else
      DEDUCED_NOEXCEPT_FUNCTION(this->boundaries().assign(
          std::forward<Source_t>(source).boundaries()))
#endif

  /// Return the Memory_resource object that manages memory in this object,
  /// or a default-constructed Memory_resource if there is none.
  [[nodiscard]] decltype(auto) get_memory_resource() const noexcept {
    return mysql::allocators::get_memory_resource_or_default(boundaries());
  }

  /// Make this container empty.
  ///
  /// This function cannot fail.
  void clear() noexcept { boundaries().clear(); }

  /// Return an lvalue reference to the underlying boundary container. The
  /// caller may modify the container through that reference, which will affect
  /// this interval container.
  [[nodiscard]] auto &boundaries() &noexcept { return m_boundaries; }

  /// Return a const lvalue reference to the underlying boundary container.
  [[nodiscard]] const auto &boundaries() const &noexcept {
    return m_boundaries;
  }

  /// Return an rvalue reference to the underlying boundary container. The
  /// caller may modify the container through that reference, which will affect
  /// this interval container.
  [[nodiscard]] auto &&boundaries() &&noexcept {
    return std::move(m_boundaries);
  }

  /// Insert the given element (inplace union).
  ///
  /// This may merge adjacent intervals.
  ///
  /// @param element The element to insert.
  ///
  /// If an out-of-memory condition occurred while inserting the interval, this
  /// returns error or throws bad_alloc according to the underlying boundary
  /// container. This leaves the container unmodified.
  ///
  /// We use [[nodiscard]] rather than [[nodiscard]], because nodiscard gives
  /// a warning even if auto resolves to void, whereas nodiscard does not.
  [[nodiscard]] auto insert(const Element_t &element)
#ifdef IN_DOXYGEN
      ;  // doxygen can't parse the macro below
#else
      DEDUCED_NOEXCEPT_FUNCTION(boundaries().insert(element))
#endif

  /// Remove the given element (inplace subtraction).
  ///
  /// This may truncate and/or split an interval that overlaps with the
  /// subtracted interval.
  ///
  /// @param element The element to remove.
  ///
  /// If an out-of-memory condition occurred while splitting an interval, this
  /// returns error or throws bad_alloc according to the underlying boundary
  /// container. This leaves the container unmodified.
  [[nodiscard]] auto remove(const Element_t &element)
#ifdef IN_DOXYGEN
      ;  // doxygen can't parse the macro below
#else
          DEDUCED_NOEXCEPT_FUNCTION(boundaries().remove(element))
#endif

  /// Insert the given interval (inplace union).
  ///
  /// This may merge intervals that are overlapping or adjacent with the given
  /// interval.
  ///
  /// @param interval The interval to insert.
  ///
  /// If an out-of-memory condition occurred while inserting an interval, this
  /// returns error or throws bad_alloc according to the underlying boundary
  /// container. This leaves the container unmodified.
  [[nodiscard]] auto inplace_union(const Interval_t &interval)
#ifdef IN_DOXYGEN
      ;  // doxygen can't parse the macro below
#else
          DEDUCED_NOEXCEPT_FUNCTION(boundaries().inplace_union(
              interval.start(), interval.exclusive_end()))
#endif

  /// Insert the given interval (inplace union), reading and updating the given
  /// cursor.
  ///
  /// This may merge intervals that are overlapping or adjacent with the given
  /// interval.
  ///
  /// @param[in,out] cursor Hint for the position. @see
  /// Boundary_container::inplace_union.
  ///
  /// @param interval The interval to insert.
  ///
  /// If an out-of-memory condition occurred while inserting an interval, this
  /// returns error or throws bad_alloc according to the underlying boundary
  /// container. This leaves the container unmodified.
  [[nodiscard]] auto inplace_union(Boundary_iterator_t &cursor,
                                   const Interval_t &interval)
#ifdef IN_DOXYGEN
      ;  // doxygen can't parse the macro below
#else
          DEDUCED_NOEXCEPT_FUNCTION(boundaries().inplace_union(
              cursor, interval.start(), interval.exclusive_end()))
#endif

  /// In-place insert the intervals of the given set into this container
  /// (inplace union).
  ///
  /// This may merge intervals that are overlapping or adjacent.
  ///
  /// This uses one of two algorithms, depending on the nature of the underlying
  /// storage:
  ///
  /// - If the underlying storage supports fast insertion in the middle (e.g.
  /// set or list), then it uses a true in-place algorithm, possibly adjusting
  /// existing intervals, and reusing memory as much as possible.
  ///
  /// - Otherwise (e.g. sorted vector), it uses an out-of-place algorithm that
  /// computes the result in a new container and then move-assigns the new
  /// container to the current container.
  ///
  /// The complexity depends on the underlying storage:
  ///
  /// - set: O(number_of_removed_intervals +
  ///          interval_set.size() * log(this->size()))
  ///
  /// - list: Normally O(interval_set.size() + this->size());
  ///   but O(interval_set.size()) if interval_set.front() >= this->back().
  ///
  /// - vector: Normally, O(interval_set.size() + this->size()); but
  ///   O(interval_set.size()) if interval_set.front() >= this->back().
  ///
  /// @param interval_set The interval set.
  ///
  /// If an out-of-memory condition occurred while inserting an interval, this
  /// returns error or throws bad_alloc according to the underlying boundary
  /// container. This may occur when the operation is half-completed, which
  /// may leave the container as a superset of the previous set and a subset of
  /// the union.
  template <
      Is_interval_set_over_traits_unqualified<Set_traits_t> Interval_set_t>
  [[nodiscard]] auto inplace_union(Interval_set_t &&interval_set)
#ifdef IN_DOXYGEN
      ;  // doxygen can't parse the macro below
#else
          DEDUCED_NOEXCEPT_FUNCTION(boundaries().inplace_union(
              std::forward<Interval_set_t>(interval_set).boundaries()))
#endif

  /// Subtract the given interval.
  ///
  /// This may truncate and/or split intervals that overlap partially with the
  /// subtracted interval, and remove intervals that overlap completely with the
  /// subtracted interval.
  ///
  /// @param interval The interval to remove.
  ///
  /// If an out-of-memory condition occurred while splitting an interval, this
  /// returns error or throws bad_alloc according to the underlying boundary
  /// container. This leaves the container unmodified.
  [[nodiscard]] auto inplace_subtract(const Interval_t &interval)
#ifdef IN_DOXYGEN
      ;  // doxygen can't parse the macro below
#else
          DEDUCED_NOEXCEPT_FUNCTION(boundaries().inplace_subtract(
              interval.start(), interval.exclusive_end()))
#endif

  /// Subtract the given interval, reading and updating the given cursor.
  ///
  /// This may truncate and/or split intervals that overlap partially with the
  /// subtracted interval, and remove intervals that overlap completely with the
  /// subtracted interval.
  ///
  /// @param[in,out] cursor Hint for the position. @see
  /// Boundary_container::inplace_subtract.
  ///
  /// @param interval The interval to remove.
  ///
  /// If an out-of-memory condition occurred while splitting an interval, this
  /// returns error or throws bad_alloc according to the underlying boundary
  /// container. This leaves the container unmodified.
  [[nodiscard]] auto inplace_subtract(Boundary_iterator_t &cursor,
                                      const Interval_t &interval)
#ifdef IN_DOXYGEN
      ;  // doxygen can't parse the macro below
#else
          DEDUCED_NOEXCEPT_FUNCTION(boundaries().inplace_subtract(
              interval.start(), interval.exclusive_end(), cursor))
#endif

  /// In-place subtract intervals of the given set from this container.
  ///
  /// This may truncate and/or split intervals that overlap partially with
  /// subtracted intervals, and remove intervals that overlap completely with
  /// subtracted intervals.
  ///
  /// Algorithm and complexity: @see inplace_union.
  ///
  /// @param interval_set The interval set.
  ///
  /// If an out-of-memory condition occurred while splitting an interval, this
  /// returns error or throws bad_alloc according to the underlying boundary
  /// container. This may occur when the operation is half-completed, which may
  /// leave the container as a subset of the previous set and a superset of the
  /// difference.
  template <
      Is_interval_set_over_traits_unqualified<Set_traits_t> Interval_set_t>
  [[nodiscard]] auto inplace_subtract(Interval_set_t &&interval_set)
#ifdef IN_DOXYGEN
      ;  // doxygen can't parse the macro below
#else
          DEDUCED_NOEXCEPT_FUNCTION(boundaries().inplace_subtract(
              std::forward<Interval_set_t>(interval_set).boundaries()))
#endif

  /// In-place intersect this container with the given interval.
  ///
  /// This may truncate intervals that overlap partially with the given
  /// interval, and remove intervals that are disjoint from the given interval.
  ///
  /// Algorithm and complexity: @see inplace_union.
  ///
  /// @param interval The interval to intersect with.
  ///
  /// Since this cannot increase the number of intervals, it never fails.
  void inplace_intersect(const Interval_t &interval) noexcept {
    boundaries().inplace_intersect(interval.start(), interval.exclusive_end());
  }

  /// In-place intersect this container with the given set.
  ///
  /// This may truncate intervals that overlap partially with one interval from
  /// the given set, split intervals that overlap partially with more
  /// than one interval from the given set, and remove intervals that are
  /// disjoint from the given interval set.
  ///
  /// Algorithm and complexity: @see inplace_union.
  ///
  /// @param interval_set The interval set.
  ///
  /// If an out-of-memory condition occurred while splitting an interval, this
  /// returns error or throws bad_alloc according to the underlying boundary
  /// container. This may occur when the operation is half-completed, which may
  /// leave the container as a subset of the previous set and a superset of the
  /// intersection.
  template <
      Is_interval_set_over_traits_unqualified<Set_traits_t> Interval_set_t>
  [[nodiscard]] auto inplace_intersect(Interval_set_t &&interval_set)
#ifdef IN_DOXYGEN
      ;  // doxygen can't parse the macro below
#else
      DEDUCED_NOEXCEPT_FUNCTION(
        boundaries().inplace_intersect(
          std::forward<Interval_set_t>(interval_set).boundaries()))
#endif

 private:
  Boundary_container_t m_boundaries;

};  // class Interval_container

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_CONTAINER_H
