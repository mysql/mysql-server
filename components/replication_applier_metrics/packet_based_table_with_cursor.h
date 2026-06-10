/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PACKET_BASED_TABLE_WITH_CURSOR_H
#define PACKET_BASED_TABLE_WITH_CURSOR_H

#include <atomic>                      // std::atomic
#include <cassert>                     // assert
#include <concepts>                    // std::same_as, std::derived_from
#include <type_traits>                 // std::is_enum_v
#include "mysql/abi_helpers/packet.h"  // Packet_array
#include "mysql/components/services/pfs_plugin_table_service.h"  // PSI_field
#include "mysql/meta/is_const_ref.h"                             // Is_const_ref
#include "row_proxy.h"                                           // Row_proxy

namespace detail {

/// Helper to define Is_row_view_definition. This is the "false" case,
/// matching any type that does not match the following case.
template <class Type>
struct Is_row_view_definition_helper : public std::false_type {};
/// Helper to define Is_row_view_definition. This is the "true" case, matching
/// any type that is a specialization of Row_view_definition.
template <class T1, auto T2, auto T3>
struct Is_row_view_definition_helper<Row_view_definition<T1, T2, T3>>
    : public std::true_type {};

/// True if std::remove_cvref_t<Type> is a speciaization of Row_view_definition.
template <class Type>
concept Is_row_view_definition =
    Is_row_view_definition_helper<std::remove_cvref_t<Type>>::value;

/// True if Type is a const reference to a specialization of
/// Row_view_definition.
template <class Type>
concept Is_row_view_definition_const_ref =
    mysql::meta::Is_const_ref<Type> && Is_row_view_definition<Type>;

/// The return type for the non-static member function
/// Type::get_row_view_definition.
template <class Type>
using Return_type_for_get_row_view_definition = std::remove_cvref_t<
    decltype(std::declval<Type>().get_row_view_definition())>;

/// Row_proxy_type_info object based on the types found in the given
/// Row_view_definition.
template <class Type>
using Type_info_for_row_view_definition =
    Row_proxy_type_info<typename Type::value_type::Typecode_t,
                        Type::value_type::typecode_end,
                        std::tuple_size<Type>{}>;

/// Row_proxy_type_info object based on the types found in the
/// Row_view_definition returned from Type::get_row_view_definition().
template <class Type>
using Type_info_for_get_row_view_definition = Type_info_for_row_view_definition<
    Return_type_for_get_row_view_definition<Type>>;

}  // namespace detail

/// Concept that identifies classes that correctly implement the requirements
/// for classes used to implement @c Packet_based_table_with_cursor .
///
/// In order to satisfy the concept, classes must define the following member
/// functions:
///
/// - get_table_data() -> mysql::abi_helpers::Packet_array<Typecode_tp>:
/// Will be called from the constructor and must return the table data as an
/// array of packets.
///
/// - free_table_data(mysql::abi_helpers::Packet_array<Typecode_tp>): Will
/// be called from the destructor and must release any memory that was allocated
/// in get_table.
///
/// - static get_row_view_definition() -> const Row_view_definition<...> &: must
/// return a const reference to a specialization of Row_view_definition. The
/// returned object determines how the table data will be converted to an SQL
/// table. See @c Row_proxy. The referenced object must outlive the
/// Packet_based_table_with_cursor object. Typically, the referenced object may
/// reside in static storage, for example as a static local variable in
/// get_row_view_definition.
///
/// - static get_table_name() -> const char *: must return the table name as a
/// string.
///
/// - static get_table_definition() -> const char *: must return the table
/// definition as a string.
///
/// tparam Impl_tp The implementation class to test.
template <class Impl_tp>
concept Is_packet_based_table_with_cursor_implementation =
    requires(const Impl_tp const_impl) {
      {
        const_impl.get_row_view_definition()
        } -> detail::Is_row_view_definition_const_ref;
      { const_impl.get_table_name() } -> std::same_as<const char *>;
      { const_impl.get_table_definition() } -> std::same_as<const char *>;
    } &&
    requires(
        Impl_tp impl,
        typename detail::Type_info_for_get_row_view_definition<Impl_tp>::Table_t
            table) {
      {
        impl.get_table_data()
        }
        -> std::same_as<typename detail::Type_info_for_get_row_view_definition<
            Impl_tp>::Table_t>;
      { impl.free_table_data(table) };
    };

