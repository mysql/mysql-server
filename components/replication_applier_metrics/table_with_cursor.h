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

#ifndef TABLE_WITH_CURSOR_H
#define TABLE_WITH_CURSOR_H

#include <concepts>     // std::convertible_to
#include <cstring>      // std::strlen
#include <type_traits>  // std::is_trivially_copyable_v
#include "mysql/components/services/pfs_plugin_table_service.h"  // READONLY, PSI_table_handle, PSI_field, PSI_pos, PFS_HA_ERR_END_OF_FILE, PFS_engine_table_share_proxy

/// Concept requiring that objects are copyable using memcpy, and
/// equality-testable using memcmp.
///
/// "Copyable using memcpy" is implied by std::is_trivially_copyable_v<Type>.
///
/// "Equality-testable using memcmp" cannot be checked in C++. Therefore, that
/// is a semantic requirement (see https://en.cppreference.com/w/cpp/concepts ).
/// A type that fails to be comparable using memcpy will not necessarily make
/// the concept false; only make the program behavior undefined. This semantic
/// requirement implies that the type has no internal padding bytes, which is
/// for example guaranteed by:
///
/// - arithmetic types (https://en.cppreference.com/w/cpp/types/is_arithmetic);
///
/// - arrays of padding-free objects whose sizes are powers of two;
///
/// - structs/classes which satisfy std::standard_layout_v, for which all data
/// members (1) are padding-free, and (2) have sizes that are powers of two, and
/// (3) are ordered by size, largest first.
///
/// Although the compiler cannot deduce if a class is equality-testable using
/// memcmp, you can check it manually on a given platform, by verifying
/// (recursively, for nested objets) that the sum of the sizes of all parts is
/// equal the size of the object as a whole.
///
/// tparam Type The class to test.
template <class Type>
concept Is_trivially_comparable = std::is_trivially_copyable_v<Type>
    /* && has no internal padding bytes */;

/// Concept that identifies that a class is a "table with cursor".
///
/// A table with cursor can be used to define a performance_schema table, using
/// @c get_table_share_from_table_with_cursor.
///
/// An object represent a view over a snapshot of a table, together with a row
/// cursor. The cursor should at any time be positioned either on a row, or at
/// one-past-the-last-element. The class should implement the following members:
///
/// - Default constructor: Must ensure that the object holds a snapshot of the
/// table, and must position the cursor at the first row. (Or if the table is
/// empty, position the cursor at one-past-the-last-row.)
///
/// - advance() const: Must advance the cursor to the next row, if it is not
/// already at the end.
///
/// - get_cursor() const: Must return a cursor object that represents the
/// current position. The return type must model @c Is_trivially_comparable.
///
/// - set_cursor(Position): Given an object returned from `get_cursor()`, or a
/// bytewise copy of one, this function must move the cursor to that position.
/// The parameter type must be the same as the return type for `get_cursor()`.
///
/// - is_at_end() const -> bool: Must return true if the cursor position is at
/// one-past-the-last-element, and false otherwise.
///
/// - copy_field(int from_index, PSI_field *to_field) const: Must copy the value
/// of the current row, in the column with the given index, to `to_field`.
///
/// - static get_approximate_row_count() -> int: This is optional. If given, it
/// should return an approximation of the row count. The value is only used as a
/// hint, so it may be good for performance if it is accurate, but there is no
/// strict requirement that it is consistent with the actual table size.
///
/// - static get_table_name() -> const char *: Must return the table name as a
/// string.
///
/// - static get_table_definition() -> const char *: Must return the table
/// definition as a string.
///
/// @note Tables can also be defined using the "handler interface" directly. The
/// handler interface is C-oriented and has several legacy limitations, whereas
/// "Table with cursor" is intended as a more C++-friendly alternative. It
/// overcomes the limitations that implementations of the handler interface
/// must...
///
/// - Define global functions. Table with cursor just requires user to define
/// member functions, which may be more natural as they operate on an object.
///
/// - Expose a non-const pointer to the cursor, which the user may alter
/// outside the control of the implementation. This violates OOP
/// principles. Table with cursor does not have to do that.
///
/// - Position cursors at one-before-the-beginning. This is different from
/// most iterator idioms. Table with cursor only has to support cursors
/// positioned on a row or at one-past-the-last-row.
///
/// - Implement unusual requirements on the cursor type. This could not be
/// worked around in table with cursor, but the requirement is modeled using a
/// C++20 concept, providing partial type safety.
///
/// - Initialize the table using lengthy boilerplate code. Table with cursor
/// implements that once and for all in an internal function that does not have
/// to be re-implemented per table.
///
/// - By convention, use slightly cryptic names for some functions. Table with
/// cursor attempts to use more straightforward names.
///
/// tparam Type the class to test
template <class Type>
concept Is_table_with_cursor =
    std::is_default_constructible_v<Type> &&
    requires(Type object, int index, PSI_field *field) {
      { object.advance() };
      { object.get_cursor() } -> Is_trivially_comparable;
      { object.set_cursor(object.get_cursor()) };
      { object.is_at_end() } -> std::convertible_to<bool>;
      { object.copy_field(index, field) };
      { Type::get_approximate_row_count() } -> std::convertible_to<int>;
      { Type::get_table_name() } -> std::same_as<const char *>;
      { Type::get_table_definition() } -> std::same_as<const char *>;
    };

namespace detail {

/// Adaptor class that takes a _table with cursor_ and provides an
/// implementation of MySQL's handler interface.
template <Is_table_with_cursor Table_with_cursor_tp>
class Table_handle_adaptor {
 public:
  using Table_with_cursor_t = Table_with_cursor_tp;
  using Self_t = Table_handle_adaptor<Table_with_cursor_t>;
  using Cursor_t = decltype(std::declval<Table_with_cursor_t>().get_cursor());
  static_assert(Is_trivially_comparable<Cursor_t>,
                "The return type for Table_with_cursor_t::get_cursor() must be "
                "*trivially comparable*, i.e., both satisfy "
                "std::is_trivially_copyable and "
                "be equality-comparable using memcmp (i.e., must have no "
                "internal padding "
                "bytes).");

  Table_handle_adaptor() {
    // clang-tidy suggests to initialize the cursors in initializer. But
    // we can only do it after m_table_with_cursor is initialized and this is
    // a practical way to ensure that.
    // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
    m_current_cursor = m_begin_cursor = m_table_with_cursor.get_cursor();
  }

  static PFS_engine_table_share_proxy &get_table_share() {
    static auto &table_share = init_table_share();
    return table_share;
  }

 private:
  /// @return PFS_engine_table_share_proxy initialized based on this class.
  static PFS_engine_table_share_proxy &init_table_share() {
    // Pack the static members of this class in the format expected by the
    // framework.
    static PFS_engine_table_share_proxy table_share;
    table_share.m_table_name = Table_with_cursor_t::get_table_name();
    table_share.m_table_name_length = std::strlen(table_share.m_table_name);
    table_share.m_table_definition =
        Table_with_cursor_t::get_table_definition();
    table_share.m_ref_length =
        sizeof(decltype(std::declval<Table_with_cursor_t>().get_cursor()));
    table_share.m_acl = READONLY;
    table_share.get_row_count = get_row_count;
    table_share.delete_all_rows = nullptr;  // Read-only table
    table_share.m_proxy_engine_table = {rnd_next, rnd_init, rnd_pos,
                                        // Table without index
                                        nullptr,  // index_init
                                        nullptr,  // index_read
                                        nullptr,  // index_next
                                        read_column_value,
                                        // Table without index
                                        nullptr,  // reset_position
                                        // Read-only table
                                        nullptr,  // write_column_value
                                        nullptr,  // write_row_values
                                        nullptr,  // update_column_value
                                        nullptr,  // update_row_values
                                        nullptr,  // delete_row_values
                                        open_table, close_table};
    return table_share;
  }

  /// @brief Implementation of @c PFS_engine_table_proxy::rnd_init, matching
  /// @c rnd_init_t.
  /// @param opaque_handle Pointer to Table_handle_adaptor object, cast to
  /// PSI_table_handle *.
  /// @return PFS_HA_ERR_END_OF_FILE if table is empty, 0 otherwise.
  static int rnd_init(PSI_table_handle *opaque_handle, bool /*scan*/) {
    return get_handle_adaptor(opaque_handle).do_rnd_init();
  }

  /// @brief Implementation of @c PFS_engine_table_proxy::rnd_next, matching
  /// @c rnd_next_t.
  /// @param opaque_handle Pointer to Table_handle_adaptor object, cast to
  /// PSI_table_handle *.
  /// @return PFS_HA_ERR_END_OF_FILE if it reached end of the table, 0
  /// otherwise.
  static int rnd_next(PSI_table_handle *opaque_handle) {
    return get_handle_adaptor(opaque_handle).do_rnd_next();
  }

  /// @brief Implementation of @c PFS_engine_table_proxy::rnd_pos, matching
  /// @c rnd_pos_t.
  /// @param opaque_handle Pointer to Table_handle_adaptor object, cast to
  /// PSI_table_handle *.
  /// @return PFS_HA_ERR_END_OF_FILE if the position is at the end of the table,
  /// 0 otherwise.
  static int rnd_pos(PSI_table_handle *opaque_handle) {
    return get_handle_adaptor(opaque_handle).do_rnd_pos();
  }

  /// @brief Implementation of @c PFS_engine_table_proxy::read_column_value,
  /// matching @c read_column_value_t.
  /// @param opaque_handle Pointer to Table_handle_adaptor object, cast to
  /// PSI_table_handle *.
  /// @param field PSI_field to which the value should be copied.
  /// @param index Column number in the table.
  /// @return 0.
  static int read_column_value(PSI_table_handle *opaque_handle,
                               PSI_field *field, unsigned int index) {
    assert(!get_handle_adaptor(opaque_handle).m_table_with_cursor.is_at_end());
    get_handle_adaptor(opaque_handle).do_read_column_value(field, index);
    return 0;
  }