/// @brief Class to aid implementing a _table with cursor_ ( @c
/// Is_table_with_cursor) where the table data is represented in a @c
/// mysql::abi_helpers::Packet_array object.
///
/// @tparam Impl_tp Implementation capable of returning the name, definition,
/// and data for the table.
template <Is_packet_based_table_with_cursor_implementation Impl_tp>
class Packet_based_table_with_cursor {
 public:
  /// Type of the implementation.
  using Impl_t = Impl_tp;
  /// Type of Row_view_definitions returned by the get_row_view_definition()
  /// member of Impl_t.
  using Type_info_t = detail::Type_info_for_get_row_view_definition<Impl_t>;

  Packet_based_table_with_cursor()
      : m_impl{},
        m_table(m_impl.get_table_data()),
        m_row_proxy(m_impl.get_row_view_definition()) {
    get_cached_approximate_row_count_ref().store(m_table.size(),
                                                 std::memory_order_relaxed);
  }

  ~Packet_based_table_with_cursor() { m_impl.free_table_data(m_table); }

  /// Delete copy/move semantics
  Packet_based_table_with_cursor(const Packet_based_table_with_cursor &) =
      delete;
  Packet_based_table_with_cursor(Packet_based_table_with_cursor &&) = delete;
  Packet_based_table_with_cursor &operator=(
      const Packet_based_table_with_cursor &) = delete;
  Packet_based_table_with_cursor &operator=(Packet_based_table_with_cursor &&) =
      delete;

  /// Return the table name.
  static const char *get_table_name() { return Impl_t::get_table_name(); }

  /// Return the table definition.
  static const char *get_table_definition() {
    return Impl_t::get_table_definition();
  }

  /// Move the cursor to the given row number.
  void set_cursor(int row) {
    m_cursor = row;
    m_cursor_dirty = true;
  }

  /// Return the current cursor (row number).
  int get_cursor() { return m_cursor; }

  /// Move the cursor to next row.
  void advance() {
    ++m_cursor;
    m_cursor_dirty = true;
  }

  /// @return true if the cursor is positioned at the end of the table.
  bool is_at_end() const { return m_cursor == m_table.ssize(); }

  /// Copy the value of the given field index to the given PSI_field object.
  /// @param index The column number
  /// @param field The output PSI_field.
  void copy_field(int index, PSI_field *field) const {
    if (m_cursor_dirty) {
      assert(m_cursor < m_table.ssize());
      m_row_proxy.set_row(m_table[m_cursor]);
      m_cursor_dirty = false;
    }
    m_row_proxy.copy_field(index, field);
  }

  /// Return the cached approximate row count.
  static int get_approximate_row_count() {
    return get_cached_approximate_row_count_ref().load(
        std::memory_order_relaxed);
  }

 private:
  Impl_t m_impl;

  /// Return a reference to an atomic integer that caches the row count for the
  /// last created object.
  ///
  /// (This could have been implemented as a static member variable. But that
  /// would be less convenient, since each specialization of this class template
  /// would have to provide a definition of the member variable in exactly one
  /// compilation unit. Using a function-local static variable, the variable
  /// gets automatically instantiated in each specialization of the function.)
  static std::atomic<int> &get_cached_approximate_row_count_ref() {
    static std::atomic<int> counter{0};
    return counter;
  }

  /// Table object.
  typename Type_info_t::Table_t m_table;

  /// Current cursor position.
  int m_cursor{0};

  /// Row proxy, containing a view over the current row.
  ///
  /// This is capable of translating column index to an entry in the
  /// mysql::abi_helpers::Packet object for the current row, and of copying the
  /// value in that column to a PSI_field object.
  ///
  /// Mutable, because it is updated lazily. The actual object state is
  /// represented by m_table and m_cursor.
  mutable typename Type_info_t::Row_proxy_t m_row_proxy;

  /// True if the cursor has moved away from the row that m_row_proxy refers to.
  mutable bool m_cursor_dirty{true};
};

#endif  // ifndef PACKET_BASED_TABLE_WITH_CURSOR_H