  /// Implementation of @c PFS_engine_table_share_proxy::get_row_count, matching
  /// @c get_row_count_t.
  ///
  /// @return The row count.
  static unsigned long long get_row_count() {
    if constexpr (requires {
                    Table_with_cursor_t::get_approximate_row_count();
                  }) {
      return Table_with_cursor_t::get_approximate_row_count();
    } else {
      return 0;
    }
  }

  /// Implementation of @c PFS_engine_table_proxy::open_table, matching
  /// @c open_table_t.
  ///
  /// @param opaque_pos Pointer to pointer to opaque "position". *opaque_pos
  /// will be set to point to the m_position member of the returned object.
  /// @return Pointer to a new Table_handler_adaptor object, cast to
  /// PSI_table_handle *.
  static PSI_table_handle *open_table(PSI_pos **opaque_pos) {
    // Implementing a C interface, we are required to use a raw pointer.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    auto *handle_ptr = new Self_t;
    *opaque_pos = reinterpret_cast<PSI_pos *>(&handle_ptr->m_current_cursor);
    return reinterpret_cast<PSI_table_handle *>(handle_ptr);
  }

  /// Implementation of @c PFS_engine_table_proxy::close_table, matching
  /// @c close_table_t.
  ///
  /// @param opaque_handle Pointer to Table_handle_adaptor object, cast to
  /// PSI_table_handle *.
  static void close_table(PSI_table_handle *opaque_handle) {
    // Implementing a C interface, we are required to use raw pointers.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    delete &get_handle_adaptor(opaque_handle);
  }

  /// Member function implementing @c rnd_init.
  /// @return PFS_HA_ERR_END_OF_FILE if the table is empty, 0 otherwise.
  int do_rnd_init() {
    m_before_first_row = true;
    m_current_cursor = m_begin_cursor;
    m_table_with_cursor.set_cursor(m_begin_cursor);
    return status();
  }

  /// Member function implementing @c rnd_next.
  /// @return PFS_HA_ERR_END_OF_FILE if the new position is at the end of the
  /// table, 0 otherwise.
  int do_rnd_next() {
    if (m_before_first_row) {
      m_before_first_row = false;
    } else {
      m_table_with_cursor.advance();
      m_current_cursor = m_table_with_cursor.get_cursor();
    }
    return status();
  }

  /// Member function implementing @c rnd_pos.
  /// @return PFS_HA_ERR_END_OF_FILE if the new position is at the end of the
  /// table, 0 otherwise.
  int do_rnd_pos() {
    m_table_with_cursor.set_cursor(m_current_cursor);
    return status();
  }

  /// Member function implementing @c read_column_value.
  /// @param field Opaque pointer to the output field.
  /// @param index The column index.
  void do_read_column_value(PSI_field *field, unsigned int index) {
    m_table_with_cursor.copy_field(index, field);
  }

  /// @return PFS_HA_ERR_END_OF_FILE if the position is at the end of the
  /// table, 0 otherwise.
  int status() const {
    return m_table_with_cursor.is_at_end() ? PFS_HA_ERR_END_OF_FILE : 0;
  }

  /// Cast point to PSI_table_handle to reference to Table_handle_adaptor.
  /// @param opaque_handle PSI_table_handle that actually is a pointer to a
  /// Table_handle_adaptor.
  /// @return Reference to the Table_handle_adaptor pointed to by the parameter.
  static Self_t &get_handle_adaptor(PSI_table_handle *opaque_handle) {
    return *(Self_t *)opaque_handle;
  }

  /// True if the current position is "one-before-the-first-row".
  ///
  /// The caller expects that rnd_init followed by rnd_next positions the cursor
  /// on the first row, thus it has to be initially positioned at
  /// "one-before-the-first-row". Since Table_and_row does not have that
  /// concept, we identify the position using this flag.
  bool m_before_first_row{true};

  /// Table_with_cursor object representing the table and the current cursor
  /// position.
  Table_with_cursor_t m_table_with_cursor{};

  /// Cursor to the first row. We capture this after initializing the object,
  /// and use it to implement rnd_init.
  Cursor_t m_begin_cursor;

  /// Current cursor. We set this in rnd_next and rnd_init, share the pointer to
  /// it with the Optimizer code in the output parameter for open_table, and
  /// allow Optimizer to alter it as it needs, as long as the alteration is
  /// followed by a call to rnd_pos.
  Cursor_t m_current_cursor;
};

}  // namespace detail

/// Takes a _table with cursor_ and returns a new
/// PFS_engine_table_share_proxy for the class.
template <Is_table_with_cursor Table_with_cursor>
PFS_engine_table_share_proxy *get_table_share_from_table_with_cursor() {
  return &detail::Table_handle_adaptor<Table_with_cursor>::get_table_share();
}

#endif  // ifndef TABLE_WITH_CURSOR_H
